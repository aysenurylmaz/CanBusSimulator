// Uygulamanin baslangic noktasi. Qt GU (Arayuz) motorunu baslatir ve MainWindow (Ana Pencere) nesnesini ekrana cizer.

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
