#include "CameraTracker.h"
#include "QtBridge.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/scene_tree.hpp>

namespace godot {

void CameraTracker::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_send_interval", "interval"),
                         &CameraTracker::set_send_interval);
    ClassDB::bind_method(D_METHOD("get_send_interval"),
                         &CameraTracker::get_send_interval);

    ADD_PROPERTY(
        PropertyInfo(Variant::FLOAT, "send_interval"),
        "set_send_interval", "get_send_interval");
}

void CameraTracker::_process(double delta) {
    m_timer += delta;
    if (m_timer < m_interval) return;
    m_timer = 0.0;

    // Берём синглтон моста
    QtBridge* bridge = QtBridge::get_singleton();
    if (!bridge || !bridge->is_connected_to_qt()) return;

    // Ищем камеру в текущем вьюпорте
    Camera3D* cam = get_viewport()->get_camera_3d();
    if (!cam) return;

    const Vector3 pos = cam->get_global_position();
    const Vector3 rot = cam->get_global_rotation_degrees();

    // Собираем словарь и шлём
    Dictionary p, r, data;
    p["x"] = pos.x; p["y"] = pos.y; p["z"] = pos.z;
    r["x"] = rot.x; r["y"] = rot.y; r["z"] = rot.z;
    data["pos"] = p;
    data["rot"] = r;

    bridge->send_event("camera_update", data);
}

} // namespace godot
