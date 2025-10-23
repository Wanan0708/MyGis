#ifndef MYFORM_H
#define MYFORM_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QListView>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QPixmap>
#include <QPoint>
#include <QProgressBar>

// 添加TileMapManager的前置声明
class TileMapManager;

namespace Ui {
class MyForm;
}

class MyForm : public QWidget
{
    Q_OBJECT

public:
    explicit MyForm(QWidget *parent = nullptr);
    ~MyForm();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    // 重命名槽函数，避免Qt自动连接
    void handleNewButtonClicked();
    void handleOpenButtonClicked();
    void handleSaveButtonClicked();
    void handleSaveAsButtonClicked();
    void handleUndoButtonClicked();
    void handleRedoButtonClicked();
    
    // 地图相关槽函数
    void handleLoadMapButtonClicked();
    void handleZoomInButtonClicked();
    void handleZoomOutButtonClicked();
    void handlePanButtonClicked();
    
    // 瓦片地图相关槽函数
    void handleLoadTileMapButtonClicked();
    void handleZoomInTileMapButtonClicked();
    void handleZoomOutTileMapButtonClicked();
    
    // 瓦片下载进度槽函数
    void onTileDownloadProgress(int current, int total);
    void onRegionDownloadProgress(int current, int total, int zoom); // 区域下载进度槽函数

private:
    Ui::MyForm *ui;
    QString currentFile;
    bool isModified;
    
    // 地图相关成员
    QGraphicsScene *mapScene;
    QGraphicsPixmapItem *mapItem;
    qreal currentScale;
    int currentZoomLevel;  // 当前瓦片地图缩放层级 (1-10)
    
    // 右键拖拽相关成员
    bool isRightClickDragging;
    QPoint lastRightClickPos;
    QPointF lastRightClickScenePos;
    
    // 缩放限制
    static constexpr int MIN_ZOOM_LEVEL = 1;   // 最小缩放层级
    static constexpr int MAX_ZOOM_LEVEL = 10;  // 最大缩放层级
    
    // 瓦片地图管理器
    TileMapManager *tileMapManager;
    
    // 进度条相关
    QProgressBar *progressBar;
    bool isDownloading;
    
    // 拖动更新相关
    QTimer *viewUpdateTimer;  // 延迟更新定时器
    
    // 日志记录函数
    void logMessage(const QString &message);
    void updateVisibleTiles();  // 更新可见瓦片
    
    void setupFunctionalArea();
    void setupMapArea();
    void setupSplitter();
    void updateStatus(const QString &message);
    void loadMap(const QString &mapPath);
    
    // 添加公共方法来触发区域下载
public:
    void startRegionDownload(); // 公共方法来触发区域下载
};

#endif // MYFORM_H