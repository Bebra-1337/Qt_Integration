# GodotQtHost — Godot 4 встроенный в Qt 6

## Структура проекта

```
godot_qt_integration/
├── CMakeLists.txt              ← Qt приложение (открывать в Qt Creator)
├── src/                        ← Qt C++ код
│   ├── main.cpp
│   ├── MainWindow.h/cpp        ← Главное окно, данные из движка
│   ├── GodotWidget.h/cpp       ← Виджет-контейнер, XReparentWindow
│   ├── GodotRuntime.h/cpp      ← Запуск процесса Godot
│   └── GodotBridge.h/cpp       ← TCP сервер, приём данных от Godot
└── godot_project/              ← Godot 4 проект
    ├── project.godot
    ├── qt_bridge.gdextension   ← Регистрация C++ плагина
    ├── SConstruct              ← Сборка плагина
    ├── godot-cpp/              ← (клонировать отдельно, см. ниже)
    ├── bin/                    ← Собранный .so плагина
    ├── scenes/                 ← Твои сцены
    └── extension_src/          ← C++ код для Godot
        ├── register_types.h/cpp
        ├── QtBridge.h/cpp      ← Синглтон моста, подключается к Qt
        └── CameraTracker.h/cpp ← Пример: шлёт координаты камеры
```

---

## Первоначальная настройка

### 1. Установить зависимости

```bash
sudo pacman -S qtcreator qt6-base scons python git
```

### 2. Клонировать godot-cpp в папку Godot проекта

```bash
cd godot_project/
git clone https://github.com/godotengine/godot-cpp.git
cd godot-cpp
git checkout godot-4.3-stable    # должно совпадать с версией твоего Godot
git submodule update --init
```

Проверить версию своего Godot:
```bash
godot --version
# Например: 4.3.stable.arch_linux
```

### 3. Собрать godot-cpp

```bash
cd godot_project/godot-cpp
scons platform=linux target=template_debug -j$(nproc)
# Занимает ~5-10 минут
# Результат: bin/libgodot-cpp.linux.template_debug.x86_64.a
```

### 4. Собрать C++ плагин для Godot

```bash
cd godot_project/
scons platform=linux target=template_debug -j$(nproc)
# Результат: bin/libqt_bridge.linux.template_debug.x86_64.so
```

### 5. Открыть Qt проект в Qt Creator

1. **File → Open File or Project** → выбрать `CMakeLists.txt`
2. Kit: **Desktop Qt 6.x.x GCC 64bit**
3. CMake arguments оставить пустыми — путь к Godot подтягивается автоматически
   через `$ENV{HOME}/godot`. Если у тебя godot в другом месте:
   **Projects → Build → CMake → добавить** `-DGODOT_ROOT=/твой/путь`
4. **Configure Project**

### 6. Настроить переменную окружения в Qt Creator

**Projects → Run → Environment → Add:**
```
QT_QPA_PLATFORM = xcb
```

Это нужно для XReparentWindow на Wayland. Можно пропустить если запускать
из терминала — `qputenv` в `main.cpp` уже делает это автоматически.

---

## Сборка и запуск

### Qt приложение

Из Qt Creator: **Ctrl+B** (сборка), **Ctrl+R** (запуск).

Из терминала:
```bash
mkdir build && cd build
cmake .. -GNinja
ninja
./GodotQtHost
```

### Godot плагин (при изменении extension_src/)

```bash
cd godot_project/
scons platform=linux target=template_debug -j$(nproc)
```

Godot подхватывает новый `.so` при следующем запуске автоматически.

---

## Настройка Godot сцены

### Добавить QtBridge как Autoload

1. Открой проект в редакторе Godot: `godot --editor --path godot_project/`
2. **Project Settings → Autoload → Add**
   - Path: создай сцену `autoload/QtBridge.tscn` с нодой типа **QtBridge**
   - Name: `QtBridge`

Или сделай это программно — добавь в `project.godot`:
```ini
[autoload]
QtBridge="*res://autoload/QtBridge.tscn"
```

### Добавить CameraTracker в сцену

В любой сцене добавь дочернюю ноду типа **CameraTracker**.
Она автоматически найдёт Camera3D и начнёт слать координаты в Qt.

---

## Как добавлять новый функционал

### Новые данные из Godot → Qt

**1. В `extension_src/` добавь C++ класс или расширь CameraTracker:**

```cpp
// В _process() любого C++ узла:
QtBridge* bridge = QtBridge::get_singleton();
if (bridge && bridge->is_connected_to_qt()) {
    Dictionary data;
    data["health"] = player_health;
    data["score"]  = current_score;
    bridge->send_event("player_stats", data);
}
```

**2. В Qt пересобери плагин:**
```bash
cd godot_project && scons platform=linux target=template_debug
```

**3. В `src/GodotBridge.h` добавь сигнал:**
```cpp
signals:
    void playerStatsReceived(int health, int score);
```

**4. В `src/GodotBridge.cpp` добавь обработку в `dispatchMessage()`:**
```cpp
if (event == "player_stats") {
    emit playerStatsReceived(
        data["health"].toInt(),
        data["score"].toInt()
    );
}
```

**5. В `src/MainWindow.cpp` подключи и отобрази:**
```cpp
connect(bridge, &GodotBridge::playerStatsReceived,
        this, [this](int hp, int score) {
    m_hpLabel->setText(QString("HP: %1").arg(hp));
    m_scoreLabel->setText(QString("Score: %1").arg(score));
});
```

### Команды из Qt → Godot

**1. В Qt отправь команду:**
```cpp
// Смена сцены
m_godotWidget->bridge()->sendCommand(
    GodotBridge::Cmd::ChangeScene, "res://scenes/Level2.tscn");

// Вызов метода на ноде
m_godotWidget->bridge()->callNodeMethod(
    "/root/Game/Player", "take_damage", {25});

// Установка переменной
m_godotWidget->bridge()->setNodeVar(
    "/root/Game", "gravity_scale", 0.5);
```

**2. В Godot C++ обработай в `QtBridge::handle_command()`** или
   подпишись на сигнал `command_received` из любого узла.

---

## Типичный workflow разработки

```
1. Открыл Qt Creator
2. Редактируешь src/*.cpp  →  Ctrl+B  →  Ctrl+R
3. Редактируешь extension_src/*.cpp  →  scons в терминале  →  Ctrl+R
4. Редактируешь сцены в Godot редакторе  →  Ctrl+R
```

Для удобства можно добавить scons как Custom Build Step в Qt Creator:
**Projects → Build → Add Build Step → Custom Process Step**
- Command: `scons`
- Arguments: `platform=linux target=template_debug -j8`
- Working directory: `%{sourceDir}/godot_project`

---

## Архитектура моста

```
Qt (GodotBridge)          TCP :47890          Godot (QtBridge.cpp)
     QTcpServer     ←─── connect ────────     ::connect()
          │                                        │
          │          {"cmd":"change_scene"}         │
          │◄────────────────────────────────────────│  sendCommand()
          │                                        │
          │     {"type":"custom_event",             │
          │      "event":"camera_update",           │
          │◄────────────────────────────────────────│  send_event()
          │      "data":{"pos":{...}}}              │
          ▼                                        ▼
   emit cameraUpdated()                    handle_command()
   MainWindow обновляет UI           (смена сцены, пауза, etc.)
```
