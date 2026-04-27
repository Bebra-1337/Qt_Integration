#pragma once
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace godot {

class StaticBody3D;

class ObjectPicker : public Node {
    GDCLASS(ObjectPicker, Node)

public:
    void _ready() override;
    void _input(const Ref<InputEvent>& event) override;

    // Вызывается через сигнал command_received от QtBridge
    void handle_qt_command(const String& cmd, const Dictionary& payload);

protected:
    static void _bind_methods();

private:
    void _spawn_preview(const Vector3& position);
    void _remove_preview();

    StaticBody3D* m_preview = nullptr;
    static int    s_cube_counter;
};

} // namespace godot