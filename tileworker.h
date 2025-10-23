#ifndef TILEWORKER_H
#define TILEWORKER_H

#include <QObject>
#include <QPixmap>
#include <QByteArray>
#include <QString>

class QNetworkAccessManager;

class TileWorker : public QObject
{
    Q_OBJECT

public:
    explicit TileWorker(QObject *parent = nullptr);

public slots:
    void downloadAndSaveTile(int x, int y, int z, const QString &url, const QString &filePath);
    void loadTileFromFile(int x, int y, int z, const QString &filePath);
    // 全异步网络模式（非阻塞，使用 QNetworkReply 信号）
    void downloadAsync(int x, int y, int z, const QString &url, const QString &filePath);
    void configureNetworkRetries(int retryMax, int backoffInitialMs);

private:
    // 异步下载函数
    void downloadAndSaveTileAsync(int x, int y, int z, const QString &url, const QString &filePath);
    // 执行下载的函数
    bool performDownload(int x, int y, int z, const QString &url, const QString &filePath);

    // 复用网络访问管理器（在工作线程内懒创建）
    QNetworkAccessManager* networkManager();
    QNetworkAccessManager *m_manager = nullptr;

private slots:
    void onDownloadFinished();
    void onDownloadTimeout();
    void onReplyFinished();
    void onReplyReadyRead();
    void onReplyError();

private:
    void startAsyncRequest(int x, int y, int z, const QString &url, const QString &filePath, int attempt);
    int m_retryMax = 3;
    int m_backoffInitialMs = 3000;

signals:
    void tileDownloaded(int x, int y, int z, const QByteArray &data, bool success, const QString &errorString);
    void tileLoaded(int x, int y, int z, const QPixmap &pixmap, bool success, const QString &errorString);
    // 新增：跨线程安全的加载结果（传输原始字节，由主线程构建 QPixmap）
    void tileLoadedBytes(int x, int y, int z, const QByteArray &data, bool success, const QString &errorString);
};

#endif // TILEWORKER_H