// This file is part of mokaGram, https://github.com/mokagram/mokagram
// (c) 2026 mokaGram contributors. GPLv3 with the attribution term
// of MOKAGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
#pragma once

#include "base/basic_types.h"

#include <QtCore/QString>

#include <vector>

namespace Mokagram::Music {

struct Track;

[[nodiscard]] bool HasYouTubeMusicMetadata(const Track &track);

// Популярные песни исполнителя из каталога YouTube Music. Возвращает
// неиграемые напрямую Track с videoId: плеер скачивает их через yt-dlp.
void LoadArtistTracks(
	const QString &artist,
	int limit,
	Fn<void(std::vector<Track>)> done);

// Рекомендации YouTube Music для трека с известным videoId.
void LoadYouTubeMusicRecommendations(
	const QString &videoId,
	Fn<void(std::vector<Track>)> done);

// Запускается в главном потоке. Возвращает путь к аудио в кэше.
// Скачанный файл живёт в общем LRU кэше аудио, повторные запросы объединяются.
void DownloadYouTubeMusic(
	const Track &track,
	Fn<void(const QString &path, const QString &error)> done);

} // namespace Mokagram::Music
