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

    // 1. ASAMA: UNNSTALLER (KALDRC) KONTROLU
    // Ayni .exe dosyasi hem kurucu hem kaldirici olarak calisiyor.
    // sletim sistemi bu programi "--uninstall" argumani ile calistirdiysa, 
    // kurulum arayuzunu HC GOSTERMEDEN dogrudan kaldirma moduna geciyoruz.
    if (app.arguments().contains("--uninstall")) {
        // Kullaniciya yanlislikla tiklama ihtimaline karsi son bir onay soruyoruz.
        QMessageBox::StandardButton reply = QMessageBox::question(nullptr, 
            QObject::tr("Kaldirma Onayi"),
            QObject::tr("CanBusSimulator uygulamasini ve tum bilesenlerini kaldirmak istediginize emin misiniz?"),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            // 2. ASAMA: REGSTRY (KAYT DEFTER) TEMZLG
            // Kurulum yaparken Windows'un Denetim Masasi listesine ekledigimiz kaydi bulup,
            // "CanBusSimulator" klasorunu tamamen siliyoruz (z birakmiyoruz).
            QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall", QSettings::NativeFormat);
            settings.remove("CanBusSimulator");
            qDebug() << "Registry keys removed.";

            // 3. ASAMA: MASAUSTU KSAYOL TEMZLG
            // Kullanicinin masaustu yolunu bulup, orada olusturdugumuz .lnk dosyasini siliyoruz.
            QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
            QString shortcutPath = desktopPath + QDir::separator() + "CanBusSimulator.lnk";
            if (QFile::exists(shortcutPath)) {
                QFile::remove(shortcutPath);
                qDebug() << "Shortcut removed.";
            }
            
            // 4. ASAMA: KENDN SLME ALGORTMAS (BATON TESLM)
            // Bir program Windows'ta calisirken kendi klasorunu veya .exe dosyasini SLEMEZ!
            // Bunu asmak icin System Hack (Sistem Hilesi) kullaniyoruz:
            
            // Calisan programin bulundugu klasor yolunu aliyoruz.
            QString installDir = QCoreApplication::applicationDirPath();
            QString winnstallDir = QDir::toNativeSeparators(installDir);
            
            // Arka planda gizli bir CMD (Terminal) komutu hazirliyoruz:
            // "ping 127.0.0.1 -n 3" komutu, CMD'nin islemi yapmadan once yaklasik 2-3 saniye BEKLEMESN saglar.
            // Bekleme bitince "rmdir /s /q" komutu ile klasor ve icindeki her sey ( .exe dahil) sessizce ve zorla silinir.
            QString cmd = QString("ping 127.0.0.1 -n 3 > nul & rmdir /s /q \"%1\"").arg(winnstallDir);
            QProcess::startDetached("cmd.exe", {"/c", cmd});
            
            QMessageBox::information(nullptr, QObject::tr("Kaldirildi"), QObject::tr("Uygulama basariyla kaldirildi."));
        }
        return 0; // Kaldirma islemi tamamlandiktan sonra programdan cikiyoruz.
    }
// 6. ASAMA: NORMAL KURULUM MODU
    // Eger "--uninstall" parametresi YOKSA, demek ki kullanici programi kurmak istiyor.
    // O zaman kurulum sihirbazini (QWizard) yaratip ekranda gosteriyoruz.
    nstallerWizard wizard;
    wizard.show();

    return app.exec();
}
