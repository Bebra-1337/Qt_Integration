#include "ObjectPicker.h"
#include "QtBridge.h"

#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

int ObjectPicker::s_cube_counter = 0;

// ─── helpers ─────────────────────────────────────────────────────────────────

static Dictionary do_raycast(Node* node, Vector2 screen_pos) {
    Camera3D* cam = node->get_viewport()->get_camera_3d();
    if (!cam) return Dictionary();

    PhysicsDirectSpaceState3D* space =
        node->get_viewport()->get_world_3d()->get_direct_space_state();
    if (!space) return Dictionary();

    const Vector3 origin = cam->project_ray_origin(screen_pos);
    const Vector3 end    = origin + cam->project_ray_normal(screen_pos) * 1000.0f;

    Ref<PhysicsRayQueryParameters3D> query =
        PhysicsRayQueryParameters3D::create(origin, end);
    query->set_collide_with_bodies(true);

    return space->intersect_ray(query);
}

static StaticBody3D* make_cube_body(const Vector3& position,
                                    const Color& color,
                                    const String& name)
{
    StaticBody3D* body = memnew(StaticBody3D);

    CollisionShape3D* col = memnew(CollisionShape3D);
    Ref<BoxShape3D> shape;
    shape.instantiate();
    shape->set_size(Vector3(0.2f, 0.2f, 0.2f));
    col->set_shape(shape);
    body->add_child(col);

    MeshInstance3D* mesh_inst = memnew(MeshInstance3D);
    Ref<BoxMesh> mesh;
    mesh.instantiate();
    mesh->set_size(Vector3(0.2f, 0.2f, 0.2f));

    Ref<StandardMaterial3D> mat;
    mat.instantiate();
    mat->set_albedo(color);
    if (color.a < 1.0f) {
        mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
    }
    mesh->surface_set_material(0, mat);
    mesh_inst->set_mesh(mesh);
    body->add_child(mesh_inst);

    body->set_position(position);
    body->set_name(name);
    return body;
}

// ─── _bind_methods ───────────────────────────────────────────────────────────

void ObjectPicker::_bind_methods() {
    // Команды от Qt
    ClassDB::bind_method(D_METHOD("handle_qt_command", "cmd", "payload"),
                         &ObjectPicker::handle_qt_command);
}

void ObjectPicker::_ready() {
    // Подписываемся на команды от Qt через QtBridge
    QtBridge* bridge = QtBridge::get_singleton();
    if (bridge) {
        bridge->connect("command_received",
                        Callable(this, "handle_qt_command"));
    }
}

// ─── preview management ──────────────────────────────────────────────────────

void ObjectPicker::_spawn_preview(const Vector3& position) {
    _remove_preview();

    // Полупрозрачный ярко-зелёный
    m_preview = make_cube_body(position,
                               Color(0.0f, 1.0f, 0.1f, 0.45f),
                               "__preview__");
    get_tree()->get_current_scene()->add_child(m_preview);
}

void ObjectPicker::_remove_preview() {
    if (m_preview) {
        m_preview->queue_free();
        m_preview = nullptr;
    }
}

// ─── handle_qt_command ───────────────────────────────────────────────────────

void ObjectPicker::handle_qt_command(const String& cmd, const Dictionary& payload) {
    if (cmd == "confirm_spawn") {
        if (!m_preview) return;
        const Vector3 pos = m_preview->get_position();
        const Vector3 rot = m_preview->get_rotation_degrees();
        const String  name = payload.has("name")
                             ? String(payload["name"])
                             : String("Куб_") + String::num_int64(s_cube_counter);
        ++s_cube_counter;

        StaticBody3D* cube = make_cube_body(pos,
            Color(0.0f, 0.9f, 0.1f, 1.0f),
            name);
        cube->set_rotation_degrees(rot);
        cube->add_to_group("spawned_cubes");
        get_tree()->get_current_scene()->add_child(cube);
        _remove_preview();

        QtBridge* bridge = QtBridge::get_singleton();
        if (bridge && bridge->is_connected_to_qt()) {
            Dictionary data;
            data["name"] = name;
            data["path"] = String(cube->get_path());
            data["x"] = pos.x; data["y"] = pos.y; data["z"] = pos.z;
            bridge->send_event("cube_spawned", data);
        }

    } else if (cmd == "cancel_spawn") {
        _remove_preview();

    } else if (cmd == "move_preview") {
        if (!m_preview) return;
        m_preview->set_position(Vector3(
            double(payload["x"]), double(payload["y"]), double(payload["z"])));

    } else if (cmd == "rotate_preview") {
        if (!m_preview) return;
        m_preview->set_rotation_degrees(Vector3(
            double(payload["x"]), double(payload["y"]), double(payload["z"])));

    } else if (cmd == "move_cube") {
        const String path = payload["path"];
        Node* node = get_node_or_null(NodePath(path));
        if (auto* body = Object::cast_to<StaticBody3D>(node)) {
            body->set_position(Vector3(
                double(payload["x"]), double(payload["y"]), double(payload["z"])));
        }

    } else if (cmd == "rotate_cube") {
        const String path = payload["path"];
        Node* node = get_node_or_null(NodePath(path));
        if (auto* body = Object::cast_to<StaticBody3D>(node)) {
            body->set_rotation_degrees(Vector3(
                double(payload["x"]), double(payload["y"]), double(payload["z"])));
        }

    } else if (cmd == "rename_cube") {
        const String path    = payload["path"];
        const String newname = payload["new_name"];
        Node* node = get_node_or_null(NodePath(path));
        if (!node) return;
        node->set_name(newname);
        QtBridge* bridge = QtBridge::get_singleton();
        if (bridge && bridge->is_connected_to_qt()) {
            Dictionary data;
            data["path"]     = path;
            data["new_path"] = String(node->get_path());
            data["new_name"] = newname;
            bridge->send_event("cube_renamed", data);
        }

    } else if (cmd == "delete_cube") {
        const String path = payload["path"];
        Node* node = get_node_or_null(NodePath(path));
        if (!node) return;
        const String name = node->get_name();
        node->queue_free();
        QtBridge* bridge = QtBridge::get_singleton();
        if (bridge && bridge->is_connected_to_qt()) {
            Dictionary data;
            data["name"] = name;
            data["path"] = path;
            bridge->send_event("cube_deleted", data);
        }

    } else if (cmd == "set_alert") {
        const String path  = payload["path"];
        const bool   alert = static_cast<bool>(payload.get("alert", false));
        Node* node = get_node_or_null(NodePath(path));
        if (!node) return;
        node->set_meta("alert", alert);
        for (int i = 0; i < node->get_child_count(); ++i) {
            if (auto* mi = Object::cast_to<MeshInstance3D>(node->get_child(i))) {
                Ref<StandardMaterial3D> mat = mi->get_mesh()->surface_get_material(0);
                if (mat.is_valid())
                    mat->set_albedo(alert ? Color(1.0f, 0.05f, 0.05f, 1.0f)
                                          : Color(0.0f, 0.9f,  0.1f,  1.0f));
                break;
            }
        }
        QtBridge* bridge = QtBridge::get_singleton();
        if (bridge && bridge->is_connected_to_qt()) {
            Dictionary data;
            data["path"]  = path;
            data["alert"] = alert;
            bridge->send_event("alert_changed", data);
        }
    }
}

// ─── _input ──────────────────────────────────────────────────────────────────

void ObjectPicker::_input(const Ref<InputEvent>& event) {
    const auto* mb = Object::cast_to<InputEventMouseButton>(*event);
    if (!mb || !mb->is_pressed() || mb->is_double_click()) return;
    if (mb->get_button_index() != MOUSE_BUTTON_RIGHT) return;

    QtBridge* bridge = QtBridge::get_singleton();
    if (!bridge || !bridge->is_connected_to_qt()) return;

    const Vector2 mouse_pos = get_viewport()->get_mouse_position();
    const Dictionary result = do_raycast(this, mouse_pos);

    if (result.is_empty()) return;

    Node* collider = Object::cast_to<Node>(result["collider"].operator Object*());
    // collider — это StaticBody3D напрямую (физдвижок регистрирует тело, не форму)
    // Проверяем сначала сам collider, потом его родителя
    Node* node = nullptr;
    if (collider && collider->is_in_group("spawned_cubes")) {
        node = collider;
    } else if (collider && collider->get_parent()
               && collider->get_parent()->is_in_group("spawned_cubes")) {
        node = collider->get_parent();
    } else {
        node = (collider && collider->get_parent()) ? collider->get_parent() : collider;
    }

    const Vector3 hit_pos    = result["position"];
    const Vector3 hit_normal = result["normal"];

    Dictionary data;
    data["hit"]      = true;
    data["name"]     = node ? String(node->get_name()) : String("unknown");
    data["type"]     = node ? String(node->get_class()) : String("unknown");
    data["path"]     = node ? String(node->get_path())  : String("");
    data["hit_x"]    = hit_pos.x;
    data["hit_y"]    = hit_pos.y;
    data["hit_z"]    = hit_pos.z;
    data["normal_x"] = hit_normal.x;
    data["normal_y"] = hit_normal.y;
    data["normal_z"] = hit_normal.z;

    // Проверяем: это спавненный куб? (по группе)
    bool is_cube = node && node->is_in_group("spawned_cubes");

    if (is_cube) {
        if (auto* n3d = Object::cast_to<Node3D>(node)) {
            const Vector3 wp  = n3d->get_global_position();
            const Vector3 rot = n3d->get_rotation_degrees();
            data["world_x"] = wp.x;  data["world_y"] = wp.y;  data["world_z"] = wp.z;
            data["rot_x"]   = rot.x; data["rot_y"]   = rot.y; data["rot_z"]   = rot.z;
        }
        data["alert"] = node->has_meta("alert") ? static_cast<bool>(node->get_meta("alert")) : false;
        bridge->send_event("object_picked", data);
    } else {
        const Vector3 spawn_pos = hit_pos + hit_normal * 0.15f;
        _spawn_preview(spawn_pos);

        Dictionary spawn_data;
        spawn_data["spawn_x"] = spawn_pos.x;
        spawn_data["spawn_y"] = spawn_pos.y;
        spawn_data["spawn_z"] = spawn_pos.z;
        spawn_data["next_id"] = s_cube_counter;
        bridge->send_event("spawn_preview", spawn_data);
    }

    get_viewport()->set_input_as_handled();
}

} // namespace godot
