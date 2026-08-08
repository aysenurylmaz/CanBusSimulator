#include "installerwizard.h"
#include "installerutils.h"

#include <QtWidgets>

class IntroPage : public QWizardPage
{
public:
    IntroPage(QWidget *parent = nullptr) : QWizardPage(parent) {
        setTitle(tr("Welcome to the CanBusSimulator Installer"));
        QLabel *label = new QLabel(tr("This wizard will guide you through the installation of CanBusSimulator."));
        label->setWordWrap(true);
        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget(label);
        setLayout(layout);
    }
};

class DirectoryPage : public QWizardPage
{
    Q_OBJECT
public:
    DirectoryPage(QWidget *parent = nullptr) : QWizardPage(parent) {
        setTitle(tr("Installation Directory"));
        setSubTitle(tr("Please select where you want to install the application."));

        directoryLineEdit = new QLineEdit;
        registerField("installationDirectory*", directoryLineEdit);

        QPushButton *browseButton = new QPushButton(tr("Browse..."));
        connect(browseButton, &QPushButton::clicked, this, &DirectoryPage::browse);

        QHBoxLayout *layout = new QHBoxLayout;
        layout->addWidget(directoryLineEdit);
        layout->addWidget(browseButton);
        setLayout(layout);
    }

    void initializePage() override {
        // Set a fixed default path based on the user's local AppData
        QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/CanBusSimulatorApp";
        directoryLineEdit->setText(QDir::toNativeSeparators(defaultPath));
    }

private slots:
    void browse() {
        QString dir = QFileDialog::getExistingDirectory(this, tr("Select Installation Directory"), directoryLineEdit->text());
        if (!dir.isEmpty()) {
            directoryLineEdit->setText(QDir::toNativeSeparators(dir));
        }
    }

private:
    QLineEdit *directoryLineEdit;
};

class OptionsPage : public QWizardPage
{
public:
    OptionsPage(QWidget *parent = nullptr) : QWizardPage(parent) {
        setTitle(tr("Installation Options"));
        setSubTitle(tr("Select additional tasks to be performed."));

        shortcutCheckBox = new QCheckBox(tr("Create Desktop Shortcut"));
        shortcutCheckBox->setChecked(true);
        registerField("createShortcut", shortcutCheckBox);

        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget(shortcutCheckBox);
        setLayout(layout);
    }
private:
    QCheckBox *shortcutCheckBox;
};

class ProgressPage : public QWizardPage
{
    Q_OBJECT
public:
    ProgressPage(QWidget *parent = nullptr) : QWizardPage(parent) {
        setTitle(tr("Installing"));
        setSubTitle(tr("Please wait while the application installs."));

        progressBar = new QProgressBar;
        progressBar->setRange(0, 100);

        statusLabel = new QLabel;

        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget(statusLabel);
        layout->addWidget(progressBar);
        setLayout(layout);
    }

    void initializePage() override {
        progressBar->setValue(0);
        statusLabel->setText(tr("Starting installation..."));
        
        // Start installation process slightly delayed to allow UI to update
        QTimer::singleShot(100, this, &ProgressPage::performInstallation);
    }

    bool isComplete() const override {
        return m_isComplete;
    }

private slots:
    void performInstallation() {
        QString destDir = field("installationDirectory").toString();
        bool createShortcut = field("createShortcut").toBool();

        statusLabel->setText(tr("Copying files..."));
        progressBar->setValue(25);
        QCoreApplication::processEvents();

        QString errorMsg;
        // Copy files from QRC (:/payload) to destination
        if (!InstallerUtils::copyResources(":/payload", destDir, errorMsg)) {
            QMessageBox::critical(this, tr("Installation Error"), tr("Failed to copy files:\n%1").arg(errorMsg));
            statusLabel->setText(tr("Installation failed."));
            return;
        }

        progressBar->setValue(75);
        QCoreApplication::processEvents();

        if (createShortcut) {
            statusLabel->setText(tr("Creating shortcut..."));
            QString targetExe = destDir + QDir::separator() + "CanBusSimulator.exe";
            if (!InstallerUtils::createDesktopShortcut(targetExe, "CanBusSimulator", destDir, errorMsg)) {
                QMessageBox::warning(this, tr("Shortcut Error"), tr("Failed to create desktop shortcut:\n%1").arg(errorMsg));
            }
        }

        statusLabel->setText(tr("Registering application..."));
        QCoreApplication::processEvents();

        QString uninstallerPath = destDir + QDir::separator() + "uninstall.exe";
        QFile::copy(QCoreApplication::applicationFilePath(), uninstallerPath);

        QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\CanBusSimulator", QSettings::NativeFormat);
        settings.setValue("DisplayName", "CanBusSimulator");
        settings.setValue("UninstallString", "\"" + QDir::toNativeSeparators(uninstallerPath) + "\" --uninstall");
        settings.setValue("InstallLocation", QDir::toNativeSeparators(destDir));
        settings.setValue("Publisher", "Aysenur");
        QString targetExe = destDir + QDir::separator() + "CanBusSimulator.exe";
        settings.setValue("DisplayIcon", QDir::toNativeSeparators(targetExe));
        settings.sync();

        progressBar->setValue(100);
        statusLabel->setText(tr("Installation complete."));
        m_isComplete = true;
        emit completeChanged();
    }

private:
    QProgressBar *progressBar;
    QLabel *statusLabel;
    bool m_isComplete = false;
};

class ConclusionPage : public QWizardPage
{
public:
    ConclusionPage(QWidget *parent = nullptr) : QWizardPage(parent) {
        setTitle(tr("Completing the Installer"));
        QLabel *label = new QLabel(tr("The application has been successfully installed on your computer.\n\nClick Finish to exit."));
        label->setWordWrap(true);
        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget(label);
        setLayout(layout);
    }
};

InstallerWizard::InstallerWizard(QWidget *parent)
    : QWizard(parent)
{
    introPage = new IntroPage;
    directoryPage = new DirectoryPage;
    optionsPage = new OptionsPage;
    progressPage = new ProgressPage;
    conclusionPage = new ConclusionPage;

    setPage(Page_Intro, introPage);
    setPage(Page_Directory, directoryPage);
    setPage(Page_Options, optionsPage);
    setPage(Page_Progress, progressPage);
    setPage(Page_Conclusion, conclusionPage);

    setStartId(Page_Intro);

    setWindowTitle(tr("CanBusSimulator Installer"));
    resize(500, 400);

    // Force text colors to black to avoid white-on-white text issues 
    // caused by dark mode + Fusion style on QWizard's white background.
    setStyleSheet("QWizard QCheckBox, QWizard QLabel, QWizard QRadioButton { color: black; }");
}

#include "installerwizard.moc"
