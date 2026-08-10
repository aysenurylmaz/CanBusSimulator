#include "installerutils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
// Eğer kod Windows'ta derleniyorsa (Q_OS_WIN), Windows API kütüphanelerini dahil et.
// Bu sayede aynı kod Linux'ta derlenirse hata vermez (Cross-platform uyumluluğu).
#ifdef Q_OS_WIN
#include <windows.h>
#include <shobjidl.h>
#include <shlguid.h>
#endif

namespace InstallerUtils {
// 1. FONKSİYON: DOSYA VE KLASÖR KOPYALAMA (RECURSIVE - ÖZYİNELEMELİ)
// Bu fonksiyon, kaynağın (:/payload) içindeki tüm klasörleri ve dosyaları 
// alt klasörleriyle birlikte hedef dizine kopyalar.
bool copyResources(const QString &sourceDir, const QString &destDir, QString &errorMessage)
{
    QDir src(sourceDir);
    if (!src.exists()) {
        errorMessage = "Source resource directory does not exist: " + sourceDir;
        return false;
    }

    QDir dst(destDir);
    if (!dst.exists() && !dst.mkpath(".")) {
        errorMessage = "Could not create destination directory: " + destDir;
        return false;
    }
    // 1. AŞAMA: KLASÖRLERİ GEZME (RECURSION)
    // Kaynak klasörün içindeki tüm alt klasörleri listele 
    foreach (QString dirName, src.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString dstPath = destDir + QDir::separator() + dirName;
        // Kendini tekrar çağır (Recursive Call). Böylece iç içe 10 klasör bile olsa hepsini gezer.
        if (!copyResources(sourceDir + "/" + dirName, dstPath, errorMessage)) {
            return false;
        }
    }
    // 2. AŞAMA: DOSYALARI KOPYALAMA
    // Sadece dosyaları listele
    foreach (QString fileName, src.entryList(QDir::Files)) {
        QString srcFilePath = sourceDir + "/" + fileName;
        QString dstFilePath = destDir + QDir::separator() + fileName;

        // Eğer hedefte aynı isimde bir dosya varsa (Örn: üzerine kurulum yapılıyorsa)
        // Kopyalamadan önce eski dosyayı sil.
        if (QFile::exists(dstFilePath)) {
            if (!QFile::remove(dstFilePath)) {
                errorMessage = "Could not overwrite existing file: " + dstFilePath;
                return false;
            }
        }
        // Asıl kopyalama işlemi burada yapılıyor.
        if (!QFile::copy(srcFilePath, dstFilePath)) {
            errorMessage = "Could not copy file to: " + dstFilePath;
            return false;
        }

        // Qt'nin .qrc (Resource) sisteminden çıkan dosyalar işletim sistemine "Salt Okunur" (Read-Only) olarak çıkar.
        // Eğer bunları yazılabilir (Writable) yapmazsak, ileride Uninstaller (Kaldırıcı) bu dosyaları silemez!
        // Bu yüzden kopya dosyanın izinlerini "Okuma ve Yazma" olarak güncelliyoruz.
        QFile::setPermissions(dstFilePath, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);
    }

    return true;
}
// 2. FONKSİYON: MASAÜSTÜ KISAYOLU OLUŞTURMA (WINDOWS COM API)
// İşletim sisteminin derinliklerine inip standart bir .lnk (Kısayol) dosyası üretir.
bool createDesktopShortcut(const QString &targetFile, const QString &shortcutName, const QString &workingDirectory, QString &errorMessage)
{
#ifdef Q_OS_WIN
    HRESULT hres;
    IShellLink* psl;

    // 1. COM (Component Object Model) kütüphanesini başlatıyoruz.
    CoInitialize(NULL);

    // 2. Bellekte boş bir Kısayol objesi (IShellLink) oluşturuyoruz.
    hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl);
    if (SUCCEEDED(hres)) {
        IPersistFile* ppf;

        // 3. Kısayolun özelliklerini atıyoruz.
        // Windows API, Qt'nin standart String'lerini anlamaz. Bu yüzden "toStdWString().c_str()" 
        // kullanarak metinleri Windows'un anlayacağı "Geniş Karakterli" (Wide String) formata çeviriyoruz.
        psl->SetPath(QDir::toNativeSeparators(targetFile).toStdWString().c_str());
        psl->SetWorkingDirectory(QDir::toNativeSeparators(workingDirectory).toStdWString().c_str());

        // 4. Hafızadaki bu objeyi diske yazabilmek için IPersistFile (Kalıcı Dosya) yeteneğini çağırıyoruz.
        hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);

        if (SUCCEEDED(hres)) {
            // Qt yardımıyla işletim sisteminin Masaüstü (Desktop) yolunu buluyoruz (Örn: C:\Users\Kullanici\Desktop)
            QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
            QString shortcutPath = desktopPath + QDir::separator() + shortcutName + ".lnk";
            shortcutPath = QDir::toNativeSeparators(shortcutPath);

            // 5. Kısayolu masaüstüne fiziksel bir dosya (.lnk) olarak kaydediyoruz.
            hres = ppf->Save(shortcutPath.toStdWString().c_str(), TRUE);
            if (FAILED(hres)) {
                errorMessage = "Failed to save the shortcut to desktop.";
            }
            // İşimiz bitince IPersistFile objesini hafızadan serbest bırakıyoruz (Memory Leak önlemi).
            ppf->Release();
        } else {
            errorMessage = "Failed to query IPersistFile interface.";
        }
        // İşimiz bitince IShellLink objesini serbest bırakıyoruz.
        psl->Release();
    } else {
        errorMessage = "Failed to create IShellLink instance.";
    }
// COM sistemini kapatıyoruz.
    CoUninitialize();
    
    return SUCCEEDED(hres);
#else
    // Eğer program Windows harici bir sistemde (Linux/Mac) derlenirse, 
    // sistemin çökmemesi için Qt'nin standart Sembolik Link (Symlink) oluşturma fonksiyonunu kullandık (Fallback).
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString shortcutPath = desktopPath + QDir::separator() + shortcutName;
    if (QFile::link(targetFile, shortcutPath)) {
        return true;
    } else {
        errorMessage = "Failed to create link.";
        return false;
    }
#endif
}

} // namespace InstallerUtils
