// This file is part of mokaGram, https://github.com/mokagram/mokagram.
// GPLv3 with the attribution term of MOKAGRAM_NOTICE.md.
#include "mokagram/music/mokagram_music_presence.h"

#include "base/random.h"
#include "base/timer.h"
#include "data/data_user.h"
#include "main/main_session.h"
#include "mokagram/misc/mokagram_misc.h"
#include "mokagram/settings/mokagram_settings.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"

#include "styles/palette.h"

#include <QtCore/QStringList>

#include <cmath>

namespace Mokagram::Music::Presence {
namespace {

constexpr auto kLease = crl::time(35000);
constexpr auto kRefresh = crl::time(4000);
constexpr auto kPublishCheck = crl::time(1000);
constexpr auto kReadInterval = crl::time(2000);
constexpr auto kLabelBytes = 150;
constexpr auto kBatchSize = 12;
const auto kResultPrefix = u"presence-v1:"_q;
const auto kWriteAck = u"presence-v1"_q;

[[nodiscard]] QByteArray Encode(const Playback &track) {
	auto label = track.title.simplified();
	if (label.isEmpty()) {
		return {};
	}
	const auto artist = track.artist.simplified();
	if (!artist.isEmpty()) {
		label += u" · "_q + artist;
	}
	auto bytes = label.toUtf8();
	if (bytes.size() > kLabelBytes) {
		auto end = kLabelBytes;
		while ((uchar(bytes.at(end)) & 0xC0) == 0x80) {
			--end;
		}
		bytes.truncate(end);
	}
	const auto encoded = bytes.toBase64(
		QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
	return track.soundcloudId
		? (QByteArray::number(track.soundcloudId) + ':' + encoded)
		: encoded;
}

struct Activity {
	Track track;
	crl::time expiresAt = 0;
};

[[nodiscard]] Activity Decode(
		const Bot::Result &result,
		crl::time sentAt,
		crl::time now) {
	const auto description = QStringView(result.description);
	const auto separator = description.indexOf(u':');
	auto ok = false;
	const auto ttl = description.left(separator < 0
		? description.size()
		: separator).toInt(&ok);
	if (!ok || ttl <= 0 || ttl > kLease || sentAt + ttl <= now
		|| result.title.toUtf8().size() > kLabelBytes) {
		return {};
	}
	const auto label = result.title.simplified();
	const auto split = label.lastIndexOf(u" · "_q);
	const auto id = (separator < 0)
		? uint64(0)
		: description.mid(separator + 1).toULongLong();
	return {
		.track = {
			.title = split < 0 ? label : label.left(split),
			.artist = split < 0 ? QString() : label.mid(split + 3),
			.soundcloudId = id,
		},
		.expiresAt = sentAt + ttl,
	};
}

struct Publisher {
	const QString writer = QString::number(base::RandomValue<uint64>(), 16);
	Fn<Playback()> playback;
	base::weak_ptr<Main::Session> session;
	QByteArray last;
	crl::time refreshAt = 0;
	int request = 0;
	bool pending = false;
	bool shared = false;
	base::Timer timer;
};

void Refresh(const std::shared_ptr<Publisher> &state) {
	auto &s = *state;
	auto session = s.session.get();
	if (!session) {
		s.pending = s.shared = false;
		s.last.clear();
	}
	if (s.pending) {
		return;
	}
	const auto current = Settings::Bool(kShareKey) ? s.playback() : Playback();
	const auto label = current.session ? Encode(current) : QByteArray();
	const auto now = crl::now();
	const auto clear = s.shared && (session != current.session || label.isEmpty());
	if (!clear) {
		if (label.isEmpty() || (label == s.last && now < s.refreshAt)) {
			return;
		}
		session = current.session;
		s.session = base::make_weak(session);
		s.shared = true;
	}
	s.pending = true;
	s.last = clear ? QByteArray() : label;
	s.refreshAt = now + kRefresh;
	const auto request = ++s.request;
	const auto complete = [weak = std::weak_ptr(state), request, clear](
			bool acknowledged) {
		if (const auto state = weak.lock(); state && state->request == request) {
			state->pending = false;
			if (clear) {
				state->shared = false;
			} else if (!Settings::Bool(kShareKey)) {
				Refresh(state);
			} else if (!acknowledged) {
				state->refreshAt = crl::now() + kPublishCheck;
			}
		}
	};
	Bot::Ask(
		session,
		clear
			? u"presence:v1:clear:"_q + s.writer
			: u"presence:v1:put:"_q + s.writer + u':' + QString::fromLatin1(label),
		[=](Bot::Results results) {
			complete(results.size() == 1 && results.front().id == kWriteAck);
		},
		[=] { complete(false); });
}

struct Viewer {
	base::Timer poll;
	base::Timer expiry;
	std::map<uint64, Activity> active;
	Tracks shown;
	Fn<void(Tracks)> changed;
	int generation = 0;
	int pending = 0;
};

void Notify(Viewer &state) {
	const auto now = crl::now();
	std::erase_if(state.active, [&](const auto &entry) {
		return entry.second.expiresAt <= now;
	});
	auto next = crl::time(0);
	auto tracks = Tracks();
	for (const auto &[id, activity] : state.active) {
		tracks.emplace(id, activity.track);
		next = next ? std::min(next, activity.expiresAt) : activity.expiresAt;
	}
	state.expiry.cancel();
	if (next) {
		state.expiry.callOnce(next - now);
	}
	if (tracks != state.shown) {
		state.shown = tracks;
		state.changed(std::move(tracks));
	}
}

} // namespace

void Publish(Fn<Playback()> playback, rpl::lifetime &lifetime) {
	const auto state = std::make_shared<Publisher>();
	state->playback = std::move(playback);
	const auto weak = std::weak_ptr(state);
	const auto refresh = [weak] {
		if (const auto state = weak.lock()) {
			Refresh(state);
		}
	};
	state->timer.setCallback(refresh);
	lifetime.add([state] {});
	Settings::BoolValue(kShareKey) | rpl::on_next([=](bool enabled) {
		if (const auto state = weak.lock()) {
			if (enabled) {
				state->timer.callEach(kPublishCheck);
			} else {
				state->timer.cancel();
			}
		}
		refresh();
	}, lifetime);
}

bool CanWatch(not_null<UserData*> user) {
	return !user->isSelf()
		&& !user->isBot()
		&& !user->isServiceUser()
		&& !user->isInaccessible()
		&& !user->isBlocked();
}

void Watch(
		not_null<UserData*> user,
		not_null<Ui::RpWidget*> view,
		Fn<void(Track)> changed,
		rpl::lifetime &lifetime) {
	if (!CanWatch(user)) {
		return;
	}
	const auto id = peerToUser(user->id).bare;
	WatchList(
		&user->session(),
		view,
		[=] {
			return CanWatch(user)
				? std::vector<uint64>{ id }
				: std::vector<uint64>();
		},
		[=](Tracks tracks) {
			const auto i = tracks.find(id);
			changed(i == tracks.end() || !CanWatch(user)
				? Track()
				: std::move(i->second));
		},
		lifetime);
}

void WatchList(
		not_null<Main::Session*> session,
		not_null<Ui::RpWidget*> view,
		Fn<std::vector<uint64>()> visible,
		Fn<void(Tracks)> changed,
		rpl::lifetime &lifetime) {
	const auto state = std::make_shared<Viewer>();
	state->changed = std::move(changed);
	const auto weak = std::weak_ptr(state);
	state->expiry.setCallback([weak] {
		if (const auto state = weak.lock()) {
			Notify(*state);
		}
	});
	const auto refresh = [=] {
		const auto state = weak.lock();
		if (!state || state->pending || !view->isVisible()
			|| !Settings::Bool(kShowKey)) {
			return;
		}
		auto ids = visible();
		std::sort(ids.begin(), ids.end());
		ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
		std::erase_if(state->active, [&](const auto &entry) {
			return !std::binary_search(ids.begin(), ids.end(), entry.first);
		});
		Notify(*state);
		const auto generation = state->generation;
		for (auto first = ids.cbegin(); first != ids.cend();) {
			const auto end = first + std::min<ptrdiff_t>(
				kBatchSize, ids.cend() - first);
			const auto batch = std::vector<uint64>(first, end);
			first = end;
			auto portions = QStringList();
			for (const auto id : batch) {
				portions.push_back(QString::number(id));
			}
			++state->pending;
			const auto sentAt = crl::now();
			Bot::Ask(
				session,
				u"presence:v1:list:"_q + portions.join(u','),
				[=](Bot::Results results) {
					const auto state = weak.lock();
					if (!state || state->generation != generation) {
						return;
					}
					--state->pending;
					const auto now = crl::now();
					for (const auto &result : results) {
						if (!result.id.startsWith(kResultPrefix)) {
							continue;
						}
						const auto id = QStringView(result.id)
							.mid(kResultPrefix.size()).toULongLong();
						if (!std::binary_search(batch.begin(), batch.end(), id)) {
							continue;
						}
						// An empty batch can also mean the bot could not answer.
						if (result.description == u"0"_q) {
							state->active.erase(id);
						} else if (auto activity = Decode(result, sentAt, now);
								activity.track) {
							state->active.insert_or_assign(id, std::move(activity));
						}
					}
					Notify(*state);
				},
				[=] {
					if (const auto state = weak.lock();
						state && state->generation == generation) {
						--state->pending;
					}
				});
		}
	};
	state->poll.setCallback(refresh);
	lifetime.add([state] {});
	view->events() | rpl::filter([](not_null<QEvent*> event) {
		return event->type() == QEvent::Show;
	}) | rpl::on_next(refresh, lifetime);
	Settings::BoolValue(kShowKey) | rpl::on_next([=](bool enabled) {
		if (const auto state = weak.lock()) {
			if (enabled) {
				state->poll.callEach(kReadInterval);
				refresh();
			} else {
				++state->generation;
				state->pending = 0;
				state->poll.cancel();
				state->active.clear();
				Notify(*state);
			}
		}
	}, lifetime);
}

void PaintRing(QPainter &p, const QRect &avatar, crl::time now) {
	p.save();
	auto hq = PainterHighQualityEnabler(p);
	const auto rect = QRectF(avatar);
	const auto line = style::ConvertFloatScale(2.);
	const auto inset = line / 2.;
	const auto path = rect.adjusted(inset, inset, -inset, -inset);

	// MD3 indeterminate: дуга вращается и дышит длиной, как загрузка Google.
	constexpr auto kCycle = crl::time(1800);
	const auto phase = (now % kCycle) / double(kCycle);
	const auto sweep = 40. + 260. * (0.5 - 0.5 * std::cos(phase * M_PI * 2));
	const auto start = -90. + phase * 360.;

	auto arc = QPen(st::windowBgActive->c, line);
	arc.setCapStyle(Qt::RoundCap);
	p.setPen(arc);
	p.setBrush(Qt::NoBrush);
	p.drawArc(path, int(-start * 16), int(sweep * 16));
	p.restore();
}

} // namespace Mokagram::Music::Presence
