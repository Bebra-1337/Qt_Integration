#pragma once
// QtBridge.h — Godot-сторона моста. Синглтон-нода, добавляется как Autoload.
//
// Добавь в Project Settings -> Autoload:
//   Path: res://autoload/QtBridge.tscn  (сцена с этой нодой)
//   Name: QtBridge
//
// Использование из любого C++ класса в движке:
//   QtBridge* bridge = QtBridge::get_singleton();
//   if (bridge && bridge->is_connected_to_qt()) {
//       Dictionary data;
//       data["hp"] = 42;
//       bridge->send_event("player_hp_changed", data);
//   }

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

namespace godot {

class QtBridge : public Node {
    GDCLASS(QtBridge, Node)

public:
    QtBridge();
    ~QtBridge() override;

    // Синглтон-доступ — QtBridge::get_singleton()
    static QtBridge* get_singleton() { return s_singleton; }

    void _process(double delta) override;

    // ── API для использования из других C++ классов ───────────────────────

    // Отправить произвольное событие в Qt
    void send_event(const String& event_name, const Dictionary& data);

    // Отправить сырой JSON (для продвинутых случаев)
    void send_raw(const String& json);

    bool is_connected_to_qt() const { return m_connected; }

    // ── Сигналы (доступны из GDScript если нужно) ─────────────────────────
    // emit_signal("command_received", cmd, payload)

protected:
    static void _bind_methods();

private:
    void try_connect();
    void poll_incoming();
    void handle_command(const String& json);
    void do_disconnect();

    int    m_socket           = -1;
    bool   m_connected        = false;
    double m_reconnect_timer  = 0.0;
    String m_read_buf;

    static QtBridge*      s_singleton;
    static constexpr int    PORT               = 47890;
    static constexpr double RECONNECT_INTERVAL = 2.0;
};

} // namespace godot
