#include "installerutils.h"

#include <QDir>
#include <QFile>
#include <QFilenfo>
#include <QStandardPaths>
// Eger kod Windows'ta derleniyorsa (Q_OS_WN), Windows AP kutuphanelerini dahil et.
// Bu sayede ayni kod Linux'ta derlenirse hata vermez (Cross-platform uyumlulugu).
#ifdef Q_OS_WN
#include <windows.h>
#include <shobjidl.h>
#include <shlguid.h>
#endif

namespace nstallerUtils {
// 1. FONKSYON: DOSYA VE KLASOR KOPYALAMA (RECURSVE - OZYNELEMEL)
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
    // 1. ASAMA: KLASORLER GEZME (RECURSON)
    // Kaynak klasorun icindeki tum alt klasorleri listele 
    foreach (QString dirName, src.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString dstPath = destDir + QDir::separator() + dirName;
        // Kendini tekrar cagir (Recursive Call). Boylece ic ice 10 klasor bile olsa hepsini gezer.
        if (!copyResources(sourceDir + "/" + dirName, dstPath, errorMessage)) {
            return false;
        }
    }
    // 2. ASAMA: DOSYALAR KOPYALAMA
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
// 2. FONKSYON: MASAUSTU KSAYOLU OLUSTURMA (WNDOWS COM AP)
// sletim sisteminin derinliklerine inip standart bir .lnk (Kisayol) dosyasi uretir.
bool createDesktopShortcut(const QString &targetFile, const QString &shortcutName, const QString &workingDirectory, QString &errorMessage)
{
#ifdef Q_OS_WN
    HRESULT hres;
    ShellLink* psl;

    // 1. COM (Component Object Model) kutuphanesini baslatiyoruz.
    Conitialize(NULL);

    // 2. Bellekte bos bir Kisayol objesi (ShellLink) olusturuyoruz.
    hres = CoCreatenstance(CLSD_ShellLink, NULL, CLSCTX_NPROC_SERVER, D_ShellLink, (LPVOD*)&psl);
    if (SUCCEEDED(hres)) {
        PersistFile* ppf;

        // 3. Kisayolun ozelliklerini atiyoruz.
        // Windows AP, Qt'nin standart String'lerini anlamaz. Bu yuzden "toStdWString().c_str()" 
        // kullanarak metinleri Windows'un anlayacagi "Genis Karakterli" (Wide String) formata ceviriyoruz.
        psl->SetPath(QDir::toNativeSeparators(targetFile).toStdWString().c_str());
        psl->SetWorkingDirectory(QDir::toNativeSeparators(workingDirectory).toStdWString().c_str());

        // 4. Hafizadaki bu objeyi diske yazabilmek icin PersistFile (Kalici Dosya) yetenegini cagiriyoruz.
        hres = psl->Querynterface(D_PersistFile, (LPVOD*)&ppf);

        if (SUCCEEDED(hres)) {
            // Qt yardimiyla isletim sisteminin Masaustu (Desktop) yolunu buluyoruz (Orn: C:\Users\Kullanici\Desktop)
            QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
            QString shortcutPath = desktopPath + QDir::separator() + shortcutName + ".lnk";
            shortcutPath = QDir::toNativeSeparators(shortcutPath);

            // 5. Kisayolu masaustune fiziksel bir dosya (.lnk) olarak kaydediyoruz.
            hres = ppf->Save(shortcutPath.toStdWString().c_str(), TRUE);
            if (FALED(hres)) {
                errorMessage = "Failed to save the shortcut to desktop.";
            }
            // simiz bitince PersistFile objesini hafizadan serbest birakiyoruz (Memory Leak onlemi).
            ppf->Release();
        } else {
            errorMessage = "Failed to query PersistFile interface.";
        }
        // simiz bitince ShellLink objesini serbest birakiyoruz.
        psl->Release();
    } else {
        errorMessage = "Failed to create ShellLink instance.";
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

} // namespace nstallerUtils
