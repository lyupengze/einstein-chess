#ifndef TOAST_WIDGET_H_
#define TOAST_WIDGET_H_

#include <QLabel>
#include <QTimer>

namespace ui {

class ToastWidget : public QLabel {
    Q_OBJECT
public:
    explicit ToastWidget(QWidget* parent);

    void show_message(const QString& msg, int duration = 2500);

private:
    QTimer* timer_;
};

}  // namespace ui

#endif