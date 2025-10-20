#ifndef TILEWORKER_H
#define TILEWORKER_H

#include <QObject>
#include <QPixmap>
#include <QByteArray>
#include <QString>

class TileWorker : public QObject
{
    Q_OBJECT

public:
    explicit TileWorker(QObject *parent = nullptr);

public slots:
    void downloadAndSaveTile(int x, int y, int z, const QString &url, const QString &filePath);
    void loadTileFromFile(int x, int y, int z, const QString &filePath);

private:
    // 异步下载函数
    void downloadAndSaveTileAsync(int x, int y, int z, const QString &url, const QString &filePath);
    // 执行下载的函数
    bool performDownload(int x, int y, int z, const QString &url, const QString &filePath);

private slots:
    void onDownloadFinished();
    void onDownloadTimeout();

signals:
    void tileDownloaded(int x, int y, int z, const QByteArray &data, bool success, const QString &errorString);
    void tileLoaded(int x, int y, int z, const QPixmap &pixmap, bool success, const QString &errorString);
};

#endif // TILEWORKER_H