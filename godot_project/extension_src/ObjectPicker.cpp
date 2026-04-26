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

namespace godot {

void ObjectPicker::_spawn_cube(const Vector3& position) {
    MeshInstance3D* cube = memnew(MeshInstance3D);

    Ref<BoxMesh> mesh;
    mesh.instantiate();
    mesh->set_size(Vector3(0.2f, 0.2f, 0.2f));

    Ref<StandardMaterial3D> mat;
    mat.instantiate();
    mat->set_albedo(Color(0.0f, 1.0f, 0.0f));
    mesh->set_material(mat);

    cube->set_mesh(mesh);
    cube->set_global_position(position);

    StaticBody3D* body = memnew(StaticBody3D);
    cube->add_child(body);

    CollisionShape3D* col = memnew(CollisionShape3D);
    Ref<BoxShape3D> shape;
    shape.instantiate();
    shape->set_size(Vector3(0.2f, 0.2f, 0.2f));
    col->set_shape(shape);
    body->add_child(col);

    Node* scene = get_tree()->get_current_scene();
    scene->add_child(cube);
    cube->set_owner(scene);
    body->set_owner(scene);
    col->set_owner(scene);
}

void ObjectPicker::_input(const Ref<InputEvent>& event) {
    const auto* mb = Object::cast_to<InputEventMouseButton>(*event);
    if (!mb || !mb->is_pressed() || mb->is_double_click()) return;

    Camera3D* cam = get_viewport()->get_camera_3d();
    if (!cam) return;

    const Vector2 click_pos  = mb->get_position();
    const Vector3 ray_origin = cam->project_ray_origin(click_pos);
    const Vector3 ray_end    = ray_origin + cam->project_ray_normal(click_pos) * 1000.0f;

    if (mb->get_button_index() == MOUSE_BUTTON_MIDDLE) {
        PhysicsDirectSpaceState3D* space =
            get_viewport()->get_world_3d()->get_direct_space_state();
        if (!space) return;

        Ref<PhysicsRayQueryParameters3D> query =
            PhysicsRayQueryParameters3D::create(ray_origin, ray_end);
        query->set_collide_with_areas(true);
        query->set_collide_with_bodies(true);

        const Dictionary result = space->intersect_ray(query);
        const Vector3 spawn_pos = result.is_empty()
            ? ray_origin + cam->project_ray_normal(click_pos) * 5.0f
            : Vector3(result["position"]);

        _spawn_cube(spawn_pos);
        return;
    }

    if (mb->get_button_index() != MOUSE_BUTTON_RIGHT) return;

    QtBridge* bridge = QtBridge::get_singleton();
    if (!bridge || !bridge->is_connected_to_qt()) return;

    PhysicsDirectSpaceState3D* space =
        get_viewport()->get_world_3d()->get_direct_space_state();
    if (!space) return;

    Ref<PhysicsRayQueryParameters3D> query =
        PhysicsRayQueryParameters3D::create(ray_origin, ray_end);
    query->set_collide_with_areas(true);
    query->set_collide_with_bodies(true);

    const Dictionary result = space->intersect_ray(query);

    Dictionary data;

    if (result.is_empty()) {
        data["hit"]  = false;
        data["name"] = String("");
        bridge->send_event("object_picked", data);
        return;
    }

    Object* obj  = Object::cast_to<Object>(result["collider"]);
    Node*   node = Object::cast_to<Node>(obj);

    const Vector3 hit_pos    = result["position"];
    const Vector3 hit_normal = result["normal"];

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

    if (auto* n3d = Object::cast_to<Node3D>(node)) {
        const Vector3 wp = n3d->get_global_position();
        data["world_x"] = wp.x;
        data["world_y"] = wp.y;
        data["world_z"] = wp.z;
    }

    bridge->send_event("object_picked", data);
}

} // namespace godot