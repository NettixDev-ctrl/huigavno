// This file is part of mokaGram, https://github.com/ecronxy/mokagram
// (c) 2026 @ecronx (https://t.me/ecronx). GPLv3 with the attribution term
// of MOKAGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
#include "mokagram/music/mokagram_tyanka_hikka.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "core/file_location.h"
#include "data/data_document.h"
#include "data/data_session.h"
#include "main/main_session.h"
#include "media/audio/media_audio.h"
#include "media/audio/media_audio_track.h"
#include "media/player/media_player_instance.h"
#include "mokagram/lang/mokagram_lang.h"
#include "mokagram/music/mokagram_soundcloud.h"
#include "mokagram/settings/mokagram_settings.h"
#include "settings.h"
#include "storage/storage_account.h"
#include "window/window_controller.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QRegularExpression>
#include <QtCore/QSaveFile>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QWidget>

#include <array>

namespace Mokagram::TyankaHikka {
namespace {

constexpr auto kSong = AudioMsgId::Type::Song;
constexpr auto kDocumentId = DocumentId(0x4D4F4D0000000001ULL);
constexpr auto kExcerptStarts = std::array{ 34000, 56000, 82000, 107000 };
constexpr auto kExcerptLength = crl::time(2400);

[[nodiscard]] QString SettingKey() {
	return u"player.tyanka_hikka"_q;
}

[[nodiscard]] QString UnlockPath() {
	return cWorkingDir() + u"tdata/moka_tyanka_hikka.unlocked"_q;
}

[[nodiscard]] QString TrackPath() {
	const auto local = cWorkingDir() + u"tdata/moka_tyanka_hikka.m4a"_q;
	if (QFileInfo(local).size() > 0) {
		return local;
	}
	const auto cached = Music::CacheDir()
		+ u"/send/tyankahikka - тянка хикка - АХЕГАО (prod. Aki).m4a"_q;
	if (QFileInfo(cached).size() <= 0) {
		return {};
	}
	return QFile::copy(cached, local) ? local : cached;
}

[[nodiscard]] const QRegularExpression &ArtistPattern() {
	static const auto result = QRegularExpression(
		uR"((?<![\p{L}\p{N}_])(?:tyanka\s*hikka|тянка\s*хикка)(?![\p{L}\p{N}_]))"_q,
		QRegularExpression::CaseInsensitiveOption
			| QRegularExpression::UseUnicodePropertiesOption);
	return result;
}

[[nodiscard]] bool IsCurrentSong() {
	const auto current = Media::Player::instance()->current(kSong);
	const auto document = current.audio();
	return document && document->id == kDocumentId;
}

void RegisterSetting(bool unlocked) {
	Settings::Register({
		.key = SettingKey(),
		.section = QString::fromUtf8(Lang::Tr("Плеер")),
		.title = QString::fromUtf8(Lang::Tr("Тянка хикка")),
		.description = QString::fromUtf8(Lang::Tr("Отрывки песни звучат при действиях в клиенте. Упоминание тянка хикка в отправленном сообщении запускает трек целиком.")),
		.category = u"look"_q,
		.sectionIcon = u"hide"_q,
		.hidden = !unlocked,
	});
}

class Mode final : public QObject {
public:
	Mode() : _unlocked(QFile::exists(UnlockPath())) {
		RegisterSetting(_unlocked);
		Settings::BoolValue(SettingKey()) | rpl::on_next([this](bool enabled) {
			setEnabled(_unlocked && enabled);
		}, _lifetime);
	}

	void unlock() {
		if (!_unlocked) {
			auto marker = QSaveFile(UnlockPath());
			if (!marker.open(QIODevice::WriteOnly) || !marker.commit()) {
				return;
			}
			_unlocked = true;
			RegisterSetting(true);
		}
		const auto wasEnabled = _enabled;
		Settings::SetBool(SettingKey(), true);
		if (!wasEnabled) {
			if (const auto window = Core::App().activeWindow()) {
				window->showToast(QString::fromUtf8(Lang::Tr(_path.isEmpty()
					? "Тянка хикка включена. Аудиофайл не найден."
					: "Режим «Тянка хикка» включён")));
			}
		}
	}

	void playForMessage(not_null<Main::Session*> session, const QString &text) {
		if (!_enabled || !ArtistPattern().match(text).hasMatch()) {
			return;
		}
		if (_path.isEmpty() || !QFile::exists(_path)) {
			_path = TrackPath();
			if (_path.isEmpty()) {
				return;
			}
		}

		_excerpts = {};
		const auto document = session->data().document(
			kDocumentId,
			0,
			QByteArray(),
			base::unixtime::now(),
			{ MTP_documentAttributeAudio(
				MTP_flags(MTPDdocumentAttributeAudio::Flag::f_title
					| MTPDdocumentAttributeAudio::Flag::f_performer),
				MTP_int(148),
				MTP_string(u"тянка хикка - АХЕГАО (prod. Aki)"_q),
				MTP_string(u"tyankahikka"_q),
				MTPstring()) },
			u"audio/mp4"_q,
			InlineImageLocation(),
			ImageWithLocation(),
			ImageWithLocation(),
			false,
			0,
			QFileInfo(_path).size());
		document->setLocation(Core::FileLocation(_path));
		session->local().setMediaLastPlaybackPosition(kDocumentId, 0);
		Media::Player::instance()->play(AudioMsgId(document, FullMsgId()), std::nullopt);
	}

protected:
	bool eventFilter(QObject *watched, QEvent *event) override {
		if (!event->spontaneous()
			|| !watched->isWidgetType()
			|| _path.isEmpty()) {
			return false;
		}
		const auto type = event->type();
		if (type == QEvent::MouseButtonRelease
			|| type == QEvent::Wheel
			|| (type == QEvent::KeyPress
				&& !static_cast<QKeyEvent*>(event)->isAutoRepeat())) {
			playExcerpt();
		}
		return false;
	}

private:
	void setEnabled(bool enabled) {
		if (_enabled == enabled) {
			return;
		}
		_enabled = enabled;
		const auto app = QCoreApplication::instance();
		if (enabled) {
			_path = TrackPath();
			app->installEventFilter(this);
			playExcerpt();
		} else {
			app->removeEventFilter(this);
			_excerpts = {};
			if (IsCurrentSong()) {
				Media::Player::instance()->stop(kSong);
			}
		}
	}

	void playExcerpt() {
		if (_path.isEmpty()
			|| !Core::App().settings().notificationsVolume()
			|| std::ranges::any_of(_excerpts, [](const auto &excerpt) {
				return excerpt && excerpt->isActive();
			})) {
			return;
		}
		if (IsCurrentSong()) {
			const auto state = Media::Player::instance()->getState(kSong).state;
			if (!Media::Player::IsStoppedOrStopping(state)
				&& !Media::Player::IsPausedOrPausing(state)) {
				return;
			}
		}
		auto &excerpt = _excerpts[_nextExcerpt];
		if (!excerpt) {
			excerpt = Media::Audio::Current().createTrack();
			excerpt->fillFromFile(_path, kExcerptStarts[_nextExcerpt], kExcerptLength);
		}
		_nextExcerpt = (_nextExcerpt + 1) % kExcerptStarts.size();
		excerpt->playOnce();
	}

	bool _unlocked = false;
	bool _enabled = false;
	QString _path;
	size_t _nextExcerpt = 0;
	std::array<std::unique_ptr<Media::Audio::Track>, kExcerptStarts.size()> _excerpts;
	rpl::lifetime _lifetime;
};

std::unique_ptr<Mode> Instance;

} // namespace

void Start() {
	Instance = std::make_unique<Mode>();
}

void Finish() {
	Instance.reset();
}

bool IsArtist(const QString &name) {
	const auto trimmed = name.trimmed();
	const auto match = ArtistPattern().match(trimmed);
	return match.hasMatch() && match.capturedLength() == trimmed.size();
}

void Unlock() {
	if (Instance) {
		Instance->unlock();
	}
}

void OnOwnMessage(not_null<Main::Session*> session, const QString &text) {
	if (Instance) {
		Instance->playForMessage(session, text);
	}
}

} // namespace Mokagram::TyankaHikka
