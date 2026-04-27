cmake_minimum_required(VERSION 3.20)
project(GodotQtHost VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── Qt ───────────────────────────────────────────────────────────────────────
find_package(Qt5 REQUIRED COMPONENTS Core Widgets OpenGL Network)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# ─── Godot source root (для include заголовков) ───────────────────────────────
set(GODOT_ROOT "$ENV{HOME}/godot" CACHE PATH "Path to Godot source root")
message(STATUS "Godot root: ${GODOT_ROOT}")

# ─── GDExtension (scons) ──────────────────────────────────────────────────────
# Путь к папке Godot проекта где лежит SConstruct
set(GODOT_PROJECT_DIR "${CMAKE_SOURCE_DIR}/godot_project" CACHE PATH
    "Path to Godot project with SConstruct")

set(GDEXT_TARGET  "template_debug" CACHE STRING "Scons target (template_debug / template_release)")
set(GDEXT_JOBS    "0"              CACHE STRING "Parallel jobs for scons (0 = nproc)")

# Определяем количество потоков
if(GDEXT_JOBS STREQUAL "0")
    cmake_host_system_information(RESULT _cpu_count QUERY NUMBER_OF_LOGICAL_CORES)
    set(_scons_jobs ${_cpu_count})
else()
    set(_scons_jobs ${GDEXT_JOBS})
endif()

# Файл-маркер: scons пересобирает только если исходники изменились
file(GLOB_RECURSE GDEXT_SOURCES
    "${GODOT_PROJECT_DIR}/extension_src/*.cpp"
    "${GODOT_PROJECT_DIR}/extension_src/*.h"
)

# Имя выходного .so (scons кладёт его в godot_project/bin/)
set(GDEXT_OUTPUT "${GODOT_PROJECT_DIR}/bin/libqt_bridge.so")

# Кастомная команда — вызывает scons при изменении исходников
add_custom_command(
    OUTPUT  ${GDEXT_OUTPUT}
    COMMAND scons
            platform=linux
            target=${GDEXT_TARGET}
            -j${_scons_jobs}
    WORKING_DIRECTORY ${GODOT_PROJECT_DIR}
    DEPENDS ${GDEXT_SOURCES}
    COMMENT "Building GDExtension with scons (target=${GDEXT_TARGET}, jobs=${_scons_jobs})"
    VERBATIM
)

# Цель gdextension — можно собирать отдельно: cmake --build . --target gdextension
add_custom_target(gdextension
    DEPENDS ${GDEXT_OUTPUT}
    COMMENT "GDExtension (libqt_bridge.so)"
)

# ─── Sources ──────────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/GodotWidget.cpp
    src/GodotRuntime.cpp
    src/GodotBridge.cpp
    src/XYZDialog.cpp
)

set(HEADERS
    src/MainWindow.h
    src/GodotWidget.h
    src/GodotRuntime.h
    src/GodotBridge.h
    src/XYZDialog.h
)

# Регистрируем extension_src как "заголовочный" проект — только для навигации в Qt Creator
# Файлы видны в дереве проекта, но компилируются через scons
file(GLOB_RECURSE GDEXT_ALL_FILES
    "${GODOT_PROJECT_DIR}/extension_src/*.cpp"
    "${GODOT_PROJECT_DIR}/extension_src/*.h"
    "${GODOT_PROJECT_DIR}/SConstruct"
    "${GODOT_PROJECT_DIR}/qt_bridge.gdextension"
    "${GODOT_PROJECT_DIR}/project.godot"
)

# Помечаем .cpp файлы extension_src как HEADER_FILE_ONLY
# — Qt Creator их видит и индексирует, но CMake не компилирует
foreach(_f ${GDEXT_ALL_FILES})
    if(_f MATCHES "\\.cpp$")
        set_source_files_properties(${_f} PROPERTIES HEADER_FILE_ONLY TRUE)
    endif()
endforeach()

# Фиктивная цель только для отображения файлов в Qt Creator
add_custom_target(godot_project_files
    SOURCES ${GDEXT_ALL_FILES}
    COMMENT "Godot project files (edit only, built by scons)"
)

add_executable(GodotQtHost ${SOURCES} ${HEADERS})

# GodotQtHost зависит от gdextension — при сборке Qt автоматически
# запустится scons если extension_src изменился
add_dependencies(GodotQtHost gdextension)

target_include_directories(GodotQtHost PRIVATE
    src/
    ${GODOT_ROOT}/
    ${GODOT_ROOT}/core/
    ${GODOT_ROOT}/modules/
    # Заголовки godot-cpp для навигации/автодополнения в Qt Creator
    ${GODOT_PROJECT_DIR}/godot-cpp/include/
    ${GODOT_PROJECT_DIR}/godot-cpp/gen/include/
    ${GODOT_PROJECT_DIR}/godot-cpp/gdextension/
    ${GODOT_PROJECT_DIR}/extension_src/
)

target_link_libraries(GodotQtHost PRIVATE
    Qt5::Core
    Qt5::Widgets
    Qt5::OpenGL
    Qt5::Network
)

find_package(X11)
target_link_libraries(GodotQtHost PRIVATE X11::X11 dl pthread)
target_compile_definitions(GodotQtHost PRIVATE PLATFORM_LINUX)
target_compile_options(GodotQtHost PRIVATE -Wall -Wextra)

# ─── QT_QPA_PLATFORM=xcb при запуске из Qt Creator ───────────────────────────
set_target_properties(GodotQtHost PROPERTIES
    VS_DEBUGGER_ENVIRONMENT "QT_QPA_PLATFORM=xcb"
)

# ─── Сводка ───────────────────────────────────────────────────────────────────
message(STATUS "")
message(STATUS "=== GodotQtHost ===")
message(STATUS "  Qt host binary  : GodotQtHost")
message(STATUS "  Godot project   : ${GODOT_PROJECT_DIR}")
message(STATUS "  GDExtension out : ${GDEXT_OUTPUT}")
message(STATUS "  Scons target    : ${GDEXT_TARGET}")
message(STATUS "  Scons jobs      : ${_scons_jobs}")
message(STATUS "")
message(STATUS "Targets:")
message(STATUS "  cmake --build . --target GodotQtHost   (Qt app only)")
message(STATUS "  cmake --build . --target gdextension   (scons only)")
message(STATUS "  cmake --build .                        (оба)")
message(STATUS "")