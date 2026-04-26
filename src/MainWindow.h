#pragma once
#include <QMainWindow>
#include <QJsonObject>

class GodotWidget;
class QLabel;
class QLineEdit;
class QDockWidget;
class QFormLayout;

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

    // ── Обработчики данных из Godot ──────────────────────────────────────
    void onCameraUpdated(double x, double y, double z,
                         double rx, double ry, double rz);
    void onCustomEvent(const QString& event, const QJsonObject& data);

private:
    void setupToolBar();
    void setupDockPanel();
    void updateActions();

    GodotWidget* m_godotWidget = nullptr;

    // Toolbar
    QLineEdit* m_projectEdit = nullptr;
    QLineEdit* m_sceneEdit   = nullptr;

    // Dock panel — данные из движка
    QLabel* m_bridgeStatusLabel = nullptr;
    QLabel* m_camPosLabel       = nullptr;
    QLabel* m_camRotLabel       = nullptr;
    QLabel* m_fpsLabel          = nullptr;
    QLabel* m_customEventLabel  = nullptr;

    QMenu* m_objectMenu = nullptr;

    bool m_running = false;
};
