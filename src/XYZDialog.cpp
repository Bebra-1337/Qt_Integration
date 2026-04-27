#include "XYZDialog.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QDialogButtonBox>

XYZDialog::XYZDialog(const QString& title,
                     double min, double max, double step,
                     QVector3D initial,
                     QWidget* parent)
    : QDialog(parent), m_step(step)
{
    setWindowTitle(title);
    setModal(true);
    setMinimumWidth(360);

    auto* layout = new QVBoxLayout(this);

    const char*  labels[] = {"X:", "Y:", "Z:"};
    const double inits[]  = {double(initial.x()), double(initial.y()), double(initial.z())};
    const int    sMin     = toSlider(min);
    const int    sMax     = toSlider(max);

    for (int i = 0; i < 3; ++i) {
        auto* row    = new QHBoxLayout;
        auto* label  = new QLabel(labels[i]);
        label->setFixedWidth(20);

        auto* slider = new QSlider(Qt::Horizontal);
        slider->setRange(sMin, sMax);
        slider->setValue(toSlider(inits[i]));

        auto* spin = new QDoubleSpinBox;
        spin->setRange(min, max);
        spin->setSingleStep(step);
        spin->setDecimals(step < 1.0 ? 1 : 0);
        spin->setValue(inits[i]);
        spin->setFixedWidth(80);

        m_slider[i] = slider;
        m_spin[i]   = spin;

        // Слайдер → спинбокс
        connect(slider, &QSlider::valueChanged, this, [this, spin, i](int v) {
            const double d = fromSlider(v);
            QSignalBlocker b(spin);
            spin->setValue(d);
            emit valueChanged(value());
        });

        // Спинбокс → слайдер
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, slider, i](double d) {
            QSignalBlocker b(slider);
            slider->setValue(toSlider(d));
            emit valueChanged(value());
        });

        row->addWidget(label);
        row->addWidget(slider, 1);
        row->addWidget(spin);
        layout->addLayout(row);
    }

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QVector3D XYZDialog::value() const {
    return {float(m_spin[0]->value()),
            float(m_spin[1]->value()),
            float(m_spin[2]->value())};
}
