#pragma once
#include <QMainWindow>
#include <QJsonObject>
#include <QVector3D>

class GodotWidget;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* e) override;

private slots:
    void onStartGodot();
    void onStopGodot();
    void onGodotReady();
    void onGodotStopped();
    void onGodotError(const QString& msg);
    void onGodotConnected();
    void onGodotDisconnected();

    void onCameraUpdated(double x, double y, double z,
                         double rx, double ry, double rz);
    void onCustomEvent(const QString& event, const QJsonObject& data);

private:
    void setupToolBar();
    void setupDockPanel();
    void updateActions();

    void sendPickerCommand(const QString& cmd, const QJsonObject& payload);
    void showSpawnMenu(const QJsonObject& data);
    void showObjectMenu(const QJsonObject& data);
    QListWidgetItem* findCubeItem(const QString& path);

    GodotWidget*  m_godotWidget = nullptr;

    QLineEdit* m_projectEdit = nullptr;
    QLineEdit* m_sceneEdit   = nullptr;

    QLabel*      m_bridgeStatusLabel = nullptr;
    QLabel*      m_camPosLabel       = nullptr;
    QLabel*      m_camRotLabel       = nullptr;
    QLabel*      m_customEventLabel  = nullptr;
    QListWidget* m_cubeList          = nullptr;

    QVector3D m_previewPos;
    QVector3D m_previewRot;
    QString   m_pendingCubeName;

    bool m_running = false;
};
