#include "QtBridge.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/scene_tree.hpp>

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>

namespace godot {

QtBridge* QtBridge::s_singleton = nullptr;

QtBridge::QtBridge() {
    s_singleton = this;
}

QtBridge::~QtBridge() {
    do_disconnect();
    if (s_singleton == this) s_singleton = nullptr;
}

void QtBridge::_bind_methods() {
    ClassDB::bind_method(D_METHOD("send_event", "event_name", "data"),
                         &QtBridge::send_event);
    ClassDB::bind_method(D_METHOD("send_raw", "json"),
                         &QtBridge::send_raw);
    ClassDB::bind_method(D_METHOD("is_connected_to_qt"),
                         &QtBridge::is_connected_to_qt);

    ADD_SIGNAL(MethodInfo("command_received",
        PropertyInfo(Variant::STRING,     "cmd"),
        PropertyInfo(Variant::DICTIONARY, "payload")));
}

// ─── _process ────────────────────────────────────────────────────────────────

void QtBridge::_process(double delta) {
    if (!m_connected) {
        m_reconnect_timer -= delta;
        if (m_reconnect_timer <= 0.0) {
            try_connect();
            m_reconnect_timer = RECONNECT_INTERVAL;
        }
        return;
    }
    poll_incoming();
}

// ─── try_connect ─────────────────────────────────────────────────────────────

void QtBridge::try_connect() {
    m_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket < 0) return;

    // Неблокирующий — не фризим движок
    ::fcntl(m_socket, F_SETFL, O_NONBLOCK);

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    ::connect(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    // select с таймаутом 100ms
    fd_set wfds; FD_ZERO(&wfds); FD_SET(m_socket, &wfds);
    timeval tv{0, 100000};

    if (::select(m_socket + 1, nullptr, &wfds, nullptr, &tv) <= 0) {
        do_disconnect(); return;
    }

    int err = 0; socklen_t len = sizeof(err);
    ::getsockopt(m_socket, SOL_SOCKET, SO_ERROR, &err, &len);
    if (err != 0) { do_disconnect(); return; }

    m_connected = true;
    UtilityFunctions::print("[QtBridge] Connected to Qt on port 47890");

    // Приветствие
    send_raw("{\"type\":\"connected\",\"engine\":\"godot4_cpp\"}\n");
}

// ─── send_event ──────────────────────────────────────────────────────────────

void QtBridge::send_event(const String& event_name, const Dictionary& data) {
    if (!m_connected) return;
    Dictionary msg;
    msg["type"]  = "custom_event";
    msg["event"] = event_name;
    msg["data"]  = data;
    send_raw(JSON::stringify(msg) + "\n");
}

void QtBridge::send_raw(const String& json) {
    if (m_socket < 0) return;
    const CharString cs = json.utf8();
    if (::write(m_socket, cs.get_data(), cs.length()) < 0) {
        UtilityFunctions::printerr("[QtBridge] Write failed");
        do_disconnect();
    }
}

// ─── poll_incoming ───────────────────────────────────────────────────────────

void QtBridge::poll_incoming() {
    char buf[4096];
    const ssize_t n = ::read(m_socket, buf, sizeof(buf) - 1);

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;
        do_disconnect(); return;
    }
    if (n == 0) { do_disconnect(); return; }

    buf[n] = '\0';
    m_read_buf += String::utf8(buf, static_cast<int>(n));

    while (true) {
        const int64_t nl = m_read_buf.find("\n");
        if (nl < 0) break;
        const String line = m_read_buf.substr(0, nl).strip_edges();
        m_read_buf = m_read_buf.substr(nl + 1);
        if (!line.is_empty()) handle_command(line);
    }
}

// ─── handle_command ──────────────────────────────────────────────────────────

void QtBridge::handle_command(const String& json_str) {
    Variant parsed = JSON::parse_string(json_str);
    if (parsed.get_type() != Variant::DICTIONARY) return;

    const Dictionary msg = parsed;
    const String cmd = msg.get("cmd", "");

    if (cmd == "change_scene") {
        const String path = msg.get("path", "");
        if (!path.is_empty())
            get_tree()->change_scene_to_file(path);

    } else if (cmd == "set_paused") {
        get_tree()->set_pause(static_cast<bool>(msg.get("paused", false)));

    } else if (cmd == "call_method") {
        const String node_path = msg.get("node", "");
        const String method    = msg.get("method", "");
        Node* node = get_node_or_null(NodePath(node_path));
        if (node && !method.is_empty())
            node->call(method);  // args можно добавить при необходимости

    } else if (cmd == "set_var") {
        const String node_path = msg.get("node", "");
        const String var_name  = msg.get("var", "");
        Node* node = get_node_or_null(NodePath(node_path));
        if (node && !var_name.is_empty())
            node->set(var_name, msg.get("value", Variant()));

    } else if (cmd == "quit") {
        get_tree()->quit();

    } else {
        // Пробрасываем неизвестные команды через сигнал
        emit_signal("command_received", cmd, msg);
    }
}

void QtBridge::do_disconnect() {
    if (m_socket >= 0) { ::close(m_socket); m_socket = -1; }
    m_connected = false;
}

} // namespace godot
