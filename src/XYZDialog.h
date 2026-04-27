#pragma once
#include <QDialog>
#include <QVector3D>

class QDoubleSpinBox;
class QSlider;

class XYZDialog : public QDialog {
    Q_OBJECT
public:
    explicit XYZDialog(const QString& title,
                       double min, double max, double step,
                       QVector3D initial,
                       QWidget* parent = nullptr);

    QVector3D value() const;

signals:
    void valueChanged(QVector3D v);

private:
    // step < 1 → слайдер масштабируется в 10x (шаг 0.1 → целые единицы)
    int    toSlider(double v) const { return int(v / m_step); }
    double fromSlider(int v)  const { return v * m_step; }

    double          m_step;
    QDoubleSpinBox* m_spin[3]{};
    QSlider*        m_slider[3]{};
};
