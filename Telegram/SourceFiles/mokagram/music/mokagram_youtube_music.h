// This file is part of mokaGram, https://github.com/mokagram/mokagram
// (c) 2026 mokaGram contributors. GPLv3 with the attribution term
// of MOKAGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
#pragma once

#include "base/basic_types.h"

#include <QtCore/QString>

namespace Mokagram::Music {

struct Track;

[[nodiscard]] bool HasYouTubeMusicMetadata(const Track &track);

// Запускается в главном потоке. Возвращает путь к аудио в кэше.
void DownloadYouTubeMusic(
	const Track &track,
	Fn<void(QString path, bool missingDownloader)> done);

} // namespace Mokagram::Music
