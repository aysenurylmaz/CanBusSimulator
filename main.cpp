// Uygulaman�n ba�lang�� noktas�. Qt GUI (Aray�z) motorunu ba�lat�r ve MainWindow (Ana Pencere) nesnesini ekrana �izer.

// Projenin baslangic (giris) dosyasidir.
// Uygulama ilk buradan calismaya baslar ve MainWindow (Ana Pencere) arayuzunu ekrana cizer.
#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    // Qt uygulamasini baslatir
    QApplication a(argc, argv);
    
    // Ana pencereyi olustur ve ekranda goster
    MainWindow w;
    w.show();
    
    // Uygulamayi acik tutan ana donguyu baslatir (Kapatilana kadar calisir)
    return a.exec();
}
