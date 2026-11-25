#pragma once

#include "media/streaming/media_streaming_player.h"
#include "ui/effects/animations.h"

#include <QtNetwork/QNetworkAccessManager>

class QPainter;

namespace Mokagram::Music {

class CanvasPlayer final : public QObject {
public:
	CanvasPlayer(QObject *parent, Fn<void()> repaint, bool prepareOnly = false);
	~CanvasPlayer();

	void refresh(QString id, QString title, QString artist, bool playing);
	void setVisible(bool visible);
	[[nodiscard]] bool loading() const { return _loading; }
	[[nodiscard]] QImage frame();
	[[nodiscard]] QImage frameForPaint();
	void paintLoading(QPainter &p, QRect area, QColor color);

private:
	struct Candidate {
		QString trackId;
		QUrl video;
	};

	void get(QUrl url, int limit, Fn<void(QByteArray)> done);
	void findArtist(const QString &artist);
	void readArtist(const QByteArray &data, const QString &artist);
	void readCandidates(const QByteArray &data);
	void findTitle();
	void loadVideo(QUrl url);
	void startVideo(QByteArray data);
	void updatePlayback();
	void setLoading(bool loading);

	QNetworkAccessManager _network;
	std::unique_ptr<QObject> _requests;
	Fn<void()> _repaint;
	const bool _prepareOnly;
	QString _id;
	QString _title;
	QString _artist;
	std::vector<Candidate> _candidates;
	int _candidate = 0;
	int _pages = 0;
	QUrl _nextPage;
	bool _playing = false;
	bool _visible = false;
	bool _loading = false;
	float64 _loadingProgress = -1.;
	Ui::Animations::Basic _loadingAnimation;
	std::unique_ptr<Media::Streaming::Player> _player;
	rpl::lifetime _playbackLifetime;

};

} // namespace Mokagram::Music
