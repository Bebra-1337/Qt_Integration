#include "register_types.h"
#include "CameraTracker.h"
#include "ObjectPicker.h"
#include "QtBridge.h"
#include "QtModule.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

void initialize_qt_bridge_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;

    ClassDB::register_class<QtBridge>();
    //ClassDB::register_class<QtModule>();
    ClassDB::register_class<CameraTracker>();
    ClassDB::register_class<ObjectPicker>();
    // CameraTracker и ObjectPicker заменены на QtModule
}

void uninitialize_qt_bridge_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
}

extern "C" {
GDExtensionBool GDE_EXPORT qt_bridge_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    const GDExtensionClassLibraryPtr   p_library,
    GDExtensionInitialization*         r_initialization)
{
    GDExtensionBinding::InitObject init_obj(
        p_get_proc_address, p_library, r_initialization);

    init_obj.register_initializer(initialize_qt_bridge_module);
    init_obj.register_terminator(uninitialize_qt_bridge_module);
    init_obj.set_minimum_library_initialization_level(
        MODULE_INITIALIZATION_LEVEL_SCENE);

    return init_obj.init();
}
}