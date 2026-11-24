// This file is part of mokaGram, https://github.com/ecronxy/mokagram
// (c) 2026 @ecronx (https://t.me/ecronx). GPLv3 with the attribution term
// of MOKAGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
#pragma once

#include <QtCore/QPoint>
#include <QtGui/QImage>

namespace Mokagram::Quote {

// Image pixels, independent of interface scale. Content is never cropped or resized.
[[nodiscard]] QImage FrameImage(const QImage &content, QPoint &offset);

} // namespace Mokagram::Quote
