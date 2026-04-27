#include "MainWindow.h"
#include "GodotWidget.h"
#include "GodotBridge.h"
#include "XYZDialog.h"

#include <QToolBar>
#include <QAction>
#include <QLineEdit>
#include <QLabel>
#include <QDockWidget>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QSettings>
#include <QStatusBar>
#include <QMenu>
#include <QInputDialog>
#include <QJsonDocument>
#include <QCursor>

Q_DECLARE_METATYPE(QJsonObject)

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Godot Qt Host");
    resize(1280, 720);

    m_godotWidget = new GodotWidget(this);
    setCentralWidget(m_godotWidget);

    connect(m_godotWidget, &GodotWidget::godotReady,    this, &MainWindow::onGodotReady);
    connect(m_godotWidget, &GodotWidget::godotStopped,  this, &MainWindow::onGodotStopped);
    connect(m_godotWidget, &GodotWidget::godotError,    this, &MainWindow::onGodotError);

    GodotBridge* bridge = m_godotWidget->bridge();
    connect(bridge, &GodotBridge::godotConnected,      this, &MainWindow::onGodotConnected);
    connect(bridge, &GodotBridge::godotDisconnected,   this, &MainWindow::onGodotDisconnected);
    connect(bridge, &GodotBridge::cameraUpdated,       this, &MainWindow::onCameraUpdated);
    connect(bridge, &GodotBridge::customEventReceived, this, &MainWindow::onCustomEvent);

    setupToolBar();
    setupDockPanel();
    updateActions();

    QSettings s;
    m_projectEdit->setText(s.value("lastProject").toString());
    m_sceneEdit->setText(s.value("lastScene").toString());
}

MainWindow::~MainWindow() = default;

// ─── sendPickerCommand ────────────────────────────────────────────────────────

void MainWindow::sendPickerCommand(const QString& cmd, const QJsonObject& payload) {
    QJsonObject msg;
    msg["cmd"]     = cmd;
    msg["payload"] = payload;
    m_godotWidget->bridge()->sendRaw(
        QJsonDocument(msg).toJson(QJsonDocument::Compact) + "\n");
}

// ─── showSpawnMenu ────────────────────────────────────────────────────────────

void MainWindow::showSpawnMenu(const QJsonObject& data) {
    m_previewPos = {float(data["spawn_x"].toDouble()),
                    float(data["spawn_y"].toDouble()),
                    float(data["spawn_z"].toDouble())};
    m_previewRot = {0, 0, 0};
    m_pendingCubeName = QString("Куб_%1").arg(data["next_id"].toInt());

    QMenu menu;
    auto* title = menu.addAction("Новый куб");
    title->setEnabled(false);
    QFont f = title->font(); f.setBold(true); title->setFont(f);
    menu.addSeparator();

    menu.addAction("Переместить");
    menu.addAction("Повернуть");
    menu.addAction("Переименовать");
    menu.addSeparator();
    menu.addAction("Создать");
    menu.addAction("Отмена");

    QAction* chosen = menu.exec(QCursor::pos());
    if (!chosen) {
        sendPickerCommand("cancel_spawn", {});
        return;
    }

    const QString btn = chosen->text();

    if (btn == "Переместить") {
        XYZDialog dlg("Переместить", -10.0, 10.0, 0.1, m_previewPos, this);
        connect(&dlg, &XYZDialog::valueChanged, this, [this](QVector3D v) {
            QJsonObject p; p["x"]=v.x(); p["y"]=v.y(); p["z"]=v.z();
            sendPickerCommand("move_preview", p);
        });
        const QVector3D saved = m_previewPos;
        if (dlg.exec() == QDialog::Accepted) {
            m_previewPos = dlg.value();
        } else {
            QJsonObject p; p["x"]=saved.x(); p["y"]=saved.y(); p["z"]=saved.z();
            sendPickerCommand("move_preview", p);
        }
        showSpawnMenu(data); // переоткрываем меню

    } else if (btn == "Повернуть") {
        XYZDialog dlg("Повернуть", -180.0, 180.0, 1.0, m_previewRot, this);
        connect(&dlg, &XYZDialog::valueChanged, this, [this](QVector3D v) {
            QJsonObject p; p["x"]=v.x(); p["y"]=v.y(); p["z"]=v.z();
            sendPickerCommand("rotate_preview", p);
        });
        const QVector3D saved = m_previewRot;
        if (dlg.exec() == QDialog::Accepted) {
            m_previewRot = dlg.value();
        } else {
            QJsonObject p; p["x"]=saved.x(); p["y"]=saved.y(); p["z"]=saved.z();
            sendPickerCommand("rotate_preview", p);
        }
        showSpawnMenu(data); // переоткрываем меню

    } else if (btn == "Переименовать") {
        bool ok = false;
        const QString name = QInputDialog::getText(
            this, "Переименовать", "Имя куба:", QLineEdit::Normal, m_pendingCubeName, &ok);
        if (ok && !name.isEmpty()) m_pendingCubeName = name;
        showSpawnMenu(data); // переоткрываем меню

    } else if (btn == "Создать") {
        QJsonObject p; p["name"] = m_pendingCubeName;
        sendPickerCommand("confirm_spawn", p);

    } else if (btn == "Отмена") {
        sendPickerCommand("cancel_spawn", {});
    }
}

// ─── showObjectMenu ───────────────────────────────────────────────────────────

void MainWindow::showObjectMenu(const QJsonObject& data) {
    const QString path = data["path"].toString();
    const QString name = data["name"].toString();

    QVector3D curPos(float(data["world_x"].toDouble()),
                     float(data["world_y"].toDouble()),
                     float(data["world_z"].toDouble()));
    QVector3D curRot(float(data["rot_x"].toDouble()),
                     float(data["rot_y"].toDouble()),
                     float(data["rot_z"].toDouble()));

    QMenu menu;
    auto* title = menu.addAction(name);
    title->setEnabled(false);
    QFont f = title->font(); f.setBold(true); title->setFont(f);

    auto addInfo = [&](const QString& label, const QString& val) {
        menu.addAction(QString("%1 %2").arg(label, -8).arg(val))->setEnabled(false);
    };
    addInfo("Тип:",  data["type"].toString());
    addInfo("Путь:", path);
    menu.addSeparator();

    menu.addAction("Переместить");
    menu.addAction("Повернуть");
    menu.addAction("Переименовать");
    menu.addSeparator();
    const bool inAlert = data["alert"].toBool();
    menu.addAction(inAlert ? "Норма" : "Тревога");
    menu.addAction("Удалить");

    QAction* chosen = menu.exec(QCursor::pos());
    if (!chosen) return;

    const QString btn = chosen->text();

    if (btn == "Переместить") {
        XYZDialog dlg("Переместить", -10.0, 10.0, 0.1, curPos, this);
        connect(&dlg, &XYZDialog::valueChanged, this, [this, path](QVector3D v) {
            QJsonObject p; p["path"]=path; p["x"]=v.x(); p["y"]=v.y(); p["z"]=v.z();
            sendPickerCommand("move_cube", p);
        });
        if (dlg.exec() != QDialog::Accepted) {
            QJsonObject p; p["path"]=path; p["x"]=curPos.x(); p["y"]=curPos.y(); p["z"]=curPos.z();
            sendPickerCommand("move_cube", p);
        }

    } else if (btn == "Повернуть") {
        XYZDialog dlg("Повернуть", -180.0, 180.0, 1.0, curRot, this);
        connect(&dlg, &XYZDialog::valueChanged, this, [this, path](QVector3D v) {
            QJsonObject p; p["path"]=path; p["x"]=v.x(); p["y"]=v.y(); p["z"]=v.z();
            sendPickerCommand("rotate_cube", p);
        });
        if (dlg.exec() != QDialog::Accepted) {
            QJsonObject p; p["path"]=path; p["x"]=curRot.x(); p["y"]=curRot.y(); p["z"]=curRot.z();
            sendPickerCommand("rotate_cube", p);
        }

    } else if (btn == "Переименовать") {
        bool ok = false;
        const QString newName = QInputDialog::getText(
            this, "Переименовать", "Новое имя:", QLineEdit::Normal, name, &ok);
        if (ok && !newName.isEmpty()) {
            QJsonObject p; p["path"]=path; p["new_name"]=newName;
            sendPickerCommand("rename_cube", p);
        }

    } else if (btn == "Тревога" || btn == "Норма") {
        QJsonObject p; p["path"]=path; p["alert"]=!inAlert;
        sendPickerCommand("set_alert", p);

    } else if (btn == "Удалить") {
        QJsonObject p; p["path"]=path;
        sendPickerCommand("delete_cube", p);
    }
}

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

void MainWindow::setupDockPanel() {
    auto* dock = new QDockWidget("Engine Data", this);
    dock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    dock->setMinimumWidth(240);
    dock->setMaximumWidth(240);

    auto* container = new QWidget;
    auto* form      = new QFormLayout(container);
    form->setSpacing(8);
    form->setContentsMargins(12, 12, 12, 12);

    m_bridgeStatusLabel = new QLabel("⬤  Disconnected");
    m_bridgeStatusLabel->setStyleSheet("color: #888;");
    form->addRow("Bridge:", m_bridgeStatusLabel);
    form->addRow(new QLabel);

    form->addRow(new QLabel("<b>Camera</b>"));
    m_camPosLabel = new QLabel("—");
    m_camPosLabel->setFont(QFont("Monospace", 9));
    form->addRow("Position:", m_camPosLabel);

    m_camRotLabel = new QLabel("—");
    m_camRotLabel->setFont(QFont("Monospace", 9));
    form->addRow("Rotation:", m_camRotLabel);

    form->addRow(new QLabel);
    form->addRow(new QLabel("<b>Last Event</b>"));
    m_customEventLabel = new QLabel("—");
    m_customEventLabel->setWordWrap(true);
    form->addRow(m_customEventLabel);

    form->addRow(new QLabel);
    form->addRow(new QLabel("<b>Кубы</b>"));
    m_cubeList = new QListWidget;
    m_cubeList->setMaximumHeight(200);
    m_cubeList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_cubeList, &QListWidget::customContextMenuRequested,
            this, [this](const QPoint&) {
        QListWidgetItem* item = m_cubeList->currentItem();
        if (!item) return;
        showObjectMenu(item->data(Qt::UserRole + 1).value<QJsonObject>());
    });
    form->addRow(m_cubeList);

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
    if (!m_godotWidget->startGodot(path, m_sceneEdit->text().trimmed()))
        QMessageBox::critical(this, "Failed", "Could not start Godot process.");
}

void MainWindow::onStopGodot() { m_godotWidget->stopGodot(); }

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
    m_cubeList->clear();
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

void MainWindow::onCameraUpdated(double x, double y, double z,
                                  double rx, double ry, double rz)
{
    m_camPosLabel->setText(
        QString("X: %1\nY: %2\nZ: %3").arg(x,0,'f',3).arg(y,0,'f',3).arg(z,0,'f',3));
    m_camRotLabel->setText(
        QString("X: %1°\nY: %2°\nZ: %3°").arg(rx,0,'f',1).arg(ry,0,'f',1).arg(rz,0,'f',1));
}

void MainWindow::onCustomEvent(const QString& event, const QJsonObject& data) {
    const QString dataStr = QString::fromUtf8(
        QJsonDocument(data).toJson(QJsonDocument::Compact));
    m_customEventLabel->setText(
        QString("<b>%1</b><br><small>%2</small>").arg(event, dataStr));

    if (event == "spawn_preview") {
        showSpawnMenu(data);
    } else if (event == "object_picked" && data["hit"].toBool()) {
        // Обновляем кэш данных куба в списке
        if (QListWidgetItem* item = findCubeItem(data["path"].toString()))
            item->setData(Qt::UserRole + 1, QVariant::fromValue(data));
        showObjectMenu(data);
    } else if (event == "cube_spawned") {
        const QString path = data["path"].toString();
        auto* item = new QListWidgetItem(data["name"].toString());
        item->setData(Qt::UserRole, path);
        // Собираем начальные данные куба
        QJsonObject cubeData = data;
        cubeData["alert"] = false;
        cubeData["type"]  = "StaticBody3D";
        cubeData["world_x"] = data["x"]; cubeData["world_y"] = data["y"]; cubeData["world_z"] = data["z"];
        cubeData["rot_x"] = 0.0; cubeData["rot_y"] = 0.0; cubeData["rot_z"] = 0.0;
        item->setData(Qt::UserRole + 1, QVariant::fromValue(cubeData));
        item->setForeground(QColor("#4caf50"));
        m_cubeList->addItem(item);
    } else if (event == "cube_deleted") {
        delete findCubeItem(data["path"].toString());
    } else if (event == "cube_renamed") {
        if (QListWidgetItem* item = findCubeItem(data["path"].toString())) {
            item->setText(data["new_name"].toString());
            item->setData(Qt::UserRole, data["new_path"].toString());
            QJsonObject cubeData = item->data(Qt::UserRole + 1).value<QJsonObject>();
            cubeData["name"] = data["new_name"].toString();
            cubeData["path"] = data["new_path"].toString();
            item->setData(Qt::UserRole + 1, QVariant::fromValue(cubeData));
        }
    } else if (event == "alert_changed") {
        if (QListWidgetItem* item = findCubeItem(data["path"].toString())) {
            const bool alert = data["alert"].toBool();
            item->setForeground(alert ? QColor("#f44336") : QColor("#4caf50"));
            QJsonObject cubeData = item->data(Qt::UserRole + 1).value<QJsonObject>();
            cubeData["alert"] = alert;
            item->setData(Qt::UserRole + 1, QVariant::fromValue(cubeData));
        }
    }
}

QListWidgetItem* MainWindow::findCubeItem(const QString& path) {
    for (int i = 0; i < m_cubeList->count(); ++i) {
        QListWidgetItem* item = m_cubeList->item(i);
        if (item->data(Qt::UserRole).toString() == path)
            return item;
    }
    return nullptr;
}

void MainWindow::updateActions() {
    if (auto* a = findChild<QAction*>("startAction")) a->setEnabled(!m_running);
    if (auto* a = findChild<QAction*>("stopAction"))  a->setEnabled(m_running);
}

void MainWindow::closeEvent(QCloseEvent* e) {
    if (m_running) {
        if (QMessageBox::question(this, "Quit", "Stop Godot and quit?")
                != QMessageBox::Yes) { e->ignore(); return; }
        m_godotWidget->stopGodot();
    }
    QMainWindow::closeEvent(e);
}
