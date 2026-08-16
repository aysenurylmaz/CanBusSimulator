#ifndef NSTALLERUTLS_H
#define NSTALLERUTLS_H

#include <QString>

namespace nstallerUtils {
    // Copies contents of a QRC directory to a local file system directory
    bool copyResources(const QString &sourceDir, const QString &destDir, QString &errorMessage);

    // Creates a desktop shortcut for the target file
    bool createDesktopShortcut(const QString &targetFile, const QString &shortcutName, const QString &workingDirectory, QString &errorMessage);
}

#endif // NSTALLERUTLS_H
