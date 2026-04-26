#include "MainWindow.h"
#include "GodotWidget.h"
#include "GodotBridge.h"

#include <QToolBar>
#include <QAction>
#include <QLineEdit>
#include <QLabel>
#include <QDockWidget>
#include <QFormLayout>
#include <QWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QDebug>
#include <QMenu>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Godot Qt Host");
    resize(1280, 720);

    m_godotWidget = new GodotWidget(this);
    m_objectMenu = new QMenu(this);
    setCentralWidget(m_godotWidget);

    connect(m_godotWidget, &GodotWidget::godotReady,
            this, &MainWindow::onGodotReady);
    connect(m_godotWidget, &GodotWidget::godotStopped,
            this, &MainWindow::onGodotStopped);
    connect(m_godotWidget, &GodotWidget::godotError,
            this, &MainWindow::onGodotError);

    // Bridge сигналы подключаем сразу — Godot может подключиться
    // раньше чем сработает onGodotReady()
    GodotBridge* bridge = m_godotWidget->bridge();
    connect(bridge, &GodotBridge::godotConnected,
            this,   &MainWindow::onGodotConnected);
    connect(bridge, &GodotBridge::godotDisconnected,
            this,   &MainWindow::onGodotDisconnected);
    connect(bridge, &GodotBridge::cameraUpdated,
            this,   &MainWindow::onCameraUpdated);
    connect(bridge, &GodotBridge::customEventReceived,
            this,   &MainWindow::onCustomEvent);

    setupToolBar();
    setupDockPanel();
    updateActions();

    QSettings s;
    m_projectEdit->setText(s.value("lastProject").toString());
    m_sceneEdit->setText(s.value("lastScene").toString());
}

MainWindow::~MainWindow() = default;

// ─── setupToolBar ─────────────────────────────────────────────────────────────

void MainWindow::setupToolBar() {
    auto* tb = addToolBar("Controls");
    tb->setMovable(false);

    tb->addWidget(new QLabel(" Project: "));
    m_projectEdit = new QLineEdit;
    m_projectEdit->setPlaceholderText("/home/user/my_godot_project");
    m_projectEdit->setMinimumWidth(300);
    tb->addWidget(m_projectEdit);

    auto* browse = tb->addAction("📁");
    connect(browse, &QAction::triggered, this, [this]() {
        const QString d = QFileDialog::getExistingDirectory(
            this, "Select Godot Project", m_projectEdit->text());
        if (!d.isEmpty()) m_projectEdit->setText(d);
    });

    tb->addSeparator();
    tb->addWidget(new QLabel(" Scene: "));
    m_sceneEdit = new QLineEdit;
    m_sceneEdit->setPlaceholderText("res://scenes/Main.tscn  (optional)");
    m_sceneEdit->setMinimumWidth(220);
    tb->addWidget(m_sceneEdit);

    tb->addSeparator();

    auto* startAct = tb->addAction("▶  Start");
    startAct->setObjectName("startAction");
    connect(startAct, &QAction::triggered, this, &MainWindow::onStartGodot);

    auto* stopAct = tb->addAction("■  Stop");
    stopAct->setObjectName("stopAction");
    connect(stopAct, &QAction::triggered, this, &MainWindow::onStopGodot);
}

// ─── setupDockPanel ───────────────────────────────────────────────────────────
// Правая панель показывает данные, которые Godot шлёт через мост в реальном времени

void MainWindow::setupDockPanel() {
    auto* dock = new QDockWidget("Engine Data", this);
    dock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    dock->setMinimumWidth(240);
    dock->setMaximumWidth(240);

    auto* container = new QWidget;
    auto* form      = new QFormLayout(container);
    form->setSpacing(8);
    form->setContentsMargins(12, 12, 12, 12);

    // ── Bridge статус ─────────────────────────────────────────────────────
    m_bridgeStatusLabel = new QLabel("⬤  Disconnected");
    m_bridgeStatusLabel->setStyleSheet("color: #888;");
    form->addRow("Bridge:", m_bridgeStatusLabel);

    form->addRow(new QLabel); // разделитель

    // ── Камера ────────────────────────────────────────────────────────────
    form->addRow(new QLabel("<b>Camera</b>"));

    m_camPosLabel = new QLabel("—");
    m_camPosLabel->setFont(QFont("Monospace", 9));
    form->addRow("Position:", m_camPosLabel);

    m_camRotLabel = new QLabel("—");
    m_camRotLabel->setFont(QFont("Monospace", 9));
    form->addRow("Rotation:", m_camRotLabel);

    form->addRow(new QLabel); // разделитель

    form->addRow(new QLabel); // разделитель
    form->addRow(new QLabel("<b>Selected Object</b>"));

    // ── Произвольные события ──────────────────────────────────────────────
    form->addRow(new QLabel("<b>Last Event</b>"));
    m_customEventLabel = new QLabel("—");
    m_customEventLabel->setWordWrap(true);
    form->addRow(m_customEventLabel);

    dock->setWidget(container);
    addDockWidget(Qt::RightDockWidgetArea, dock);
}

// ─── Slots ────────────────────────────────────────────────────────────────────

void MainWindow::onStartGodot() {
    const QString path = m_projectEdit->text().trimmed();
    if (path.isEmpty()) {
        QMessageBox::warning(this, "No Project", "Specify Godot project path.");
        return;
    }

    QSettings s;
    s.setValue("lastProject", path);
    s.setValue("lastScene",   m_sceneEdit->text().trimmed());

    if (!m_godotWidget->startGodot(path, m_sceneEdit->text().trimmed())) {
        QMessageBox::critical(this, "Failed", "Could not start Godot process.");
    }
}

void MainWindow::onStopGodot() {
    m_godotWidget->stopGodot();
}

void MainWindow::onGodotReady() {
    m_running = true;
    statusBar()->showMessage("Godot running", 3000);
    updateActions();
}

void MainWindow::onGodotStopped() {
    m_running = false;
    m_bridgeStatusLabel->setText("⬤  Disconnected");
    m_bridgeStatusLabel->setStyleSheet("color: #888;");
    m_camPosLabel->setText("—");
    m_camRotLabel->setText("—");
    updateActions();
}

void MainWindow::onGodotError(const QString& msg) {
    statusBar()->showMessage("Error: " + msg, 5000);
    QMessageBox::critical(this, "Godot Error", msg);
}

void MainWindow::onGodotConnected() {
    m_bridgeStatusLabel->setText("⬤  Connected");
    m_bridgeStatusLabel->setStyleSheet("color: #4caf50; font-weight: bold;");
    statusBar()->showMessage("Bridge connected", 2000);
}

void MainWindow::onGodotDisconnected() {
    m_bridgeStatusLabel->setText("⬤  Disconnected");
    m_bridgeStatusLabel->setStyleSheet("color: #888;");
}

void MainWindow::onCameraUpdated(double x,  double y,  double z,
                                  double rx, double ry, double rz)
{
    m_camPosLabel->setText(
        QString("X: %1\nY: %2\nZ: %3")
            .arg(x,  0, 'f', 3)
            .arg(y,  0, 'f', 3)
            .arg(z,  0, 'f', 3));

    m_camRotLabel->setText(
        QString("X: %1°\nY: %2°\nZ: %3°")
            .arg(rx, 0, 'f', 1)
            .arg(ry, 0, 'f', 1)
            .arg(rz, 0, 'f', 1));
}

void MainWindow::onCustomEvent(const QString& event, const QJsonObject& data) {
    // Обновляем Last Event как раньше
    const QString dataStr = QString::fromUtf8(
        QJsonDocument(data).toJson(QJsonDocument::Compact));
    m_customEventLabel->setText(
        QString("<b>%1</b><br><small>%2</small>").arg(event, dataStr));

    // Обработка клика по объекту
    if (event == "object_picked") {
        m_objectMenu->clear();

        if (!data["hit"].toBool()) {
            return; // клик в пустоту — не показываем
        }

        // Заголовок
        auto* title = new QAction(data["name"].toString(), this);
        title->setEnabled(false);
        QFont f = title->font();
        f.setBold(true);
        title->setFont(f);
        m_objectMenu->addAction(title);
        m_objectMenu->addSeparator();

        // Информация
        auto addRow = [&](const QString& label, const QString& value) {
            m_objectMenu->addAction(
                            QString("%1  %2").arg(label, -10).arg(value)
                            )->setEnabled(false);
        };

        addRow("Type:",  data["type"].toString());
        addRow("Path:",  data["path"].toString());

        addRow("Hit X:", QString::number(data["hit_x"].toDouble(), 'f', 3));
        addRow("Hit Y:", QString::number(data["hit_y"].toDouble(), 'f', 3));
        addRow("Hit Z:", QString::number(data["hit_z"].toDouble(), 'f', 3));

        if (data.contains("world_x")) {
            m_objectMenu->addSeparator();
            addRow("World X:", QString::number(data["world_x"].toDouble(), 'f', 3));
            addRow("World Y:", QString::number(data["world_y"].toDouble(), 'f', 3));
            addRow("World Z:", QString::number(data["world_z"].toDouble(), 'f', 3));
        }

        // Показать под курсором
        m_objectMenu->popup(QCursor::pos());
    }
}

void MainWindow::updateActions() {
    if (auto* a = findChild<QAction*>("startAction")) a->setEnabled(!m_running);
    if (auto* a = findChild<QAction*>("stopAction"))  a->setEnabled(m_running);
}

void MainWindow::closeEvent(QCloseEvent* e) {
    if (m_running) {
        if (QMessageBox::question(this, "Quit", "Stop Godot and quit?")
                != QMessageBox::Yes) {
            e->ignore();
            return;
        }
        m_godotWidget->stopGodot();
    }
    QMainWindow::closeEvent(e);
}
