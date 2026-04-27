#include "GodotBridge.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>

GodotBridge::GodotBridge(QObject* parent) : QObject(parent) {}

GodotBridge::~GodotBridge() {
    if (m_socket && m_socket->isOpen()) {
        sendCommand(Cmd::Quit);
        m_socket->flush();
    }
}

// ─── startServer ─────────────────────────────────────────────────────────────

bool GodotBridge::startServer(quint16 port) {
    m_port   = port;
    m_server = new QTcpServer(this);

    if (!m_server->listen(QHostAddress::LocalHost, port)) {
        qCritical() << "[GodotBridge] Cannot listen on port" << port
                    << ":" << m_server->errorString();
        return false;
    }

    connect(m_server, &QTcpServer::newConnection,
            this, &GodotBridge::onNewConnection);

    qDebug() << "[GodotBridge] TCP server listening on port" << port;
    return true;
}

// ─── Connection handling ──────────────────────────────────────────────────────

void GodotBridge::onNewConnection() {
    // Принимаем только одно соединение (один Godot процесс)
    if (m_socket) {
        qWarning() << "[GodotBridge] Already have a connection, rejecting new one";
        m_server->nextPendingConnection()->deleteLater();
        return;
    }

    m_socket = m_server->nextPendingConnection();
    connect(m_socket, &QTcpSocket::disconnected, this, &GodotBridge::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead,    this, &GodotBridge::onReadyRead);

    qDebug() << "[GodotBridge] Godot connected from"
             << m_socket->peerAddress().toString();
    emit godotConnected();
}

void GodotBridge::onDisconnected() {
    qDebug() << "[GodotBridge] Godot disconnected";
    m_socket->deleteLater();
    m_socket = nullptr;
    m_readBuf.clear();
    emit godotDisconnected();
}

// ─── Reading ──────────────────────────────────────────────────────────────────

void GodotBridge::onReadyRead() {
    m_readBuf += QString::fromUtf8(m_socket->readAll());

    int nl;
    while ((nl = m_readBuf.indexOf('\n')) != -1) {
        const QString line = m_readBuf.left(nl).trimmed();
        m_readBuf.remove(0, nl + 1);

        if (line.isEmpty()) continue;

        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError) {
            qWarning() << "[GodotBridge] JSON error:" << err.errorString();
            continue;
        }

        const QJsonObject msg = doc.object();
        emit messageReceived(msg);
        dispatchMessage(msg);
    }
}

// ─── Dispatch incoming messages ───────────────────────────────────────────────

void GodotBridge::dispatchMessage(const QJsonObject& msg) {
    const QString type = msg["type"].toString();

    if (type == "custom_event") {
        const QString     event = msg["event"].toString();
        const QJsonObject data  = msg["data"].toObject();

        emit customEventReceived(event, data);

        // Специальная обработка camera_update
        if (event == "camera_update") {
            const QJsonObject pos = data["pos"].toObject();
            const QJsonObject rot = data["rot"].toObject();
            emit cameraUpdated(
                pos["x"].toDouble(), pos["y"].toDouble(), pos["z"].toDouble(),
                rot["x"].toDouble(), rot["y"].toDouble(), rot["z"].toDouble()
            );
        }
    } else if (type == "connected") {
        qDebug() << "[GodotBridge] Godot engine:"
                 << msg["engine"].toString();
    }
}

// ─── Sending ──────────────────────────────────────────────────────────────────

bool GodotBridge::sendCommand(Cmd cmd, const QVariant& payload) {
    QJsonObject obj;

    switch (cmd) {
    case Cmd::ChangeScene:
        obj["cmd"]  = "change_scene";
        obj["path"] = payload.toString();
        break;
    case Cmd::Resize: {
        obj["cmd"] = "resize";
        const QStringList parts = payload.toString().split('x');
        if (parts.size() == 2) { obj["w"] = parts[0].toInt(); obj["h"] = parts[1].toInt(); }
        break;
    }
    case Cmd::Pause:
        obj["cmd"]    = "set_paused";
        obj["paused"] = true;
        break;
    case Cmd::Resume:
        obj["cmd"]    = "set_paused";
        obj["paused"] = false;
        break;
    case Cmd::Quit:
        obj["cmd"] = "quit";
        break;
    default:
        return false;
    }

    return sendJson(obj);
}

bool GodotBridge::callNodeMethod(const QString&      nodePath,
                                  const QString&      method,
                                  const QVariantList& args)
{
    QJsonObject obj;
    obj["cmd"]    = "call_method";
    obj["node"]   = nodePath;
    obj["method"] = method;

    QJsonArray jsonArgs;
    for (const auto& a : args) {
        switch (a.type()) {
        case QVariant::Int:    jsonArgs.append(a.toInt());    break;
        case QVariant::Double: jsonArgs.append(a.toDouble()); break;
        case QVariant::Bool:   jsonArgs.append(a.toBool());   break;
        default:               jsonArgs.append(a.toString()); break;
        }
    }
    obj["args"] = jsonArgs;
    return sendJson(obj);
}

bool GodotBridge::setNodeVar(const QString& nodePath,
                              const QString& varName,
                              const QVariant& value)
{
    QJsonObject obj;
    obj["cmd"]  = "set_var";
    obj["node"] = nodePath;
    obj["var"]  = varName;

    switch (value.type()) {
    case QVariant::Int:    obj["value"] = value.toInt();    break;
    case QVariant::Double: obj["value"] = value.toDouble(); break;
    case QVariant::Bool:   obj["value"] = value.toBool();   break;
    default:               obj["value"] = value.toString(); break;
    }
    return sendJson(obj);
}

bool GodotBridge::sendJson(const QJsonObject& obj) {
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "[GodotBridge] Not connected, dropping:" << obj["cmd"].toString();
        return false;
    }
    const QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
    return m_socket->write(data) == data.size();
}

bool GodotBridge::sendRaw(const QByteArray& data) {
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
        return false;
    return m_socket->write(data) == data.size();
}
