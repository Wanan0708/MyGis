#include "myform.h"
#include "ui_myform.h"
#include "tilemapmanager.h"  // 添加瓦片地图管理器头文件
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QPixmap>
#include <QWheelEvent>
#include <QScrollBar>
#include <QTimer>
#include <QShowEvent>
#include <QResizeEvent>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <cmath>

MyForm::MyForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MyForm)
    , isModified(false)
    , mapScene(nullptr)
    , mapItem(nullptr)
    , currentScale(1.0)
    , isRightClickDragging(false)  // 初始化右键拖拽状态
    , tileMapManager(nullptr)  // 初始化瓦片地图管理器
    , progressBar(nullptr)  // 初始化进度条
    , isDownloading(false)  // 初始化下载状态
{
    logMessage("=== MyForm constructor started ===");
    ui->setupUi(this);
    
    // 设置功能区
    setupFunctionalArea();
    
    // 设置地图区域
    setupMapArea();
    logMessage("=== MyForm constructor finished ===");
}

MyForm::~MyForm()
{
    delete ui;
}

void MyForm::showEvent(QShowEvent *event)
{
    // 窗口显示后设置splitter比例
    QWidget::showEvent(event);
    setupSplitter();
}

void MyForm::resizeEvent(QResizeEvent *event)
{
    // 窗口大小改变时重新设置splitter比例
    QWidget::resizeEvent(event);
    setupSplitter();
}

void MyForm::setupSplitter()
{
    // 设置splitter（listView与graphicsView的比例为1:4）
    ui->splitter->setStretchFactor(0, 1);
    ui->splitter->setStretchFactor(1, 4);
    
    // 设置具体的尺寸
    QList<int> sizes;
    int totalWidth = ui->splitter->width();
    if (totalWidth > 0) {
        sizes << totalWidth / 5 << totalWidth * 4 / 5;
        ui->splitter->setSizes(sizes);
    }
}

void MyForm::setupFunctionalArea() {
    // 为功能区设置对象名称，以便应用样式
    ui->functionalArea->setObjectName("functionalArea");
    
    // 显式连接信号槽，避免重复连接
    connect(ui->newButton, &QPushButton::clicked, this, &MyForm::handleNewButtonClicked);
    connect(ui->openButton, &QPushButton::clicked, this, &MyForm::handleOpenButtonClicked);
    connect(ui->saveButton, &QPushButton::clicked, this, &MyForm::handleSaveButtonClicked);
    connect(ui->saveAsButton, &QPushButton::clicked, this, &MyForm::handleSaveAsButtonClicked);
    connect(ui->undoButton, &QPushButton::clicked, this, &MyForm::handleUndoButtonClicked);
    connect(ui->redoButton, &QPushButton::clicked, this, &MyForm::handleRedoButtonClicked);
    
    // 地图控制按钮连接
    connect(ui->loadMapButton, &QPushButton::clicked, this, &MyForm::handleLoadMapButtonClicked);
    connect(ui->zoomInButton, &QPushButton::clicked, this, &MyForm::handleZoomInButtonClicked);
    connect(ui->zoomOutButton, &QPushButton::clicked, this, &MyForm::handleZoomOutButtonClicked);
    connect(ui->panButton, &QPushButton::clicked, this, &MyForm::handlePanButtonClicked);
    
    // 添加瓦片地图控制按钮连接
    connect(ui->loadTileMapButton, &QPushButton::clicked, this, &MyForm::handleLoadTileMapButtonClicked);
    connect(ui->zoomInTileMapButton, &QPushButton::clicked, this, &MyForm::handleZoomInTileMapButtonClicked);
    connect(ui->zoomOutTileMapButton, &QPushButton::clicked, this, &MyForm::handleZoomOutTileMapButtonClicked);
    
    // 初始化进度条
    progressBar = ui->progressBar;
    progressBar->setVisible(false);  // 初始时隐藏进度条
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    
    // 连接区域下载进度信号（在tileMapManager创建后再连接）
    // 初始化状态
    updateStatus("Ready");
}

void MyForm::setupMapArea() {
    logMessage("Setting up map area");
    
    // 创建地图场景
    mapScene = new QGraphicsScene(this);
    
    // 设置场景
    ui->graphicsView->setScene(mapScene);
    
    // 启用交互
    ui->graphicsView->setDragMode(QGraphicsView::NoDrag);
    ui->graphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    ui->graphicsView->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    
    // 设置滚动条策略
    ui->graphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->graphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // 启用鼠标跟踪
    ui->graphicsView->setMouseTracking(true);
    
    // 安装事件过滤器以处理滚轮事件
    ui->graphicsView->viewport()->installEventFilter(this);
    
    // 创建瓦片地图管理器
    logMessage("Creating TileMapManager");
    tileMapManager = new TileMapManager(this);
    logMessage(QString("TileMapManager created: %1").arg(tileMapManager != nullptr));
    tileMapManager->initScene(mapScene);
    
    // 连接下载进度信号（在这里连接，因为tileMapManager已经创建）
    logMessage("Connecting regionDownloadProgress signal");
    connect(tileMapManager, &TileMapManager::regionDownloadProgress, this, &MyForm::onRegionDownloadProgress);
    connect(tileMapManager, &TileMapManager::downloadFinished, this, [this]() {
        updateStatus("Tile map download completed");
        // 隐藏进度条
        isDownloading = false;
        progressBar->setVisible(false);
        progressBar->setValue(0);
    });
    connect(tileMapManager, &TileMapManager::localTilesFound, this, [this](int zoomLevel, int tileCount) {
        updateStatus(QString("Found %1 local tiles at zoom level %2").arg(tileCount).arg(zoomLevel));
    });
    connect(tileMapManager, &TileMapManager::noLocalTilesFound, this, [this]() {
        updateStatus("No local tiles found - Use 'Load Tile Map' to download");
    });
    logMessage("Signal connected");
    
    // 检查本地是否有已下载的瓦片，如果有则自动显示
    logMessage("Checking for local tiles...");
    updateStatus("Checking for local tiles...");
    tileMapManager->checkLocalTiles();
    logMessage("Local tiles check completed");
    
    // 显示本地瓦片信息
    tileMapManager->getLocalTilesInfo();
    
    // 显示当前可用的最大缩放级别
    int maxZoom = tileMapManager->getMaxAvailableZoom();
    if (maxZoom > 0) {
        updateStatus(QString("Ready - Max zoom level: %1").arg(maxZoom));
        logMessage(QString("Maximum available zoom level: %1").arg(maxZoom));
    } else {
        updateStatus("Ready - Use 'Load Tile Map' to download new tiles");
    }
}

bool MyForm::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->graphicsView->viewport()) {
        if (event->type() == QEvent::Wheel) {
            QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
            // 滚轮缩放（无需Ctrl键）
            qreal scaleFactor = 1.15;
            if (wheelEvent->angleDelta().y() > 0) {
                // 检查是否超过最大缩放限制
                if (currentScale * scaleFactor <= MAX_SCALE) {
                    // 设置变换锚点为鼠标位置
                    ui->graphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
                    ui->graphicsView->scale(scaleFactor, scaleFactor);
                    currentScale *= scaleFactor;
                }
            } else {
                // 检查是否低于最小缩放限制
                if (currentScale / scaleFactor >= MIN_SCALE) {
                    // 设置变换锚点为鼠标位置
                    ui->graphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
                    ui->graphicsView->scale(1/scaleFactor, 1/scaleFactor);
                    currentScale /= scaleFactor;
                }
            }
            updateStatus(QString("Zoom: %1x").arg(currentScale, 0, 'f', 2));
            return true; // 事件已处理
        } else if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            // 只有当鼠标在QGraphicsView区域时，右键按下才启用拖拽模式
            if (mouseEvent->button() == Qt::RightButton) {
                // 检查鼠标是否在graphicsView区域内
                QPoint mousePos = ui->graphicsView->mapFromGlobal(QCursor::pos());
                QRect viewRect = ui->graphicsView->rect();
                if (viewRect.contains(mousePos)) {
                    lastRightClickPos = mouseEvent->pos();
                    isRightClickDragging = true;
                    ui->graphicsView->setCursor(Qt::ClosedHandCursor);
                    return true; // 事件已处理
                }
            }
        } else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            // 只有当鼠标在QGraphicsView区域时，右键拖拽才生效
            if (isRightClickDragging && (mouseEvent->buttons() & Qt::RightButton)) {
                // 检查鼠标是否在graphicsView区域内
                QPoint mousePos = ui->graphicsView->mapFromGlobal(QCursor::pos());
                QRect viewRect = ui->graphicsView->rect();
                if (viewRect.contains(mousePos)) {
                    QPointF delta = mouseEvent->pos() - lastRightClickPos;
                    ui->graphicsView->horizontalScrollBar()->setValue(
                        ui->graphicsView->horizontalScrollBar()->value() - delta.x());
                    ui->graphicsView->verticalScrollBar()->setValue(
                        ui->graphicsView->verticalScrollBar()->value() - delta.y());
                    lastRightClickPos = mouseEvent->pos();
                    return true; // 事件已处理
                }
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            // 右键释放时禁用拖拽模式
            if (mouseEvent->button() == Qt::RightButton) {
                isRightClickDragging = false;
                ui->graphicsView->setCursor(Qt::ArrowCursor);
                return true; // 事件已处理
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void MyForm::updateStatus(const QString &message) {
    ui->statusLabel->setText(message);
    qDebug() << "Status:" << message;
}

void MyForm::loadMap(const QString &mapPath) {
    // 清除之前的地图项
    if (mapItem) {
        mapScene->removeItem(mapItem);
        delete mapItem;
        mapItem = nullptr;
    }
    
    // 加载新地图
    QPixmap pixmap(mapPath);
    if (!pixmap.isNull()) {
        mapItem = mapScene->addPixmap(pixmap);
        mapScene->setSceneRect(pixmap.rect());
        currentScale = 1.0;
        ui->graphicsView->resetTransform();
        updateStatus("Map loaded: " + mapPath);
    } else {
        updateStatus("Failed to load map: " + mapPath);
    }
}

void MyForm::handleNewButtonClicked()
{
    qDebug() << "New button clicked";
    currentFile.clear();
    // 这里可以初始化一个新的文档
    updateStatus("New document created");
    isModified = false;
}

void MyForm::handleOpenButtonClicked()
{
    qDebug() << "Open button clicked";
    
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), "", 
        tr("Text Files (*.txt);;All Files (*)"));
    
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            // 这里可以将文件内容加载到编辑区域
            QString content = in.readAll();
            // 假设我们有一个文本编辑器来显示内容
            // ui->textEdit->setPlainText(content);
            
            currentFile = fileName;
            updateStatus("Opened: " + fileName);
            isModified = false;
            file.close();
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Cannot open file %1").arg(fileName));
        }
    }
}

void MyForm::handleSaveButtonClicked()
{
    qDebug() << "Save button clicked";
    
    if (currentFile.isEmpty()) {
        // 如果没有当前文件，调用另存为
        handleSaveAsButtonClicked();
    } else {
        QFile file(currentFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            // 这里可以保存编辑区域的内容
            // out << ui->textEdit->toPlainText();
            
            updateStatus("Saved: " + currentFile);
            isModified = false;
            file.close();
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Cannot save file %1").arg(currentFile));
        }
    }
}

void MyForm::handleSaveAsButtonClicked()
{
    qDebug() << "Save As button clicked";
    
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save As"), "", 
        tr("Text Files (*.txt);;All Files (*)"));
    
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            // 这里可以保存编辑区域的内容
            // out << ui->textEdit->toPlainText();
            
            currentFile = fileName;
            updateStatus("Saved as: " + fileName);
            isModified = false;
            file.close();
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Cannot save file %1").arg(fileName));
        }
    }
}

void MyForm::handleUndoButtonClicked()
{
    qDebug() << "Undo button clicked";
    updateStatus("Undo action performed");
    // 这里可以实现撤销功能
    // ui->textEdit->undo();
}

void MyForm::handleRedoButtonClicked()
{
    qDebug() << "Redo button clicked";
    updateStatus("Redo action performed");
    // 这里可以实现重做功能
    // ui->textEdit->redo();
}

void MyForm::handleLoadMapButtonClicked()
{
    qDebug() << "Load Map button clicked";
    
    QString fileName = QFileDialog::getOpenFileName(this, tr("Load Map"), "", 
        tr("Image Files (*.png *.jpg *.bmp *.gif);;All Files (*)"));
    
    if (!fileName.isEmpty()) {
        loadMap(fileName);
    }
}

void MyForm::handleZoomInButtonClicked()
{
    qDebug() << "Zoom In button clicked";
    if (mapItem) {
        // 检查是否超过最大缩放限制
        if (currentScale * 1.2 <= MAX_SCALE) {
            // 设置变换锚点为鼠标位置
            ui->graphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
            currentScale *= 1.2;
            ui->graphicsView->scale(1.2, 1.2);
            updateStatus(QString("Zoom: %1x").arg(currentScale, 0, 'f', 2));
        }
    }
}

void MyForm::handleZoomOutButtonClicked()
{
    qDebug() << "Zoom Out button clicked";
    if (mapItem) {
        // 检查是否低于最小缩放限制
        if (currentScale / 1.2 >= MIN_SCALE) {
            // 设置变换锚点为鼠标位置
            ui->graphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
            currentScale /= 1.2;
            ui->graphicsView->scale(1/1.2, 1/1.2);
            updateStatus(QString("Zoom: %1x").arg(currentScale, 0, 'f', 2));
        }
    }
}

void MyForm::handlePanButtonClicked()
{
    qDebug() << "Pan button clicked";
    if (ui->graphicsView->dragMode() == QGraphicsView::ScrollHandDrag) {
        ui->graphicsView->setDragMode(QGraphicsView::NoDrag);
        ui->panButton->setText("Pan");
        updateStatus("Pan mode disabled");
    } else {
        ui->graphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
        ui->panButton->setText("Pan Off");
        updateStatus("Pan mode enabled - drag to move map");
    }
}

// 瓦片地图相关槽函数实现
void MyForm::handleLoadTileMapButtonClicked()
{
    logMessage("=== Load Tile Map button clicked ===");
    
    // 清除之前的图片地图项
    if (mapItem) {
        logMessage("Removing existing map item");
        if (mapItem->scene()) {  // 检查图形项是否属于某个场景
            mapScene->removeItem(mapItem);
        }
        delete mapItem;
        mapItem = nullptr;
    }
    
    // 清除之前的场景矩形
    logMessage("Clearing scene rect");
    mapScene->setSceneRect(0, 0, 0, 0);
    
    // 设置北京坐标为中心点
    logMessage("Setting center to Beijing coordinates: 39.9042, 116.4074");
    tileMapManager->setCenter(39.9042, 116.4074);
    // 设置缩放级别为3，与startRegionDownload中的一致
    logMessage("Setting zoom level to 3");
    tileMapManager->setZoom(3);
    
    // 触发中国区域级别3的瓦片地图下载
    logMessage("Calling startRegionDownload");
    startRegionDownload();
    
    updateStatus("Tile map loaded - downloading tiles...");
    logMessage("Tile map loading initiated");
}

void MyForm::handleZoomInTileMapButtonClicked()
{
    qDebug() << "Zoom In Tile Map button clicked";
    
    int currentZoom = tileMapManager->getZoom();
    int maxAvailableZoom = tileMapManager->getMaxAvailableZoom();
    
    // 使用动态的最大缩放限制
    if (currentZoom < maxAvailableZoom) {
        int newZoom = currentZoom + 1;
        tileMapManager->setZoom(newZoom);
        
        // 更新状态显示
        updateStatus(QString("Tile map zoom: level %1").arg(newZoom));
        qDebug() << "Tile map zoom in to level:" << newZoom;
    } else {
        updateStatus(QString("Maximum zoom level reached (%1) - No more tiles available").arg(maxAvailableZoom));
        qDebug() << "Cannot zoom in further, max available zoom:" << maxAvailableZoom;
    }
}

void MyForm::handleZoomOutTileMapButtonClicked()
{
    qDebug() << "Zoom Out Tile Map button clicked";
    
    int currentZoom = tileMapManager->getZoom();
    int maxAvailableZoom = tileMapManager->getMaxAvailableZoom();
    
    // 限制最小缩放级别为1，最大为可用缩放级别
    if (currentZoom > 1) {
        int newZoom = currentZoom - 1;
        tileMapManager->setZoom(newZoom);
        
        // 更新状态显示
        updateStatus(QString("Tile map zoom: level %1 (max: %2)").arg(newZoom).arg(maxAvailableZoom));
        qDebug() << "Tile map zoom out to level:" << newZoom;
    } else {
        updateStatus(QString("Minimum zoom level reached (1) - Max available: %1").arg(maxAvailableZoom));
    }
}

void MyForm::onTileDownloadProgress(int current, int total)
{
    if (total > 0) {
        int progress = (current * 100) / total;
        updateStatus(QString("Downloading tiles: %1% (%2/%3)").arg(progress).arg(current).arg(total));
    } else {
        updateStatus(QString("Downloading tiles: %1 tiles").arg(current));
    }
}

void MyForm::onRegionDownloadProgress(int current, int total, int zoom)
{
    qDebug() << "MyForm::onRegionDownloadProgress received:" << current << "/" << total << "zoom:" << zoom;
    
    // 显示进度条
    if (!isDownloading) {
        isDownloading = true;
        progressBar->setVisible(true);
    }
    
    if (total > 0) {
        int progress = (current * 100) / total;
        qDebug() << "Download progress:" << current << "/" << total << "(" << progress << "%) at zoom level" << zoom;
        
        // 更新进度条
        progressBar->setValue(progress);
        progressBar->setFormat(QString("Downloading zoom level %1: %2% (%3/%4)").arg(zoom).arg(progress).arg(current).arg(total));
        
        // 更新状态标签
        updateStatus(QString("Downloading zoom level %1: %2% (%3/%4)").arg(zoom).arg(progress).arg(current).arg(total));
    } else {
        qDebug() << "Download progress:" << current << "tiles at zoom level" << zoom;
        updateStatus(QString("Downloading zoom level %1: %2 tiles").arg(zoom).arg(current));
    }
}

void MyForm::startRegionDownload()
{
    logMessage("=== Starting region download ===");
    
    // 下载中国区域的地图瓦片
    // 中国大致范围：纬度18°N-54°N，经度73°E-135°E
    // 下载缩放级别：1-10级，提供良好的缩放体验
    logMessage("Calling tileMapManager->downloadRegion for China region");
    logMessage("Parameters: minLat=18.0, maxLat=54.0, minLon=73.0, maxLon=135.0, minZoom=1, maxZoom=10");
    tileMapManager->downloadRegion(18.0, 54.0, 73.0, 135.0, 1, 10);
    
    updateStatus("Starting China region map download (levels 1-10)...");
    logMessage("China region map download initiated - Levels 1-10");
}

void MyForm::logMessage(const QString &message)
{
    // 记录日志到文件
    QFile logFile("debug.log");
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&logFile);
        out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " - " << message << "\n";
        logFile.close();
    }
    
    // 同时输出到控制台
    qDebug() << message;
}
