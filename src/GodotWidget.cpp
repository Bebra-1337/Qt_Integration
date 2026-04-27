#include "GodotWidget.h"
#include "GodotRuntime.h"
#include "GodotBridge.h"

#include <QTimer>
#include <QResizeEvent>
#include <QDebug>

#ifdef Q_OS_LINUX
#  include <X11/Xlib.h>
#  include <X11/Xutil.h>
#  include <X11/Xatom.h>
#endif

GodotWidget::GodotWidget(QWidget* parent)
    : QWidget(parent)
    , m_runtime(std::make_unique<GodotRuntime>(this))
    , m_bridge(std::make_unique<GodotBridge>(this))
{
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(320, 240);

    connect(m_runtime.get(), &GodotRuntime::initialized,
            this, &GodotWidget::onGodotInitialized);
    connect(m_runtime.get(), &GodotRuntime::shutdownComplete,
            this, &GodotWidget::godotStopped);
    connect(m_runtime.get(), &GodotRuntime::engineError,
            this, &GodotWidget::godotError);
}

GodotWidget::~GodotWidget() { stopGodot(); }

bool GodotWidget::startGodot(const QString& projectPath,
                             const QString& mainScene)
{
    m_bridge->startServer(47890);

    winId(); // форсируем создание нативного окна

    GodotInitConfig cfg;
    cfg.projectPath      = projectPath;
    cfg.mainScene        = mainScene;
    cfg.initialSize      = size();
    cfg.externalWindowId = static_cast<uintptr_t>(this->winId());

    return m_runtime->initialize(cfg);
}

void GodotWidget::stopGodot() {
    if (m_runtime && m_runtime->isRunning())
        m_runtime->shutdown();
    m_embedded      = false;
    m_embeddedWinId = 0;
}

bool GodotWidget::isGodotRunning() const {
    return m_runtime && m_runtime->isRunning();
}

void GodotWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    resizeEmbeddedWindow();
    if (m_bridge->isGodotConnected()) {
        m_bridge->sendCommand(GodotBridge::Cmd::Resize,
                              QString("%1x%2")
                                  .arg(event->size().width())
                                  .arg(event->size().height()));
    }
}

// ─── onGodotInitialized ───────────────────────────────────────────────────────

void GodotWidget::onGodotInitialized() {
    qDebug() << "[GodotWidget] Searching for Godot X11 window (PID:"
             << m_runtime->godotPid() << ")";
    m_findWindowAttempts = 0;

    auto* timer = new QTimer(this);
    timer->setInterval(200); // опрашиваем чаще — меньше времени до embed

    connect(timer, &QTimer::timeout, this, [this, timer]() {
        ++m_findWindowAttempts;

        const uintptr_t winId = findWindowByPid(
            static_cast<ulong>(m_runtime->godotPid()));

        if (winId != 0) {
            timer->stop();
            timer->deleteLater();
            qDebug() << "[GodotWidget] Found Godot window" << winId;

            // Сразу скрываем окно у WM ДО того как оно успевает
            // нормально отрисоваться — убирает "мигание"
            hideWindowDecorations(winId);

            // Короткая пауза: Godot должен закончить маппинг окна
            // прежде чем мы делаем XReparentWindow
            QTimer::singleShot(300, this, [this, winId]() {
#ifdef Q_OS_LINUX
                Display* dpy = XOpenDisplay(nullptr);
                if (dpy) {
                    XWindowAttributes a{};
                    const bool alive = XGetWindowAttributes(
                        dpy, static_cast<Window>(winId), &a);
                    XCloseDisplay(dpy);
                    if (!alive || a.map_state == IsUnmapped) {
                        qDebug() << "[GodotWidget] Window gone, retrying";
                        onGodotInitialized();
                        return;
                    }
                }
#endif
                embedGodotWindow(winId);
                emit godotReady();
            });

        } else if (m_findWindowAttempts >= 75) { // 15 секунд при 200ms
            timer->stop();
            timer->deleteLater();
            emit godotError("Cannot find Godot window after 15 seconds");
        }
    });

    timer->start();
}

// ─── hideWindowDecorations ───────────────────────────────────────────────────
// Вызываем ДО embed — просим WM убрать заголовок и рамку сразу,
// чтобы окно не успело мелькнуть со своей рамкой

void GodotWidget::hideWindowDecorations(uintptr_t winId) {
#ifdef Q_OS_LINUX
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) return;

    const Window w = static_cast<Window>(winId);

    // _MOTIF_WM_HINTS — стандартный способ убрать декорации
    Atom motif = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
    if (motif != None) {
        struct MotifHints {
            unsigned long flags, functions, decorations, input_mode, status;
        };
        MotifHints hints = {2, 0, 0, 0, 0}; // flags=2 → только decorations
        XChangeProperty(dpy, w, motif, motif, 32, PropModeReplace,
                        reinterpret_cast<unsigned char*>(&hints), 5);
    }

    // Дополнительно: убираем из taskbar
    Atom wmState     = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom skipTaskbar = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
    Atom skipPager   = XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER", False);
    if (wmState != None && skipTaskbar != None) {
        Atom atoms[2] = {skipTaskbar, skipPager};
        XChangeProperty(dpy, w, wmState, XA_ATOM, 32, PropModeReplace,
                        reinterpret_cast<unsigned char*>(atoms), 2);
    }

    XFlush(dpy);
    XCloseDisplay(dpy);
#else
    Q_UNUSED(winId)
#endif
}

// ─── findWindowByPid ─────────────────────────────────────────────────────────

#ifdef Q_OS_LINUX
static uintptr_t searchTree(Display* dpy, Window w, ulong pid) {
    Atom netPid = XInternAtom(dpy, "_NET_WM_PID", False);
    if (netPid != None) {
        Atom type; int fmt; ulong n, after;
        unsigned char* prop = nullptr;
        if (XGetWindowProperty(dpy, w, netPid, 0, 1, False,
                               XA_CARDINAL, &type, &fmt,
                               &n, &after, &prop) == Success && prop) {
            const ulong wpid = *reinterpret_cast<ulong*>(prop);
            XFree(prop);
            if (wpid == pid) {
                XWindowAttributes a{};
                if (XGetWindowAttributes(dpy, w, &a) &&
                    a.width > 200 && a.height > 200 &&
                    a.map_state == IsViewable) {
                    XClassHint ch{};
                    if (XGetClassHint(dpy, w, &ch)) {
                        XFree(ch.res_name);
                        XFree(ch.res_class);
                        return static_cast<uintptr_t>(w);
                    }
                }
            }
        }
    }
    Window parent; Window* children = nullptr; unsigned n = 0;
    if (XQueryTree(dpy, w, &parent, &parent, &children, &n) && children) {
        for (unsigned i = 0; i < n; ++i) {
            uintptr_t r = searchTree(dpy, children[i], pid);
            if (r) { XFree(children); return r; }
        }
        XFree(children);
    }
    return 0;
}
#endif

uintptr_t GodotWidget::findWindowByPid(ulong pid) {
#ifdef Q_OS_LINUX
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) return 0;
    const uintptr_t r = searchTree(dpy, DefaultRootWindow(dpy), pid);
    XCloseDisplay(dpy);
    return r;
#else
    Q_UNUSED(pid) return 0;
#endif
}

// ─── embedGodotWindow ────────────────────────────────────────────────────────

void GodotWidget::embedGodotWindow(uintptr_t winId) {
    m_embeddedWinId = winId;

#ifdef Q_OS_LINUX
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) return;

    const Window parent = static_cast<Window>(this->winId());
    const Window child  = static_cast<Window>(winId);

    // Перемещаем в Qt-контейнер
    XReparentWindow(dpy, child, parent, 0, 0);

    // Сразу выставляем нужный размер
    XMoveResizeWindow(dpy, child, 0, 0,
                      static_cast<unsigned>(width()),
                      static_cast<unsigned>(height()));

    XMapWindow(dpy, child);
    XRaiseWindow(dpy, child);
    XFlush(dpy);
    XCloseDisplay(dpy);

    qDebug() << "[GodotWidget] Embedded window" << winId
             << "into Qt widget" << this->winId();
#endif

    m_embedded = true;
    resizeEmbeddedWindow();
}

// ─── resizeEmbeddedWindow ────────────────────────────────────────────────────

void GodotWidget::resizeEmbeddedWindow() {
    if (!m_embedded || !m_embeddedWinId) return;
#ifdef Q_OS_LINUX
    Display* dpy = XOpenDisplay(nullptr);
    if (dpy) {
        XMoveResizeWindow(dpy, static_cast<Window>(m_embeddedWinId),
                          0, 0,
                          static_cast<unsigned>(width()),
                          static_cast<unsigned>(height()));
        XFlush(dpy);
        XCloseDisplay(dpy);
    }
#endif
}