#include <QApplication>
#include <QStyleFactory>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QFile>
#include <QProcess>
#include <QDir>
#include "installerwizard.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create("Fusion"));

    // 1. AŞAMA: UNINSTALLER (KALDIRICI) KONTROLÜ
    // Aynı .exe dosyası hem kurucu hem kaldırıcı olarak çalışıyor.
    // İşletim sistemi bu programı "--uninstall" argümanı ile çalıştırdıysa, 
    // kurulum arayüzünü HİÇ GÖSTERMEDEN doğrudan kaldırma moduna geçiyoruz.
    if (app.arguments().contains("--uninstall")) {
        // Kullanıcıya yanlışlıkla tıklama ihtimaline karşı son bir onay soruyoruz.
        QMessageBox::StandardButton reply = QMessageBox::question(nullptr, 
            QObject::tr("Kaldırma Onayı"),
            QObject::tr("CanBusSimulator uygulamasını ve tüm bileşenlerini kaldırmak istediğinize emin misiniz?"),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            // 2. AŞAMA: REGISTRY (KAYIT DEFTERİ) TEMİZLİĞİ
            // Kurulum yaparken Windows'un Denetim Masası listesine eklediğimiz kaydı bulup,
            // "CanBusSimulator" klasörünü tamamen siliyoruz (İz bırakmıyoruz).
            QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall", QSettings::NativeFormat);
            settings.remove("CanBusSimulator");
            qDebug() << "Registry keys removed.";

            // 3. AŞAMA: MASAÜSTÜ KISAYOL TEMİZLİĞİ
            // Kullanıcının masaüstü yolunu bulup, orada oluşturduğumuz .lnk dosyasını siliyoruz.
            QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
            QString shortcutPath = desktopPath + QDir::separator() + "CanBusSimulator.lnk";
            if (QFile::exists(shortcutPath)) {
                QFile::remove(shortcutPath);
                qDebug() << "Shortcut removed.";
            }
            
            // 4. AŞAMA: KENDİNİ SİLME ALGORİTMASI (BATON TESLİMİ)
            // Bir program Windows'ta çalışırken kendi klasörünü veya .exe dosyasını SİLEMEZ!
            // Bunu aşmak için System Hack (Sistem Hilesi) kullanıyoruz:
            
            // Çalışan programın bulunduğu klasör yolunu alıyoruz.
            QString installDir = QCoreApplication::applicationDirPath();
            QString winInstallDir = QDir::toNativeSeparators(installDir);
            
            // Arka planda gizli bir CMD (Terminal) komutu hazırlıyoruz:
            // "ping 127.0.0.1 -n 3" komutu, CMD'nin işlemi yapmadan önce yaklaşık 2-3 saniye BEKLEMESİNİ sağlar.
            // Bekleme bitince "rmdir /s /q" komutu ile klasör ve içindeki her şey ( .exe dahil) sessizce ve zorla silinir.
            QString cmd = QString("ping 127.0.0.1 -n 3 > nul & rmdir /s /q \"%1\"").arg(winInstallDir);
            QProcess::startDetached("cmd.exe", {"/c", cmd});
            
            QMessageBox::information(nullptr, QObject::tr("Kaldırıldı"), QObject::tr("Uygulama başarıyla kaldırıldı."));
        }
        return 0; // Kaldırma işlemi tamamlandıktan sonra programdan çıkıyoruz.
    }
// 6. AŞAMA: NORMAL KURULUM MODU
    // Eğer "--uninstall" parametresi YOKSA, demek ki kullanıcı programı kurmak istiyor.
    // O zaman kurulum sihirbazını (QWizard) yaratıp ekranda gösteriyoruz.
    InstallerWizard wizard;
    wizard.show();

    return app.exec();
}
