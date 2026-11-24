// This file is part of mokaGram, https://github.com/mokagram/mokagram
// (c) 2026 mokaGram contributors. GPLv3 with the attribution term
// of MOKAGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
#pragma once

#include "base/basic_types.h"

#include <QtCore/QString>

#include <vector>

namespace Mokagram::Music {

struct Track;
struct Album;
struct ArtistInfo;

// Native YouTube Music identities use "yt:" followed by a UC / album browse id.
// Call on the main thread; callbacks run on the main thread.
void ResolveYouTubeMusicArtist(
	const QString &name,
	Fn<void(ArtistInfo)> done);

// Keeps a known native channel identity; shares artist browse requests with catalog.
void LoadYouTubeMusicArtistInfo(
	const QString &artistId,
	Fn<void(ArtistInfo)> done);

// Loads every native songs/releases page, including continuations.
void LoadYouTubeMusicArtistCatalog(
	const QString &artistId,
	Fn<void(std::vector<Track>, std::vector<Album>, QString)> done);

// Keeps the native album track order. Errors are never reported as empty success.
void LoadYouTubeMusicAlbum(
	const QString &albumId,
	Fn<void(std::vector<Track>, QString)> done);

[[nodiscard]] bool HasYouTubeMusicMetadata(const Track &track);

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
