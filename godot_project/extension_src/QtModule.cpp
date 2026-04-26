#include "QtModule.h"
#include "QtBridge.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>

namespace godot {

QtModule::QtModule() {}
QtModule::~QtModule() {}

// ─── _bind_methods ────────────────────────────────────────────────────────────

void QtModule::_bind_methods() {

    // ── camera_path ───────────────────────────────────────────────────────
    ClassDB::bind_method(D_METHOD("set_camera_path", "path"),
                         &QtModule::set_camera_path);
    ClassDB::bind_method(D_METHOD("get_camera_path"),
                         &QtModule::get_camera_path);
    ADD_PROPERTY(
        PropertyInfo(Variant::NODE_PATH, "camera",
                     PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Camera3D"),
        "set_camera_path", "get_camera_path");

    // ── tracked_path ──────────────────────────────────────────────────────
    ClassDB::bind_method(D_METHOD("set_tracked_path", "path"),
                         &QtModule::set_tracked_path);
    ClassDB::bind_method(D_METHOD("get_tracked_path"),
                         &QtModule::get_tracked_path);
    ADD_PROPERTY(
        PropertyInfo(Variant::NODE_PATH, "tracked_node",
                     PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Node3D"),
        "set_tracked_path", "get_tracked_path");

    // ── send_interval ─────────────────────────────────────────────────────
    ClassDB::bind_method(D_METHOD("set_send_interval", "interval"),
                         &QtModule::set_send_interval);
    ClassDB::bind_method(D_METHOD("get_send_interval"),
                         &QtModule::get_send_interval);
    ADD_PROPERTY(
        PropertyInfo(Variant::FLOAT, "send_interval",
                     PROPERTY_HINT_RANGE, "0.01,1.0,0.01"),
        "set_send_interval", "get_send_interval");

    // ── camera_tracking_enabled ───────────────────────────────────────────
    ClassDB::bind_method(D_METHOD("set_camera_tracking_enabled", "enabled"),
                         &QtModule::set_camera_tracking_enabled);
    ClassDB::bind_method(D_METHOD("get_camera_tracking_enabled"),
                         &QtModule::get_camera_tracking_enabled);
    ADD_PROPERTY(
        PropertyInfo(Variant::BOOL, "camera_tracking_enabled"),
        "set_camera_tracking_enabled", "get_camera_tracking_enabled");

    // ── picker_enabled ────────────────────────────────────────────────────
    ClassDB::bind_method(D_METHOD("set_picker_enabled", "enabled"),
                         &QtModule::set_picker_enabled);
    ClassDB::bind_method(D_METHOD("get_picker_enabled"),
                         &QtModule::get_picker_enabled);
    ADD_PROPERTY(
        PropertyInfo(Variant::BOOL, "picker_enabled"),
        "set_picker_enabled", "get_picker_enabled");

    // ── spawner_enabled ───────────────────────────────────────────────────
    ClassDB::bind_method(D_METHOD("set_spawner_enabled", "enabled"),
                         &QtModule::set_spawner_enabled);
    ClassDB::bind_method(D_METHOD("get_spawner_enabled"),
                         &QtModule::get_spawner_enabled);
    ADD_PROPERTY(
        PropertyInfo(Variant::BOOL, "spawner_enabled"),
        "set_spawner_enabled", "get_spawner_enabled");
}

// ─── Setters с резолвингом ────────────────────────────────────────────────────

void QtModule::set_camera_path(const NodePath& p) {
    m_camera_path = p;
    // Если нода уже в дереве — резолвим сразу
    if (is_inside_tree() && !p.is_empty())
        m_camera = get_node<Camera3D>(p);
}

void QtModule::set_tracked_path(const NodePath& p) {
    m_tracked_path = p;
    if (is_inside_tree() && !p.is_empty())
        m_tracked = get_node<Node3D>(p);
}

// ─── _ready ───────────────────────────────────────────────────────────────────

void QtModule::_ready() {
    // Резолвим NodePath → реальные указатели
    if (!m_camera_path.is_empty()) {
        m_camera = get_node<Camera3D>(m_camera_path);
        if (!m_camera)
            UtilityFunctions::printerr("[QtModule] Camera not found: ",
                                       m_camera_path);
    }

    if (!m_tracked_path.is_empty()) {
        m_tracked = get_node<Node3D>(m_tracked_path);
        if (!m_tracked)
            UtilityFunctions::printerr("[QtModule] Tracked node not found: ",
                                       m_tracked_path);
    }

    // Если камера не задана — пробуем найти в вьюпорте автоматически
    if (!m_camera)
        m_camera = get_viewport()->get_camera_3d();

    UtilityFunctions::print("[QtModule] Ready. Camera: ",
                            m_camera ? String(m_camera->get_name()) : "auto",
                            " | Tracked: ",
                            m_tracked ? String(m_tracked->get_name()) : "none");
}

// ─── _process ─────────────────────────────────────────────────────────────────

void QtModule::_process(double delta) {
    QtBridge* bridge = QtBridge::get_singleton();
    if (!bridge || !bridge->is_connected_to_qt()) return;

    m_timer += delta;
    if (m_timer < m_send_interval) return;
    m_timer = 0.0;

    if (m_camera_tracking && m_camera)
        send_camera_data();

    if (m_tracked)
        send_tracked_data();
}

// ─── _input ───────────────────────────────────────────────────────────────────

void QtModule::_input(const Ref<InputEvent>& event) {
    const auto* mb = Object::cast_to<InputEventMouseButton>(*event);
    if (!mb || !mb->is_pressed() || mb->is_double_click()) return;

    if (mb->get_button_index() == MOUSE_BUTTON_RIGHT && m_picker_enabled)
        handle_left_click(mb->get_position());

    if (mb->get_button_index() == MOUSE_BUTTON_MIDDLE && m_spawner_enabled)
        handle_middle_click(mb->get_position());
}

// ─── send_camera_data ────────────────────────────────────────────────────────

void QtModule::send_camera_data() {
    const Vector3 pos = m_camera->get_global_position();
    const Vector3 rot = m_camera->get_global_rotation_degrees();

    Dictionary p, r, data;
    p["x"] = pos.x; p["y"] = pos.y; p["z"] = pos.z;
    r["x"] = rot.x; r["y"] = rot.y; r["z"] = rot.z;
    data["pos"] = p;
    data["rot"] = r;

    QtBridge::get_singleton()->send_event("camera_update", data);
}

// ─── send_tracked_data ───────────────────────────────────────────────────────

void QtModule::send_tracked_data() {
    const Vector3 pos = m_tracked->get_global_position();
    const Vector3 rot = m_tracked->get_global_rotation_degrees();

    Dictionary p, r, data;
    p["x"] = pos.x; p["y"] = pos.y; p["z"] = pos.z;
    r["x"] = rot.x; r["y"] = rot.y; r["z"] = rot.z;
    data["pos"]  = p;
    data["rot"]  = r;
    data["name"] = String(m_tracked->get_name());

    QtBridge::get_singleton()->send_event("tracked_update", data);
}

// ─── do_raycast ──────────────────────────────────────────────────────────────

Dictionary QtModule::do_raycast(Vector2 screen_pos) {
    Camera3D* cam = m_camera ? m_camera : get_viewport()->get_camera_3d();
    if (!cam) return Dictionary();

    PhysicsDirectSpaceState3D* space =
        get_viewport()->get_world_3d()->get_direct_space_state();
    if (!space) return Dictionary();

    const Vector3 origin = cam->project_ray_origin(screen_pos);
    const Vector3 end    = origin + cam->project_ray_normal(screen_pos) * 1000.0f;

    Ref<PhysicsRayQueryParameters3D> query =
        PhysicsRayQueryParameters3D::create(origin, end);
    query->set_collide_with_bodies(true);
    query->set_collide_with_areas(true);

    return space->intersect_ray(query);
}

// ─── handle_left_click ───────────────────────────────────────────────────────

void QtModule::handle_left_click(Vector2 screen_pos) {
    QtBridge* bridge = QtBridge::get_singleton();
    if (!bridge || !bridge->is_connected_to_qt()) return;

    const Dictionary result = do_raycast(screen_pos);
    Dictionary data;

    if (result.is_empty()) {
        data["hit"] = false;
        bridge->send_event("object_picked", data);
        return;
    }

    Node*   node     = Object::cast_to<Node>(result["collider"]);
    Vector3 hit_pos  = result["position"];
    Vector3 hit_norm = result["normal"];

    data["hit"]      = true;
    data["name"]     = node ? String(node->get_name()) : String("unknown");
    data["type"]     = node ? String(node->get_class()) : String("unknown");
    data["path"]     = node ? String(node->get_path())  : String("");
    data["hit_x"]    = hit_pos.x;
    data["hit_y"]    = hit_pos.y;
    data["hit_z"]    = hit_pos.z;
    data["normal_x"] = hit_norm.x;
    data["normal_y"] = hit_norm.y;
    data["normal_z"] = hit_norm.z;

    if (auto* n3d = Object::cast_to<Node3D>(node)) {
        Vector3 wp    = n3d->get_global_position();
        data["world_x"] = wp.x;
        data["world_y"] = wp.y;
        data["world_z"] = wp.z;
    }

    bridge->send_event("object_picked", data);
}

// ─── handle_middle_click ─────────────────────────────────────────────────────

void QtModule::handle_middle_click(Vector2 screen_pos) {
    const Dictionary result = do_raycast(screen_pos);

    Vector3 spawn_pos;

    if (!result.is_empty()) {
        spawn_pos = Vector3(result["position"])
                  + Vector3(result["normal"]) * 0.1f;
    } else {
        Camera3D* cam = m_camera ? m_camera : get_viewport()->get_camera_3d();
        if (!cam) return;
        const Vector2 center = get_viewport()->get_visible_rect().get_center();
        spawn_pos = cam->project_ray_origin(center)
                  + cam->project_ray_normal(center) * 5.0f;
    }

    spawn_cube(spawn_pos);
}

// ─── spawn_cube ──────────────────────────────────────────────────────────────

void QtModule::spawn_cube(const Vector3& pos) {
    MeshInstance3D* mesh = memnew(MeshInstance3D);

    Ref<BoxMesh> box;
    box.instantiate();
    box->set_size(Vector3(0.2f, 0.2f, 0.2f));

    Ref<StandardMaterial3D> mat;
    mat.instantiate();
    mat->set_albedo(Color(
        UtilityFunctions::randf(),
        UtilityFunctions::randf(),
        UtilityFunctions::randf()));
    box->surface_set_material(0, mat);

    mesh->set_mesh(box);
    mesh->set_global_position(pos);
    mesh->set_name("SpawnedCube");

    StaticBody3D*   body  = memnew(StaticBody3D);
    CollisionShape3D* col = memnew(CollisionShape3D);
    Ref<BoxShape3D> shape;
    shape.instantiate();
    shape->set_size(Vector3(0.2f, 0.2f, 0.2f));
    col->set_shape(shape);
    body->add_child(col);
    mesh->add_child(body);

    get_tree()->get_current_scene()->add_child(mesh);

    QtBridge* bridge = QtBridge::get_singleton();
    if (bridge && bridge->is_connected_to_qt()) {
        Dictionary data;
        data["x"] = pos.x;
        data["y"] = pos.y;
        data["z"] = pos.z;
        bridge->send_event("cube_spawned", data);
    }

    UtilityFunctions::print("[QtModule] Spawned cube at ",
                            pos.x, " ", pos.y, " ", pos.z);
}

} // namespace godot
