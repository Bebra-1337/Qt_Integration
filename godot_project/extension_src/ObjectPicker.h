#pragma once
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/input_event.hpp>

namespace godot {

class ObjectPicker : public Node {
    GDCLASS(ObjectPicker, Node)

public:
    void _input(const Ref<InputEvent>& event) override;

protected:
    static void _bind_methods() {}

private:
    void _spawn_cube(const Vector3& position);
};

} // namespace godot