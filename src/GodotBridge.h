#pragma once
// GodotBridge.h — TCP сервер для общения Qt <-> Godot C++ extension
//
// Qt поднимает QTcpServer на порту 47890.
// Godot C++ (QtBridge.cpp в extension_src) подключается как клиент.
//
// Протокол: JSON строки разделённые '\n'
// Qt  → Godot: {"cmd":"change_scene","path":"res://scenes/Game.tscn"}
// Godot → Qt:  {"type":"custom_event","event":"camera_update","data":{...}}

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonObject>
#include <QVariantList>

class GodotBridge : public QObject {
    Q_OBJECT

public:
    enum class Cmd {
        ChangeScene,
        Resize,
        Pause,
        Resume,
        CallMethod,
        SetVar,
        Quit,
    };

    explicit GodotBridge(QObject* parent = nullptr);
    ~GodotBridge() override;

    bool startServer(quint16 port = 47890);
    bool isGodotConnected() const { return m_socket != nullptr; }

    // Отправить команду в Godot
    bool sendCommand(Cmd cmd, const QVariant& payload = QVariant());

    // Вызвать метод GDScript/C++ узла
    bool callNodeMethod(const QString& nodePath,
                        const QString& method,
                        const QVariantList& args = {});

    // Установить переменную узла
    bool setNodeVar(const QString& nodePath,
                    const QString& varName,
                    const QVariant& value);

    quint16 port() const { return m_port; }

signals:
    void godotConnected();
    void godotDisconnected();
    void messageReceived(const QJsonObject& msg);

    // Удобные специализированные сигналы
    void cameraUpdated(double x, double y, double z,
                       double rx, double ry, double rz);
    void customEventReceived(const QString& event, const QJsonObject& data);

private slots:
    void onNewConnection();
    void onDisconnected();
    void onReadyRead();

private:
    bool sendJson(const QJsonObject& obj);
    void dispatchMessage(const QJsonObject& msg);

    QTcpServer* m_server  = nullptr;
    QTcpSocket* m_socket  = nullptr;
    quint16     m_port    = 47890;
    QString     m_readBuf;
};
