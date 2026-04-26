#include "GodotRuntime.h"

#include <QProcess>
#include <QStandardPaths>
#include <QDebug>

struct GodotRuntime::Impl {
    QProcess* process = nullptr;
    qint64    pid     = 0;
};

GodotRuntime::GodotRuntime(QObject* parent)
    : QObject(parent), m_impl(std::make_unique<Impl>()) {}

GodotRuntime::~GodotRuntime() {
    if (m_running) shutdown();
}

bool GodotRuntime::initialize(const GodotInitConfig& config) {
    if (m_initialized) return false;

    qDebug() << "[GodotRuntime] Starting Godot";
    qDebug() << "  Project:" << config.projectPath;

    const QString bin = QStandardPaths::findExecutable("godot");
    if (bin.isEmpty()) {
        emit engineError("'godot' not found in PATH");
        return false;
    }

    m_impl->process = new QProcess(this);

    QStringList args;
    args << "--path" << config.projectPath;
    if (!config.mainScene.isEmpty())
        args << config.mainScene;

    m_impl->process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_impl->process, &QProcess::started, this, [this]() {
        m_impl->pid = m_impl->process->processId();
        qDebug() << "[GodotRuntime] PID:" << m_impl->pid;
        m_initialized = true;
        m_running     = true;
        emit initialized();
    });

    connect(m_impl->process, &QProcess::readyReadStandardOutput, this, [this]() {
        const QByteArray out = m_impl->process->readAllStandardOutput();
        for (const auto& line : out.split('\n')) {
            if (!line.trimmed().isEmpty())
                qDebug() << "[Godot]" << line.trimmed();
        }
    });

    connect(m_impl->process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) {
        qDebug() << "[GodotRuntime] Exited with code" << code;
        m_running = false;
        emit shutdownComplete();
    });

    connect(m_impl->process, &QProcess::errorOccurred,
            this, [this](QProcess::ProcessError e) {
        emit engineError(QString("Process error: %1").arg(static_cast<int>(e)));
    });

    m_impl->process->start(bin, args);
    if (!m_impl->process->waitForStarted(5000)) {
        emit engineError("Godot failed to start within 5 seconds");
        return false;
    }

    return true;
}

qint64 GodotRuntime::godotPid() const {
    return m_impl ? m_impl->pid : 0;
}

void GodotRuntime::startAsync() {
    // В subprocess режиме Godot уже работает — ничего не нужно
}

void GodotRuntime::shutdown() {
    m_running = false;
    if (m_impl->process &&
        m_impl->process->state() != QProcess::NotRunning) {
        m_impl->process->terminate();
        if (!m_impl->process->waitForFinished(3000))
            m_impl->process->kill();
    }
}
