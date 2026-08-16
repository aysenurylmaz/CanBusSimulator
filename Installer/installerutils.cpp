#include "installerutils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
// Eger kod Windows'ta derleniyorsa (Q_OS_WIN), Windows API kutuphanelerini dahil et.
// Bu sayede ayni kod Linux'ta derlenirse hata vermez (Cross-platform uyumlulugu).
#ifdef Q_OS_WIN
#include <windows.h>
#include <shobjidl.h>
#include <shlguid.h>
#endif

namespace InstallerUtils {
// 1. FONKSIYON: DOSYA VE KLASOR KOPYALAMA (RECURSIVE - OZYINELEMELI)
// Bu fonksiyon, kaynagin (:/payload) icindeki tum klasorleri ve dosyalari 
// alt klasorleriyle birlikte hedef dizine kopyalar.
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
    // 1. ASAMA: KLASORLERI GEZME (RECURSION)
    // Kaynak klasorun icindeki tum alt klasorleri listele 
    foreach (QString dirName, src.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString dstPath = destDir + QDir::separator() + dirName;
        // Kendini tekrar cagir (Recursive Call). Boylece ic ice 10 klasor bile olsa hepsini gezer.
        if (!copyResources(sourceDir + "/" + dirName, dstPath, errorMessage)) {
            return false;
        }
    }
    // 2. ASAMA: DOSYALARI KOPYALAMA
    // Sadece dosyalari listele
    foreach (QString fileName, src.entryList(QDir::Files)) {
        QString srcFilePath = sourceDir + "/" + fileName;
        QString dstFilePath = destDir + QDir::separator() + fileName;

        // Eger hedefte ayni isimde bir dosya varsa (Orn: uzerine kurulum yapiliyorsa)
        // Kopyalamadan once eski dosyayi sil.
        if (QFile::exists(dstFilePath)) {
            if (!QFile::remove(dstFilePath)) {
                errorMessage = "Could not overwrite existing file: " + dstFilePath;
                return false;
            }
        }
        // Asil kopyalama islemi burada yapiliyor.
        if (!QFile::copy(srcFilePath, dstFilePath)) {
            errorMessage = "Could not copy file to: " + dstFilePath;
            return false;
        }

        // Qt'nin .qrc (Resource) sisteminden cikan dosyalar isletim sistemine "Salt Okunur" (Read-Only) olarak cikar.
        // Eger bunlari yazilabilir (Writable) yapmazsak, ileride Uninstaller (Kaldirici) bu dosyalari silemez!
        // Bu yuzden kopya dosyanin izinlerini "Okuma ve Yazma" olarak guncelliyoruz.
        QFile::setPermissions(dstFilePath, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);
    }

    return true;
}
// 2. FONKSIYON: MASAUSTU KISAYOLU OLUSTURMA (WINDOWS COM API)
// Isletim sisteminin derinliklerine inip standart bir .lnk (Kisayol) dosyasi uretir.
bool createDesktopShortcut(const QString &targetFile, const QString &shortcutName, const QString &workingDirectory, QString &errorMessage)
{
#ifdef Q_OS_WIN
    HRESULT hres;
    IShellLink* psl;

    // 1. COM (Component Object Model) kutuphanesini baslatiyoruz.
    CoInitialize(NULL);

    // 2. Bellekte bos bir Kisayol objesi (IShellLink) olusturuyoruz.
    hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl);
    if (SUCCEEDED(hres)) {
        IPersistFile* ppf;

        // 3. Kisayolun ozelliklerini atiyoruz.
        // Windows API, Qt'nin standart String'lerini anlamaz. Bu yuzden "toStdWString().c_str()" 
        // kullanarak metinleri Windows'un anlayacagi "Genis Karakterli" (Wide String) formata ceviriyoruz.
        psl->SetPath(QDir::toNativeSeparators(targetFile).toStdWString().c_str());
        psl->SetWorkingDirectory(QDir::toNativeSeparators(workingDirectory).toStdWString().c_str());

        // 4. Hafizadaki bu objeyi diske yazabilmek icin IPersistFile (Kalici Dosya) yetenegini cagiriyoruz.
        hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);

        if (SUCCEEDED(hres)) {
            // Qt yardimiyla isletim sisteminin Masaustu (Desktop) yolunu buluyoruz (Orn: C:\Users\Kullanici\Desktop)
            QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
            QString shortcutPath = desktopPath + QDir::separator() + shortcutName + ".lnk";
            shortcutPath = QDir::toNativeSeparators(shortcutPath);

            // 5. Kisayolu masaustune fiziksel bir dosya (.lnk) olarak kaydediyoruz.
            hres = ppf->Save(shortcutPath.toStdWString().c_str(), TRUE);
            if (FAILED(hres)) {
                errorMessage = "Failed to save the shortcut to desktop.";
            }
            // Isimiz bitince IPersistFile objesini hafizadan serbest birakiyoruz (Memory Leak onlemi).
            ppf->Release();
        } else {
            errorMessage = "Failed to query IPersistFile interface.";
        }
        // Isimiz bitince IShellLink objesini serbest birakiyoruz.
        psl->Release();
    } else {
        errorMessage = "Failed to create IShellLink instance.";
    }
// COM sistemini kapatiyoruz.
    CoUninitialize();
    
    return SUCCEEDED(hres);
#else
    // Eger program Windows harici bir sistemde (Linux/Mac) derlenirse, 
    // sistemin cokmemesi icin Qt'nin standart Sembolik Link (Symlink) olusturma fonksiyonunu kullandik (Fallback).
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
