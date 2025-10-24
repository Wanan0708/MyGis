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
#include <QTimer>
#include <QPointer>
#include <QScrollBar>
#include <QToolButton>
#include <QGraphicsProxyWidget>
#include "maptools.h"

// 添加TileMapManager的前置声明
class TileMapManager;
class QPropertyAnimation;

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
    void keyPressEvent(QKeyEvent *event) override { if (toolManager && toolManager->handleKeyPress(event)) return; QWidget::keyPressEvent(event);} 

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

    // Overlay 相关槽
    void onOverlayPanToggled(bool checked);

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
    static constexpr int MIN_ZOOM_LEVEL = 3;   // 最小缩放层级（限制为3-10）
    static constexpr int MAX_ZOOM_LEVEL = 10;  // 最大缩放层级
    
    // 瓦片地图管理器
    TileMapManager *tileMapManager;
    
    // 进度条相关
    QProgressBar *progressBar;
    bool isDownloading;
    
    // 拖动更新相关
    QTimer *viewUpdateTimer;  // 延迟更新定时器
    // 滚动条悬浮延迟展开
    QTimer *hScrollHoverTimer = nullptr;
    QTimer *vScrollHoverTimer = nullptr;
    bool hScrollExpanded = false;
    bool vScrollExpanded = false;
    QPointer<QScrollBar> hScrollTarget;
    QPointer<QScrollBar> vScrollTarget;

    // GraphicsView 右上角浮动工具（两种模式：viewport-widget 或 scene-proxy）
    QWidget *gvOverlay = nullptr;               // 直接贴在 viewport 上
    QGraphicsProxyWidget *overlayProxy = nullptr; // 作为场景项添加
    QWidget *overlayPanel = nullptr;            // 场景代理承载的面板
    QToolButton *btnZoomIn = nullptr;
    QToolButton *btnZoomOut = nullptr;
    QToolButton *btnPanToggle = nullptr;

    // 测量工具（功能区）
    QPushButton *btnMeasureDistance = nullptr;
    QPushButton *btnMeasureArea = nullptr;
    QPushButton *btnMeasureClear = nullptr;

    enum class MeasureMode { None, Distance, Area };
    MeasureMode measureMode = MeasureMode::None;

    // 工具系统
    ToolManager *toolManager = nullptr;

    // 图形视图左下角状态覆盖
    QLabel *gvStatusLabel = nullptr;
    QTimer *gvStatusDelayTimer = nullptr;
    QPropertyAnimation *gvStatusFadeAnim = nullptr;
    
    // 日志记录函数
    void logMessage(const QString &message);
    void updateVisibleTiles();  // 更新可见瓦片
    
    void setupFunctionalArea();
    void setupMapArea();
    void setupSplitter();
    void updateStatus(const QString &message);
    void loadMap(const QString &mapPath);
    void createGraphicsOverlay();               // viewport 方案
    void positionGraphicsOverlay();
    void positionStatusOverlay();               // 左下角状态覆盖定位
    void createGraphicsOverlayScene();          // scene+proxy 方案（兜底）
    void positionGraphicsOverlayScene();
    
    // 添加公共方法来触发区域下载
public:
    void startRegionDownload(); // 公共方法来触发区域下载
};

#endif // MYFORM_H