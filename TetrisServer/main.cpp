#include <QCoreApplication>
#include "server.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    Server server; // 啟動伺服器

    return a.exec();
}
