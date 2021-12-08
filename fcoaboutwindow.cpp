#include "fcoaboutwindow.h"
#include "ui_fcoaboutwindow.h"

fcoAboutWindow::fcoAboutWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::fcoAboutWindow)
{
    ui->setupUi(this);
}

fcoAboutWindow::~fcoAboutWindow()
{
    delete ui;
}
