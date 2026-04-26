#pragma once
#include <QWidget>
#include <memory>

class GodotRuntime;
class GodotBridge;

class GodotWidget : public QWidget {
    Q_OBJECT

public:
    explicit GodotWidget(QWidget* parent = nullptr);
    ~GodotWidget() override;

    bool startGodot(const QString& projectPath,
                    const QString& mainScene = QString());
    void stopGodot();
    bool isGodotRunning() const;

    // Доступ к bridge для подключения сигналов из MainWindow
    GodotBridge* bridge() const { return m_bridge.get(); }

signals:
    void godotReady();
    void godotStopped();
    void godotError(const QString& msg);

protected:
    void resizeEvent(QResizeEvent* event) override;
    QPaintEngine* paintEngine() const override { return nullptr; }

private slots:
    void onGodotInitialized();

private:
    void embedGodotWindow(uintptr_t winId);
    void resizeEmbeddedWindow();
    uintptr_t findWindowByPid(ulong pid);

    std::unique_ptr<GodotRuntime> m_runtime;
    std::unique_ptr<GodotBridge>  m_bridge;

    uintptr_t m_embeddedWinId      = 0;
    bool      m_embedded           = false;
    int       m_findWindowAttempts = 0;
};
