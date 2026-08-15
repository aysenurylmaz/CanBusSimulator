#include <QFile>
#include <QDebug>
#include <QCoreApplication>

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);
    bool res = QFile::remove("C:/Projeler/CanBusWebPlatform/Shared/parsed_dbc.json");
    qDebug() << "Delete result:" << res;
    return 0;
}
