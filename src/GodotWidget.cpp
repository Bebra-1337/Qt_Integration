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
    // Стартуем TCP сервер ДО запуска Godot — он сразу попытается подключиться
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
    m_embedded    = false;
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
    qDebug() << "[GodotWidget] Searching for Godot X11 window...";
    m_findWindowAttempts = 0;

    auto* timer = new QTimer(this);
    timer->setInterval(300);

    connect(timer, &QTimer::timeout, this, [this, timer]() {
        m_findWindowAttempts++;

        const uintptr_t winId = findWindowByPid(
            static_cast<ulong>(m_runtime->godotPid()));

        if (winId != 0) {
            timer->stop();
            timer->deleteLater();
            qDebug() << "[GodotWidget] Found window" << winId
                     << "— confirming stability in 800ms...";

            QTimer::singleShot(800, this, [this, winId]() {
#ifdef Q_OS_LINUX
                // Проверяем что окно всё ещё живо
                Display* dpy = XOpenDisplay(nullptr);
                if (dpy) {
                    XWindowAttributes a;
                    const bool alive = XGetWindowAttributes(
                        dpy, static_cast<Window>(winId), &a);
                    XCloseDisplay(dpy);
                    if (!alive) {
                        qDebug() << "[GodotWidget] Window gone, restarting search";
                        onGodotInitialized();
                        return;
                    }
                }
#endif
                embedGodotWindow(winId);
                emit godotReady();
            });

        } else if (m_findWindowAttempts >= 60) {
            timer->stop();
            timer->deleteLater();
            emit godotError("Cannot find Godot window after 18 seconds");
        }
    });

    timer->start();
}

// ─── findWindowByPid ─────────────────────────────────────────────────────────

#ifdef Q_OS_LINUX
static uintptr_t searchTree(Display* dpy, Window w, ulong pid) {
    // Проверяем _NET_WM_PID
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
                XWindowAttributes a;
                if (XGetWindowAttributes(dpy, w, &a) &&
                    a.width > 200 && a.height > 200 &&
                    a.map_state == IsViewable) {
                    // Только окна с WM_CLASS (не splash)
                    XClassHint ch;
                    if (XGetClassHint(dpy, w, &ch)) {
                        XFree(ch.res_name);
                        XFree(ch.res_class);
                        return static_cast<uintptr_t>(w);
                    }
                }
            }
        }
    }
    // Рекурсия по детям
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

    // Убираем декорации
    Atom motif = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
    if (motif != None) {
        struct { ulong flags, funcs, deco, mode, status; }
            hints = {2,0,0,0,0};
        XChangeProperty(dpy, child, motif, motif, 32,
                        PropModeReplace,
                        reinterpret_cast<unsigned char*>(&hints), 5);
    }

    XReparentWindow(dpy, child, parent, 0, 0);
    XMapWindow(dpy, child);
    XFlush(dpy);
    XCloseDisplay(dpy);
    qDebug() << "[GodotWidget] Reparented" << winId << "->" << this->winId();
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
                          0, 0, width(), height());
        XFlush(dpy);
        XCloseDisplay(dpy);
    }
#endif
}
