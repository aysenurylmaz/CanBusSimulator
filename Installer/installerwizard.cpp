#include "installerwizard.h"
#include "installerutils.h"

#include <QtWidgets>

// 1. ADIM: HOSGELDINIZ SAYFASI
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

// 2. ADIM: KURULUM DIZINI SECME SAYFASI
class DirectoryPage : public QWizardPage
{
    Q_OBJECT
public:
    DirectoryPage(QWidget *parent = nullptr) : QWizardPage(parent) {
        setTitle(tr("Installation Directory"));
        setSubTitle(tr("Please select where you want to install the application."));

        directoryLineEdit = new QLineEdit;

        //registerField fonksiyonu sihirbazin hafizasidir.
        registerField("installationDirectory*", directoryLineEdit);

        // "Gozat..." butonu olusturuluyor ve tiklandiginda asagidaki browse() fonksiyonuna baglaniyor.
        QPushButton *browseButton = new QPushButton(tr("Browse..."));
        connect(browseButton, &QPushButton::clicked, this, &DirectoryPage::browse);

        QHBoxLayout *layout = new QHBoxLayout;
        layout->addWidget(directoryLineEdit);
        layout->addWidget(browseButton);
        setLayout(layout);
    }

    void initializePage() override {
       // Isletim sistemine gore standart "AppData" veya "Program Files" klasorunu otomatik bulur.
        QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/CanBusSimulatorApp";
        // Bulunan klasor yolunu Windows formatina (ters slash '\' kullanacak sekilde) cevirip ekrana yazar.
        directoryLineEdit->setText(QDir::toNativeSeparators(defaultPath));
    }

private slots:
// Kullanici "Gozat" butonuna bastiginda calisan fonksiyon.
    void browse() {
        QString dir = QFileDialog::getExistingDirectory(this, tr("Select Installation Directory"), directoryLineEdit->text());
        if (!dir.isEmpty()) {
            directoryLineEdit->setText(QDir::toNativeSeparators(dir));
        }
    }

private:
    QLineEdit *directoryLineEdit;
};
// 3. ADIM: SECENEKLER SAYFASI
// Masaustu kisayolu olusturulsun mu gibi ekstra seceneklerin sunuldugu sayfa.
class OptionsPage : public QWizardPage
{
public:
    OptionsPage(QWidget *parent = nullptr) : QWizardPage(parent) {
        setTitle(tr("Installation Options"));
        setSubTitle(tr("Select additional tasks to be performed."));

        shortcutCheckBox = new QCheckBox(tr("Create Desktop Shortcut"));
        shortcutCheckBox->setChecked(true); // Varsayilan olarak isaretli gelir.
        registerField("createShortcut", shortcutCheckBox);

        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget(shortcutCheckBox);
        setLayout(layout);
    }
private:
    QCheckBox *shortcutCheckBox;
};
// 4. ADIM: KURULUM (ILERLEME) SAYFASI
// Asil islemlerin, dosya kopyalamanin ve Registry kayitlarinin yapildigi kritik sayfadir.
class ProgressPage : public QWizardPage
{
    Q_OBJECT
public:
    ProgressPage(QWidget *parent = nullptr) : QWizardPage(parent) {
        setTitle(tr("Installing"));
        setSubTitle(tr("Please wait while the application installs."));

        progressBar = new QProgressBar;
        progressBar->setRange(0, 100);// %0'dan %100'e kadar dolacak bir cubuk.

        statusLabel = new QLabel;

        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget(statusLabel);
        layout->addWidget(progressBar);
        setLayout(layout);
    }

    void initializePage() override {
        progressBar->setValue(0);
        statusLabel->setText(tr("Starting installation..."));
        
       // COK ONEMLI: Islemi dogrudan baslatmiyoruz, 100 milisaniye gecikmeyle (QTimer) baslatiyoruz.
        // Eger dogrudan baslatirsak arayuz donar (UI Freeze) ve kullanici sayfayi goremez.
        // Bu sayede arayuz ekrana cizilir, ardindan arka planda kopyalama islemi baslar.
        QTimer::singleShot(100, this, &ProgressPage::performInstallation);
    }
        // Sihirbazin 'Ileri' butonuna basilabilmesi icin bu fonksiyonun 'true' donmesi gerekir.
    // Kurulum bitene kadar 'false' doneriz ki kullanici islem bitmeden sayfayi gecemesin.
    bool isComplete() const override {
        return m_isComplete;
    }

private slots:
    void performInstallation() {
        // Onceki sayfalarda 'registerField' ile kaydettigimiz verileri geri cekiyoruz.
        QString destDir = field("installationDirectory").toString();// Kurulum yapilacak klasor
        bool createShortcut = field("createShortcut").toBool();// Kisayol istendi mi?

        statusLabel->setText(tr("Copying files..."));
        progressBar->setValue(25);
        QCoreApplication::processEvents();

        QString errorMsg;
        // Qt'nin .qrc (Resource) dosyasinin icine gomdugumuz dosyalari (:/payload), hedef klasore cikariyoruz.
        if (!InstallerUtils::copyResources(":/payload", destDir, errorMsg)) {
            QMessageBox::critical(this, tr("Installation Error"), tr("Failed to copy files:\n%1").arg(errorMsg));
            statusLabel->setText(tr("Installation failed."));
            return;
        }

        progressBar->setValue(75);
        QCoreApplication::processEvents();
        // Kisayol olusturma islemi
        if (createShortcut) {
            statusLabel->setText(tr("Creating shortcut..."));
            QString targetExe = destDir + QDir::separator() + "CanBusSimulator.exe";
            if (!InstallerUtils::createDesktopShortcut(targetExe, "CanBusSimulator", destDir, errorMsg)) {
                QMessageBox::warning(this, tr("Shortcut Error"), tr("Failed to create desktop shortcut:\n%1").arg(errorMsg));
            }
        }

        statusLabel->setText(tr("Registering application..."));
        QCoreApplication::processEvents();

        // UNINSTALLER (Kaldirici) OLUSTURMA MANTIGI:
        // Aslinda ayri bir uninstaller programimiz yok. Suan calisan installer'in (kendisinin) 
        // bir kopyasini hedef klasore "uninstall.exe" adiyla kopyaliyoruz.
        QString uninstallerPath = destDir + QDir::separator() + "uninstall.exe";
        QFile::copy(QCoreApplication::applicationFilePath(), uninstallerPath);

        // REGISTRY (KAYIT DEFTERI) ISLEMLERI:
        // Programin Windows Denetim Masasinda (Program Ekle/Kaldir) gorunmesi icin gereken anahtarlari yaziyoruz.
        // QSettings'in "NativeFormat" parametresi, islemin dogrudan Windows Registry'ye yapilmasini saglar.
        QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\CanBusSimulator", QSettings::NativeFormat);
        settings.setValue("DisplayName", "CanBusSimulator");
        
        // Kullanici 'Kaldir' dediginde calisacak komut (Kendimizi --uninstall parametresiyle cagiriyoruz)
        settings.setValue("UninstallString", "\"" + QDir::toNativeSeparators(uninstallerPath) + "\" --uninstall");
        settings.setValue("InstallLocation", QDir::toNativeSeparators(destDir));
        settings.setValue("Publisher", "Aysenur");
        QString targetExe = destDir + QDir::separator() + "CanBusSimulator.exe";
        settings.setValue("DisplayIcon", QDir::toNativeSeparators(targetExe));
        settings.sync();// Yazilan degerleri Registry'ye kalici olarak kaydet.

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
// 5. ADIM: SONUC SAYFASI
// Kurulumun basariyla bittigini bildiren son ekran.
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
// ANA SIHIRBAZ SINIFI
// Tum sayfalari bir araya getirip siraya dizdigimiz merkez burasidir.
InstallerWizard::InstallerWizard(QWidget *parent)
    : QWizard(parent)
{
    introPage = new IntroPage;
    directoryPage = new DirectoryPage;
    optionsPage = new OptionsPage;
    progressPage = new ProgressPage;
    conclusionPage = new ConclusionPage;

    // Sayfalari sihirbaza tanimliyoruz.
    setPage(Page_Intro, introPage);
    setPage(Page_Directory, directoryPage);
    setPage(Page_Options, optionsPage);
    setPage(Page_Progress, progressPage);
    setPage(Page_Conclusion, conclusionPage);

    setStartId(Page_Intro);

    setWindowTitle(tr("CanBusSimulator Installer"));
    resize(500, 400);

    // GORUNUM (UI) DUZELTMESI:
    // Eger kullanicinin bilgisayari 'Karanlik Mod'da (Dark Mode) ise, yazilar beyaz olabilir.
    // Sihirbazin arka plani zaten beyaz oldugu icin yazilar okunmaz hale gelir.
    // Bunu engellemek icin CheckBox, Label ve RadioButton yazilarinin rengini zorla Siyah yapiyoruz.
    setStyleSheet("QWizard QCheckBox, QWizard QLabel, QWizard QRadioButton { color: black; }");
}

#include "installerwizard.moc"
