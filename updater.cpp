// updater.cpp
// İnternet üzerinden (veya yerel sunucudan) güncel sürümü kontrol etme,
// indirme ve eski .exe dosyasını silip yenisini başlatma işlemlerini yapar.
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
#include <QMessageBox> // MESAJ KUTUSU İÇİN EKLENEN KÜTÜPHANE

Updater::Updater(QObject *parent) : QObject(parent) {
    // İnternet/Ağ bağlantılarını (HTTP İstekleri) yönetecek sınıfı başlatıyoruz
    networkManager = new QNetworkAccessManager(this);
}
// --- GÜNCELLEME KONTROLÜ (Adım 1) ---
void Updater::checkForUpdates() {
    
    // Sunucudaki version.json dosyasına (hangi sürümün en yeni olduğunu soran dosya) bir HTTP GET isteği atılır.
    QNetworkRequest request(QUrl("http://localhost:8080/version.json"));
    QNetworkReply *reply = networkManager->get(request);

    // Sunucudan cevap (reply) geldiğinde (finished) çalışacak olan asenkron Lambda fonksiyonu
    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();// İşlem bitince hafızayı temizlemek için silinmek üzere işaretliyoruz
        
        // Eğer sunucuya bağlanırken bir ağ hatası (İnternet yok, sunucu kapalı vb.) olduysa:
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "Update check failed:" << reply->errorString();
            // KULLANICIYA GÖSTERİLEN HATA MESAJI
            QMessageBox::critical(nullptr, "Bağlanti Hatasi", "Güncelleme sunucusuna ulaşilamadi:\n" + reply->errorString());
            return;
        }
        
        // Sunucudan gelen metin verisini okuyup JSON formatına dönüştürüyoruz
        QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        
        // Sunucudaki versiyon numarası şu anki versiyondan büyük mü diye kontrol ediyoruz
        if (obj["latest_version"].toString().toFloat() > currentVersion.toFloat()) {
            qDebug() << "New version found! Downloading from:" << obj["download_url"].toString();
            // Eğer büyükse, JSON içindeki "download_url" adresini alıp indirme işlemini başlatıyoruz
            downloadAndApplyUpdate(obj["download_url"].toString());
        } else {
            // Eğer sürüm aynı veya daha düşükse güncellemeye gerek yoktur
            qDebug() << "Application is up to date.";
            QMessageBox::information(nullptr, "Zaten Güncel", "Uygulamanız zaten güncel sürümde (v" + currentVersion + "). Yeni bir güncelleme bulunmuyor.");
        }
    });
}
// --- GÜNCELLEMEYİ İNDİRME VE UYGULAMA (Adım 2) ---
void Updater::downloadAndApplyUpdate(const QString &fileUrl) {
    //Yeni sürüm varsa .exe dosyası indirilmeye başlanır
    QUrl url(fileUrl);
    QNetworkRequest request(url);
    QNetworkReply *reply = networkManager->get(request);

    // İndirme işlemi bittiğinde çalışacak fonksiyon
    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();
        
        // 1. Ağ hatasi kontrolü
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "File download network error:" << reply->errorString();
            QMessageBox::critical(nullptr, "İndirme Hatasi", "Dosya indirilirken ağ bağlantisi koptu:\n" + reply->errorString());
            return;
        }

        // 2. Gerçek HTTP Başari (200) kontrolü
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug() << "HTTP Status Code:" << statusCode;
        
        if (statusCode != 200) {
            qDebug() << "Server returned an error instead of the file! Status:" << statusCode;
            QMessageBox::warning(nullptr, "Sunucu Hatasi", "Sunucu dosyayi vermeyi reddetti. Hata Kodu: " + QString::number(statusCode));
            return;
        }

        // 3. Dosya boyutu ve veri kontrolü
        QByteArray fileData = reply->readAll(); // İndirilen tüm veriyi RAM'e (fileData içine) alıyoruz
        qDebug() << "Downloaded file size:" << fileData.size() << "bytes";

        if (fileData.size() < 10000) { 
            // Güvenlik Önlemi: İndirilen dosya 10 KB'tan küçükse bu bir .exe dosyası olamaz
            qDebug() << "CRITICAL ERROR: The downloaded file is too small! It might be corrupted or a text file.";
            QMessageBox::critical(nullptr, "Dosya Hatasi", "İndirilen dosya çok küçük veya bozuk. Lütfen daha sonra tekrar deneyin.");
            return;
        }

        // Uygulamanin kesin ve tam klasör yolunu (Absolute Path) aliyoruz
        QString appPath = QCoreApplication::applicationFilePath(); 
        QString appDir = QCoreApplication::applicationDirPath();   
        
        // update.exe'yi uygulamanin tam yanina kaydet
        QString updateExePath = QDir(appDir).filePath("update.exe");
        QFile file(updateExePath);
        
        // Dosyayı yazma (WriteOnly) modunda oluşturmayı deniyoruz (Klasör izinleri yetersiz olabilir, kontrol şart)
        if (!file.open(QIODevice::WriteOnly)) {
            qDebug() << "Failed to create update.exe! Check folder permissions.";
            QMessageBox::critical(nullptr, "Erişim Hatasi", "Güncelleme dosyasi yazilamadi. Klasör yetkilerini kontrol edin.");
            return;
        }
        // İndirilen veriyi update.exe'nin içine yaz ve dosyayı kapat
        file.write(fileData);
        file.close(); 

        // // --- UYGULAMAYI YENİLEME SÜRECİ: BAT dosyasini oluştur
        // Uygulamanın kendi kendini silebilmesi için geçici bir .bat dosyası yazılır.
        QString batPath = QDir(appDir).filePath("update.bat");
        QFile bat(batPath);
        
        if (bat.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&bat);
            
            QString winAppPath = appPath; winAppPath.replace("/", "\\");
            QString winAppDir = appDir; winAppDir.replace("/", "\\");
            
            // BAT dosyasının içine komutları yazıyoruz:
            stream << "@echo off\r\n"                             // Konsolda yazıları gizle
                   << "cd /d \"" << winAppDir << "\"\r\n"         // Programın klasörüne git
                   << "timeout /t 2 /nobreak > nul\r\n"           // Programın tamamen kapanması için 2 saniye bekle (ÖNEMLİ!)
                   << "del /f /q \"" << winAppPath << "\"\r\n"    // Eski "CanBusSimulator.exe" dosyasını zorla sil
                   << "ren \"update.exe\" \"CanBusSimulator.exe\"\r\n" // Yeni inen "update.exe"nin adını "CanBusSimulator.exe" yap
                   << "start \"\" \"" << winAppPath << "\"\r\n"   // Yeni güncellenmiş uygulamayı başlat
                   << "del \"%~f0\"\r\n"                          // İşlem bitince bu BAT dosyasının KENDİ KENDİNİ SİLMESİNİ sağla
                   << "exit\r\n";                                 // Komut istemini kapat
            bat.close();
        }

        qDebug() << "Update downloaded successfully. Restarting application...";
        
        // Oluşturduğumuz bu .bat dosyasını CMD üzerinden, programımızdan bağımsız (Detached) çalışacak şekilde tetikliyoruz.
        QString winBatPath = batPath; winBatPath.replace("/", "\\");
        QProcess::startDetached("cmd.exe", {"/c", winBatPath});
        
        // .bat dosyası çalışmaya ve 2 saniye geri saymaya başladı.
        // O geri sayarken biz de mevcut çalışan eski uygulamayı hemen kapatıyoruz ki dosya kilidi (file lock) kalksın ve .bat dosyası silebilsin.
        QCoreApplication::quit(); 
    });
}