// This file is part of mokaGram, https://github.com/ecronxy/mokagram
// (c) 2026 @ecronx (https://t.me/ecronx). GPLv3 with the attribution term
// of MOKAGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
#include "mokagram/quote/mokagram_frame.h"

#include <QtGui/QPainter>

#include <algorithm>
#include <array>
#include <cmath>

namespace Mokagram::Quote {

QImage FrameImage(const QImage &content, QPoint &offset) {
	offset = QPoint();
	if (content.isNull()) {
		return QImage();
	}
	static const auto frame = QImage(QStringLiteral(":/gui/art/mokagram_ramka.jpg"));
	if (frame.isNull()) {
		return QImage();
	}
	// Aperture of the supplied 667 x 1000 artwork. Nine slices retain the
	// corners while the sides adapt to both short and tall messages.
	constexpr auto hole = QRect(139, 191, 388, 607);
	const auto scale = content.width() / double(hole.width());
	const auto left = std::max(1, int(std::round(hole.x() * scale)));
	const auto top = std::max(1, int(std::round(hole.y() * scale)));
	const auto right = std::max(1, int(std::round((frame.width() - hole.x() - hole.width()) * scale)));
	const auto bottom = std::max(1, int(std::round((frame.height() - hole.y() - hole.height()) * scale)));
	auto result = QImage(
		content.width() + left + right,
		content.height() + top + bottom,
		QImage::Format_ARGB32_Premultiplied);
	if (result.isNull()) {
		return result;
	}
	result.fill(Qt::white);
	auto p = QPainter(&result);
	p.setRenderHint(QPainter::SmoothPixmapTransform);
	const auto sx = std::array{ 0, hole.x(), hole.x() + hole.width(), frame.width() };
	const auto sy = std::array{ 0, hole.y(), hole.y() + hole.height(), frame.height() };
	const auto dx = std::array{ 0, left, left + content.width(), result.width() };
	const auto dy = std::array{ 0, top, top + content.height(), result.height() };
	for (auto y = 0; y != 3; ++y) {
		for (auto x = 0; x != 3; ++x) {
			if (x == 1 && y == 1) {
				continue;
			}
			p.drawImage(
				QRect(dx[x], dy[y], dx[x + 1] - dx[x], dy[y + 1] - dy[y]),
				frame,
				QRect(sx[x], sy[y], sx[x + 1] - sx[x], sy[y + 1] - sy[y]));
		}
	}
	offset = QPoint(left, top);
	// An explicit destination uses physical pixels even for high-DPI shots.
	p.drawImage(QRect(offset, content.size()), content);
	return result;
}

} // namespace Mokagram::Quote
