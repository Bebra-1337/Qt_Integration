#pragma once
#include <QObject>
#include <QString>
#include <QSize>
#include <memory>

struct GodotInitConfig {
    QString   projectPath;
    QString   mainScene;
    QSize     initialSize   = {1280, 720};
    uintptr_t externalWindowId = 0;
};

class GodotRuntime : public QObject {
    Q_OBJECT

public:
    explicit GodotRuntime(QObject* parent = nullptr);
    ~GodotRuntime() override;

    bool   initialize(const GodotInitConfig& config);
    void   startAsync();
    void   shutdown();
    bool   isRunning() const { return m_running; }
    qint64 godotPid()  const;

signals:
    void initialized();
    void shutdownComplete();
    void engineError(const QString& message);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_running     = false;
    bool m_initialized = false;
};
