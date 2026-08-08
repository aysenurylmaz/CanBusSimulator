#include "installerutils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shobjidl.h>
#include <shlguid.h>
#endif

namespace InstallerUtils {

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

    foreach (QString dirName, src.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString dstPath = destDir + QDir::separator() + dirName;
        if (!copyResources(sourceDir + "/" + dirName, dstPath, errorMessage)) {
            return false;
        }
    }

    foreach (QString fileName, src.entryList(QDir::Files)) {
        QString srcFilePath = sourceDir + "/" + fileName;
        QString dstFilePath = destDir + QDir::separator() + fileName;

        // If file exists, try to remove it first
        if (QFile::exists(dstFilePath)) {
            if (!QFile::remove(dstFilePath)) {
                errorMessage = "Could not overwrite existing file: " + dstFilePath;
                return false;
            }
        }

        if (!QFile::copy(srcFilePath, dstFilePath)) {
            errorMessage = "Could not copy file to: " + dstFilePath;
            return false;
        }

        // Make sure the copied file is writable (resources are often read-only)
        QFile::setPermissions(dstFilePath, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);
    }

    return true;
}

bool createDesktopShortcut(const QString &targetFile, const QString &shortcutName, const QString &workingDirectory, QString &errorMessage)
{
#ifdef Q_OS_WIN
    HRESULT hres;
    IShellLink* psl;

    // Initialize COM library
    CoInitialize(NULL);

    // Get a pointer to the IShellLink interface.
    hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl);
    if (SUCCEEDED(hres)) {
        IPersistFile* ppf;

        // Set the path to the shortcut target
        psl->SetPath(QDir::toNativeSeparators(targetFile).toStdWString().c_str());
        // Set the working directory
        psl->SetWorkingDirectory(QDir::toNativeSeparators(workingDirectory).toStdWString().c_str());

        // Query IShellLink for the IPersistFile interface, used for saving the shortcut in persistent storage.
        hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);

        if (SUCCEEDED(hres)) {
            // Determine the desktop path
            QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
            QString shortcutPath = desktopPath + QDir::separator() + shortcutName + ".lnk";
            shortcutPath = QDir::toNativeSeparators(shortcutPath);

            // Save the link
            hres = ppf->Save(shortcutPath.toStdWString().c_str(), TRUE);
            if (FAILED(hres)) {
                errorMessage = "Failed to save the shortcut to desktop.";
            }

            ppf->Release();
        } else {
            errorMessage = "Failed to query IPersistFile interface.";
        }
        psl->Release();
    } else {
        errorMessage = "Failed to create IShellLink instance.";
    }

    CoUninitialize();
    
    return SUCCEEDED(hres);
#else
    // Fallback for non-Windows if needed, though instruction specified Windows
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
