#ifndef MAPMANAGERDIALOG_H
#define MAPMANAGERDIALOG_H

#include <QDialog>

class QProgressBar;
class QLabel;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QListWidget;
template <typename K, typename V> class QHash;
#include "mapmanagersettings.h"

class QLineEdit;
class QSpinBox;
class QComboBox;
class QPushButton;

class MapManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit MapManagerDialog(QWidget *parent = nullptr);

public slots:
    void onTaskProgress(const QString &taskId, int completed, int total);
    MapManagerSettings getSettings() const;
    void setSettings(const MapManagerSettings &s);

signals:
    void requestSaveSettings();
    void requestStartDownload();
    void requestPause();
    void requestResume();
    void requestPauseTask(const QString &taskId);
    void requestResumeTask(const QString &taskId);
    void requestCancelTask(const QString &taskId);

private:
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_progressLabel = nullptr;
    QString m_currentTaskId;

    // Settings fields
    QLineEdit *m_editUrl = nullptr;
    QLineEdit *m_editServers = nullptr;
    QLineEdit *m_editCacheDir = nullptr;
    QLineEdit *m_editMinLat = nullptr;
    QLineEdit *m_editMaxLat = nullptr;
    QLineEdit *m_editMinLon = nullptr;
    QLineEdit *m_editMaxLon = nullptr;
    QSpinBox  *m_spinMinZoom = nullptr;
    QSpinBox  *m_spinMaxZoom = nullptr;
    QSpinBox  *m_spinConcurrent = nullptr;
    QSpinBox  *m_spinRate = nullptr;
    QSpinBox  *m_spinRetry = nullptr;
    QSpinBox  *m_spinBackoff = nullptr;
    QSpinBox  *m_spinPrefetch = nullptr;
    QPushButton *m_btnSave = nullptr;
    QPushButton *m_btnStart = nullptr;
    QPushButton *m_btnPauseResume = nullptr;
    QListWidget *m_taskList = nullptr;
    QHash<QString, class QListWidgetItem*> *m_taskItems = nullptr;
};

#endif // MAPMANAGERDIALOG_H


