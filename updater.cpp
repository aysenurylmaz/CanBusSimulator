// Uzak sunucuya baglanarak versiyon kontrolu yapan ve gerekirse yeni guncellemeyi indirip kuran dosyadir.

// updater.cpp
// Internet uzerinden (veya yerel sunucudan) guncel surumu kontrol etme,
// indirme ve eski .exe dosyasini silip yenisini baslatma islemlerini yapar.
#include "updater.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QDebug>
#include <QCoreApplication>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMessageBox> // MESAJ KUTUSU ICIN EKLENEN KUTUPHANE

Updater::Updater(QObject *parent) : QObject(parent) {
    // Internet/Ag baglantilarini (HTTP Istekleri) yonetecek sinifi baslatiyoruz
    networkManager = new QNetworkAccessManager(this);
}
// --- GUNCELLEME KONTROLU (Adim 1) ---
void Updater::checkForUpdates() {
    
    // Sunucudaki version.json dosyasina (hangi surumun en yeni oldugunu soran dosya) bir HTTP GET istegi atilir.
    QNetworkRequest request(QUrl("http://localhost:8080/version.json"));
    QNetworkReply *reply = networkManager->get(request);

    // Sunucudan cevap (reply) geldiginde (finished) calisacak olan asenkron Lambda fonksiyonu
    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();// Islem bitince hafizayi temizlemek icin silinmek uzere isaretliyoruz
        
        // Eger sunucuya baglanirken bir ag hatasi (Internet yok, sunucu kapali vb.) olduysa:
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "Update check failed:" << reply->errorString();
            // KULLANICIYA GOSTERILEN HATA MESAJI
            QMessageBox::critical(nullptr, "Baglanti Hatasi", "Guncelleme sunucusuna ulasilamadi:\n" + reply->errorString());
            return;
        }
        
        // Sunucudan gelen metin verisini okuyup JSON formatina donusturuyoruz
        QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        
        // Sunucudaki versiyon numarasi su anki versiyondan buyuk mu diye kontrol ediyoruz
        if (obj["latest_version"].toString().toFloat() > currentVersion.toFloat()) {
            qDebug() << "New version found! Downloading from:" << obj["download_url"].toString();
            // Eger buyukse, JSON icindeki "download_url" adresini alip indirme islemini baslatiyoruz
            downloadAndApplyUpdate(obj["download_url"].toString());
        } else {
            // Eger surum ayni veya daha dusukse guncellemeye gerek yoktur
            qDebug() << "Application is up to date.";
            QMessageBox::information(nullptr, "Zaten Guncel", "Uygulamaniz zaten guncel surumde (v" + currentVersion + "). Yeni bir guncelleme bulunmuyor.");
        }
    });
}
// --- GUNCELLEMEYI INDIRME VE UYGULAMA (Adim 2) ---
void Updater::downloadAndApplyUpdate(const QString &fileUrl) {
    //Yeni surum varsa .exe dosyasi indirilmeye baslanir
    QUrl url(fileUrl);
    QNetworkRequest request(url);
    QNetworkReply *reply = networkManager->get(request);

    // Indirme islemi bittiginde calisacak fonksiyon
    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();
        
        // 1. Ag hatasi kontrolu
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "File download network error:" << reply->errorString();
            QMessageBox::critical(nullptr, "Indirme Hatasi", "Dosya indirilirken ag baglantisi koptu:\n" + reply->errorString());
            return;
        }

        // 2. Gercek HTTP Basari (200) kontrolu
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug() << "HTTP Status Code:" << statusCode;
        
        if (statusCode != 200) {
            qDebug() << "Server returned an error instead of the file! Status:" << statusCode;
            QMessageBox::warning(nullptr, "Sunucu Hatasi", "Sunucu dosyayi vermeyi reddetti. Hata Kodu: " + QString::number(statusCode));
            return;
        }

        // 3. Dosya boyutu ve veri kontrolu
        QByteArray fileData = reply->readAll(); // Indirilen tum veriyi RAM'e (fileData icine) aliyoruz
        qDebug() << "Downloaded file size:" << fileData.size() << "bytes";

        if (fileData.size() < 10000) { 
            // Guvenlik Onlemi: Indirilen dosya 10 KB'tan kucukse bu bir .exe dosyasi olamaz
            qDebug() << "CRITICAL ERROR: The downloaded file is too small! It might be corrupted or a text file.";
            QMessageBox::critical(nullptr, "Dosya Hatasi", "Indirilen dosya cok kucuk veya bozuk. Lutfen daha sonra tekrar deneyin.");
            return;
        }

        // Uygulamanin kesin ve tam klasor yolunu (Absolute Path) aliyoruz
        QString appPath = QCoreApplication::applicationFilePath(); 
        QString appDir = QCoreApplication::applicationDirPath();   
        
        // update.exe'yi uygulamanin tam yanina kaydet
        QString updateExePath = QDir(appDir).filePath("update.exe");
        QFile file(updateExePath);
        
        // Dosyayi yazma (WriteOnly) modunda olusturmayi deniyoruz (Klasor izinleri yetersiz olabilir, kontrol sart)
        if (!file.open(QIODevice::WriteOnly)) {
            qDebug() << "Failed to create update.exe! Check folder permissions.";
            QMessageBox::critical(nullptr, "Erisim Hatasi", "Guncelleme dosyasi yazilamadi. Klasor yetkilerini kontrol edin.");
            return;
        }
        // Indirilen veriyi update.exe'nin icine yaz ve dosyayi kapat
        file.write(fileData);
        file.close(); 

        // // --- UYGULAMAYI YENILEME SURECI: BAT dosyasini olustur
        // Uygulamanin kendi kendini silebilmesi icin gecici bir .bat dosyasi yazilir.
        QString batPath = QDir(appDir).filePath("update.bat");
        QFile bat(batPath);
        
        if (bat.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&bat);
            
            QString winAppPath = appPath; winAppPath.replace("/", "\\");
            QString winAppDir = appDir; winAppDir.replace("/", "\\");
            
            // BAT dosyasinin icine komutlari yaziyoruz:
            stream << "@echo off\r\n"                             // Konsolda yazilari gizle
                   << "cd /d \"" << winAppDir << "\"\r\n"         // Programin klasorune git
                   << "timeout /t 2 /nobreak > nul\r\n"           // Programin tamamen kapanmasi icin 2 saniye bekle (ONEMLI!)
                   << "del /f /q \"" << winAppPath << "\"\r\n"    // Eski "CanBusSimulator.exe" dosyasini zorla sil
                   << "ren \"update.exe\" \"CanBusSimulator.exe\"\r\n" // Yeni inen "update.exe"nin adini "CanBusSimulator.exe" yap
                   << "start \"\" \"" << winAppPath << "\"\r\n"   // Yeni guncellenmis uygulamayi baslat
                   << "del \"%~f0\"\r\n"                          // Islem bitince bu BAT dosyasinin KENDI KENDINI SILMESINI sagla
                   << "exit\r\n";                                 // Komut istemini kapat
            bat.close();
        }

        qDebug() << "Update downloaded successfully. Restarting application...";
        
        // Olusturdugumuz bu .bat dosyasini CMD uzerinden, programimizdan bagimsiz (Detached) calisacak sekilde tetikliyoruz.
        QString winBatPath = batPath; winBatPath.replace("/", "\\");
        QProcess::startDetached("cmd.exe", {"/c", winBatPath});
        
        // .bat dosyasi calismaya ve 2 saniye geri saymaya basladi.
        // O geri sayarken biz de mevcut calisan eski uygulamayi hemen kapatiyoruz ki dosya kilidi (file lock) kalksin ve .bat dosyasi silebilsin.
        QCoreApplication::quit(); 
    });
}
