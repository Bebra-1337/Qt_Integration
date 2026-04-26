#pragma once
// CameraTracker.h — пример C++ ноды которая шлёт данные камеры в Qt.
//
// Добавь эту ноду в любую сцену. Она автоматически найдёт Camera3D
// в текущем Viewport и будет слать её координаты в Qt 20 раз в секунду.
//
// Чтобы добавить свои данные — просто расширь _process() по аналогии.

#include <godot_cpp/classes/node3d.hpp>

namespace godot {

class CameraTracker : public Node {
    GDCLASS(CameraTracker, Node)

public:
    void _process(double delta) override;

    // Интервал отправки в секундах (по умолчанию 0.05 = 20 fps)
    void   set_send_interval(double interval) { m_interval = interval; }
    double get_send_interval() const { return m_interval; }

protected:
    static void _bind_methods();

private:
    double m_timer    = 0.0;
    double m_interval = 0.05; // 20 раз в секунду
};

} // namespace godot
