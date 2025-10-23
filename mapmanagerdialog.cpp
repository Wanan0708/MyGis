#include "mapmanagerdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QLineEdit>
#include <QSpinBox>
#include <QListWidget>
#include <QGridLayout>

MapManagerDialog::MapManagerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("地图管理"));
    setModal(false);
    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel(tr("这里将配置地图源、下载范围、并发与限速、任务与进度等。")));

    // Settings area (simple form)
    QGridLayout *grid = new QGridLayout();
    int r = 0;
    grid->addWidget(new QLabel(tr("源模板")), r, 0); m_editUrl = new QLineEdit(this); grid->addWidget(m_editUrl, r++, 1);
    grid->addWidget(new QLabel(tr("服务器列表(a,b,c)")), r, 0); m_editServers = new QLineEdit(this); grid->addWidget(m_editServers, r++, 1);
    grid->addWidget(new QLabel(tr("缓存路径")), r, 0); m_editCacheDir = new QLineEdit(this); grid->addWidget(m_editCacheDir, r++, 1);
    // 区域（可选），默认中国
    grid->addWidget(new QLabel(tr("最小纬度")), r, 0); m_editMinLat = new QLineEdit(this); m_editMinLat->setText("18"); grid->addWidget(m_editMinLat, r++, 1);
    grid->addWidget(new QLabel(tr("最大纬度")), r, 0); m_editMaxLat = new QLineEdit(this); m_editMaxLat->setText("54"); grid->addWidget(m_editMaxLat, r++, 1);
    grid->addWidget(new QLabel(tr("最小经度")), r, 0); m_editMinLon = new QLineEdit(this); m_editMinLon->setText("73"); grid->addWidget(m_editMinLon, r++, 1);
    grid->addWidget(new QLabel(tr("最大经度")), r, 0); m_editMaxLon = new QLineEdit(this); m_editMaxLon->setText("135"); grid->addWidget(m_editMaxLon, r++, 1);
    grid->addWidget(new QLabel(tr("层级最小")), r, 0); m_spinMinZoom = new QSpinBox(this); m_spinMinZoom->setRange(0, 20); grid->addWidget(m_spinMinZoom, r++, 1);
    grid->addWidget(new QLabel(tr("层级最大")), r, 0); m_spinMaxZoom = new QSpinBox(this); m_spinMaxZoom->setRange(0, 20); grid->addWidget(m_spinMaxZoom, r++, 1);
    grid->addWidget(new QLabel(tr("最大并发")), r, 0); m_spinConcurrent = new QSpinBox(this); m_spinConcurrent->setRange(1, 64); grid->addWidget(m_spinConcurrent, r++, 1);
    grid->addWidget(new QLabel(tr("每秒请求数")), r, 0); m_spinRate = new QSpinBox(this); m_spinRate->setRange(1, 128); grid->addWidget(m_spinRate, r++, 1);
    grid->addWidget(new QLabel(tr("最大重试")), r, 0); m_spinRetry = new QSpinBox(this); m_spinRetry->setRange(0, 10); grid->addWidget(m_spinRetry, r++, 1);
    grid->addWidget(new QLabel(tr("退避毫秒")), r, 0); m_spinBackoff = new QSpinBox(this); m_spinBackoff->setRange(0, 600000); grid->addWidget(m_spinBackoff, r++, 1);
    grid->addWidget(new QLabel(tr("预取环")), r, 0); m_spinPrefetch = new QSpinBox(this); m_spinPrefetch->setRange(0, 2); grid->addWidget(m_spinPrefetch, r++, 1);
    lay->addLayout(grid);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);
    m_progressLabel = new QLabel(tr("进度: 0%"), this);
    lay->addWidget(m_progressBar);
    lay->addWidget(m_progressLabel);
    m_btnSave = new QPushButton(tr("保存设置"));
    m_btnStart = new QPushButton(tr("开始下载"));
    m_btnPauseResume = new QPushButton(tr("暂停"));
    QHBoxLayout *ops = new QHBoxLayout();
    ops->addWidget(m_btnSave);
    ops->addWidget(m_btnStart);
    ops->addWidget(m_btnPauseResume);
    lay->addLayout(ops);
    connect(m_btnSave, &QPushButton::clicked, this, &MapManagerDialog::requestSaveSettings);
    connect(m_btnStart, &QPushButton::clicked, this, &MapManagerDialog::requestStartDownload);
    connect(m_btnPauseResume, &QPushButton::clicked, this, [this]() {
        if (m_btnPauseResume->text() == tr("暂停")) { emit requestPause(); m_btnPauseResume->setText(tr("继续")); }
        else { emit requestResume(); m_btnPauseResume->setText(tr("暂停")); }
    });
    // Task list (basic)
    m_taskList = new QListWidget(this);
    lay->addWidget(m_taskList);
    m_taskItems = new QHash<QString, QListWidgetItem*>();
}

void MapManagerDialog::onTaskProgress(const QString &taskId, int completed, int total)
{
    m_currentTaskId = taskId;
    int percent = (total > 0) ? (completed * 100 / total) : 0;
    m_progressBar->setValue(percent);
    m_progressLabel->setText(tr("进度: %1% (%2/%3)").arg(percent).arg(completed).arg(total));

    // 列表中更新或新增任务项
    QListWidgetItem *item = m_taskItems->value(taskId, nullptr);
    if (!item) {
        item = new QListWidgetItem(taskId, m_taskList);
        (*m_taskItems)[taskId] = item;
    }
    item->setText(tr("%1  %2/%3 (%4%)").arg(taskId, QString::number(completed), QString::number(total), QString::number(percent)));
}

MapManagerSettings MapManagerDialog::getSettings() const
{
    MapManagerSettings s;
    s.tileUrlTemplate = m_editUrl ? m_editUrl->text() : s.tileUrlTemplate;
    if (m_editServers) s.servers = m_editServers->text().split(',', Qt::SkipEmptyParts);
    if (m_editCacheDir) s.cacheDir = m_editCacheDir->text();
    if (m_spinMinZoom) s.minZoom = m_spinMinZoom->value();
    if (m_spinMaxZoom) s.maxZoom = m_spinMaxZoom->value();
    if (m_spinConcurrent) s.maxConcurrent = m_spinConcurrent->value();
    if (m_spinRate) s.rateLimitPerSec = m_spinRate->value();
    if (m_spinRetry) s.retryMax = m_spinRetry->value();
    if (m_spinBackoff) s.backoffInitialMs = m_spinBackoff->value();
    if (m_spinPrefetch) s.prefetchRing = m_spinPrefetch->value();
    return s;
}

void MapManagerDialog::setSettings(const MapManagerSettings &s)
{
    if (m_editUrl) m_editUrl->setText(s.tileUrlTemplate);
    if (m_editServers) m_editServers->setText(s.servers.join(','));
    if (m_editCacheDir) m_editCacheDir->setText(s.cacheDir);
    if (m_spinMinZoom) m_spinMinZoom->setValue(s.minZoom);
    if (m_spinMaxZoom) m_spinMaxZoom->setValue(s.maxZoom);
    if (m_spinConcurrent) m_spinConcurrent->setValue(s.maxConcurrent);
    if (m_spinRate) m_spinRate->setValue(s.rateLimitPerSec);
    if (m_spinRetry) m_spinRetry->setValue(s.retryMax);
    if (m_spinBackoff) m_spinBackoff->setValue(s.backoffInitialMs);
    if (m_spinPrefetch) m_spinPrefetch->setValue(s.prefetchRing);
}


