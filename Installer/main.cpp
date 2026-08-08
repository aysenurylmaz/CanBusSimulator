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

    // Check for uninstall mode
    if (app.arguments().contains("--uninstall")) {
        QMessageBox::StandardButton reply = QMessageBox::question(nullptr, 
            QObject::tr("Kaldırma Onayı"),
            QObject::tr("CanBusSimulator uygulamasını ve tüm bileşenlerini kaldırmak istediğinize emin misiniz?"),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            // 2. Remove Registry Keys
            QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall", QSettings::NativeFormat);
            settings.remove("CanBusSimulator");
            qDebug() << "Registry keys removed.";

            // 3. Remove Desktop Shortcut
            QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
            QString shortcutPath = desktopPath + QDir::separator() + "CanBusSimulator.lnk";
            if (QFile::exists(shortcutPath)) {
                QFile::remove(shortcutPath);
                qDebug() << "Shortcut removed.";
            }
            
            // Delete folder using a background cmd process
            QString installDir = QCoreApplication::applicationDirPath();
            QString winInstallDir = QDir::toNativeSeparators(installDir);
            
            // Wait 2 seconds, then delete the folder
            QString cmd = QString("ping 127.0.0.1 -n 3 > nul & rmdir /s /q \"%1\"").arg(winInstallDir);
            QProcess::startDetached("cmd.exe", {"/c", cmd});
            
            QMessageBox::information(nullptr, QObject::tr("Kaldırıldı"), QObject::tr("Uygulama başarıyla kaldırıldı."));
        }
        return 0; // Exit without showing installer
    }

    InstallerWizard wizard;
    wizard.show();

    return app.exec();
}
