// Projenin başlangıç (giriş) dosyasıdır.
// Uygulama ilk buradan çalışmaya başlar ve MainWindow (Ana Pencere) arayüzünü ekrana çizer.
#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    // Qt uygulamasını başlatır
    QApplication a(argc, argv);
    
    // Ana pencereyi oluştur ve ekranda göster
    MainWindow w;
    w.show();
    
    // Uygulamayı açık tutan ana döngüyü başlatır (Kapatılana kadar çalışır)
    return a.exec();
}