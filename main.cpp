#include "fcoeditorwindow.h"
#include <QApplication>

using namespace std;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    fcoEditorWindow w;
    w.show();

    if (argc > 1)
    {
        w.passArgument(std::string(argv[1]));
    }

    return a.exec();
}
