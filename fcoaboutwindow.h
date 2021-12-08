#ifndef FCOABOUTWINDOW_H
#define FCOABOUTWINDOW_H

#include <QDialog>

namespace Ui {
class fcoAboutWindow;
}

class fcoAboutWindow : public QDialog
{
    Q_OBJECT

public:
    explicit fcoAboutWindow(QWidget *parent = 0);
    ~fcoAboutWindow();

private:
    Ui::fcoAboutWindow *ui;
};

#endif // FCOABOUTWINDOW_H
