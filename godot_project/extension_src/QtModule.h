#pragma once
// QtModule.h — центральный модуль Qt интеграции.
//
// Добавь одну ноду QtModule в сцену, затем в инспекторе
// перетащи нужные ноды в слоты:
//
//  Инспектор QtModule:
//  ┌──────────────────────────────────────┐
//  │ Camera:    [Camera3D              ]  │
//  │ Tracker:   [Node3D / любой объект ]  │
//  │ Send Interval: 0.05                  │
//  └──────────────────────────────────────┘

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/input_event.hpp>

namespace godot {

class QtModule : public Node {
    GDCLASS(QtModule, Node)

public:
    QtModule();
    ~QtModule() override;

    void _ready()                          override;
    void _process(double delta)            override;
    void _input(const Ref<InputEvent>& event) override;

    // ── Слоты инспектора ─────────────────────────────────────────────────

    // Camera — источник координат для трекинга
    void     set_camera_path(const NodePath& p);
    NodePath get_camera_path() const { return m_camera_path; }

    // Tracked Node — произвольная нода чьи координаты слать в Qt
    // (если не задана — используется камера)
    void     set_tracked_path(const NodePath& p);
    NodePath get_tracked_path() const { return m_tracked_path; }

    // Интервал отправки данных в Qt (секунды)
    void   set_send_interval(double v) { m_send_interval = v; }
    double get_send_interval()   const { return m_send_interval; }

    // Включить/выключить трекинг камеры
    void set_camera_tracking_enabled(bool v) { m_camera_tracking = v; }
    bool get_camera_tracking_enabled()  const { return m_camera_tracking; }

    // Включить/выключить object picker (клик ЛКМ)
    void set_picker_enabled(bool v) { m_picker_enabled = v; }
    bool get_picker_enabled()  const { return m_picker_enabled; }

    // Включить/выключить spawn куба (СКМ)
    void set_spawner_enabled(bool v) { m_spawner_enabled = v; }
    bool get_spawner_enabled()  const { return m_spawner_enabled; }

protected:
    static void _bind_methods();

private:
    // ── NodePath слоты (сохраняются в .tscn) ─────────────────────────────
    NodePath m_camera_path;
    NodePath m_tracked_path;

    // ── Настройки ─────────────────────────────────────────────────────────
    double m_send_interval       = 0.05; // 20 fps
    bool   m_camera_tracking     = true;
    bool   m_picker_enabled      = true;
    bool   m_spawner_enabled     = true;

    // ── Резолвленные указатели (заполняются в _ready) ─────────────────────
    Camera3D* m_camera  = nullptr;
    Node3D*   m_tracked = nullptr;

    // ── Внутреннее состояние ──────────────────────────────────────────────
    double m_timer = 0.0;

    // ── Приватные методы ──────────────────────────────────────────────────
    void send_camera_data();
    void send_tracked_data();
    void handle_left_click(Vector2 screen_pos);
    void handle_middle_click(Vector2 screen_pos);
    void spawn_cube(const Vector3& pos);

    Dictionary do_raycast(Vector2 screen_pos);
};

} // namespace godot
