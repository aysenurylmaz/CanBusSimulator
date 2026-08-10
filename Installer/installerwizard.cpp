#include "installerwizard.h"
#include "installerutils.h"

#include <QtWidgets>

// 1. ADIM: HOŞGELDİNİZ SAYFASI
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

// 2. ADIM: KURULUM DİZİNİ SEÇME SAYFASI
class DirectoryPage : public QWizardPage
{
    Q_OBJECT
public:
    DirectoryPage(QWidget *parent = nullptr) : QWizardPage(parent) {
        setTitle(tr("Installation Directory"));
        setSubTitle(tr("Please select where you want to install the application."));

        directoryLineEdit = new QLineEdit;

        //registerField fonksiyonu sihirbazın hafızasıdır.
        registerField("installationDirectory*", directoryLineEdit);

        // "Gözat..." butonu oluşturuluyor ve tıklandığında aşağıdaki browse() fonksiyonuna bağlanıyor.
        QPushButton *browseButton = new QPushButton(tr("Browse..."));
        connect(browseButton, &QPushButton::clicked, this, &DirectoryPage::browse);

        QHBoxLayout *layout = new QHBoxLayout;
        layout->addWidget(directoryLineEdit);
        layout->addWidget(browseButton);
        setLayout(layout);
    }

    void initializePage() override {
       // İşletim sistemine göre standart "AppData" veya "Program Files" klasörünü otomatik bulur.
        QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/CanBusSimulatorApp";
        // Bulunan klasör yolunu Windows formatına (ters slash '\' kullanacak şekilde) çevirip ekrana yazar.
        directoryLineEdit->setText(QDir::toNativeSeparators(defaultPath));
    }

private slots:
// Kullanıcı "Gözat" butonuna bastığında çalışan fonksiyon.
    void browse() {
        QString dir = QFileDialog::getExistingDirectory(this, tr("Select Installation Directory"), directoryLineEdit->text());
        if (!dir.isEmpty()) {
            directoryLineEdit->setText(QDir::toNativeSeparators(dir));
        }
    }

private:
    QLineEdit *directoryLineEdit;
};
// 3. ADIM: SEÇENEKLER SAYFASI
// Masaüstü kısayolu oluşturulsun mu gibi ekstra seçeneklerin sunulduğu sayfa.
class OptionsPage : public QWizardPage
{
public:
    OptionsPage(QWidget *parent = nullptr) : QWizardPage(parent) {
        setTitle(tr("Installation Options"));
        setSubTitle(tr("Select additional tasks to be performed."));

        shortcutCheckBox = new QCheckBox(tr("Create Desktop Shortcut"));
        shortcutCheckBox->setChecked(true); // Varsayılan olarak işaretli gelir.
        registerField("createShortcut", shortcutCheckBox);

        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget(shortcutCheckBox);
        setLayout(layout);
    }
private:
    QCheckBox *shortcutCheckBox;
};
// 4. ADIM: KURULUM (İLERLEME) SAYFASI
// Asıl işlemlerin, dosya kopyalamanın ve Registry kayıtlarının yapıldığı kritik sayfadır.
class ProgressPage : public QWizardPage
{
    Q_OBJECT
public:
    ProgressPage(QWidget *parent = nullptr) : QWizardPage(parent) {
        setTitle(tr("Installing"));
        setSubTitle(tr("Please wait while the application installs."));

        progressBar = new QProgressBar;
        progressBar->setRange(0, 100);// %0'dan %100'e kadar dolacak bir çubuk.

        statusLabel = new QLabel;

        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget(statusLabel);
        layout->addWidget(progressBar);
        setLayout(layout);
    }

    void initializePage() override {
        progressBar->setValue(0);
        statusLabel->setText(tr("Starting installation..."));
        
       // ÇOK ÖNEMLİ: İşlemi doğrudan başlatmıyoruz, 100 milisaniye gecikmeyle (QTimer) başlatıyoruz.
        // Eğer doğrudan başlatırsak arayüz donar (UI Freeze) ve kullanıcı sayfayı göremez.
        // Bu sayede arayüz ekrana çizilir, ardından arka planda kopyalama işlemi başlar.
        QTimer::singleShot(100, this, &ProgressPage::performInstallation);
    }
        // Sihirbazın 'İleri' butonuna basılabilmesi için bu fonksiyonun 'true' dönmesi gerekir.
    // Kurulum bitene kadar 'false' döneriz ki kullanıcı işlem bitmeden sayfayı geçemesin.
    bool isComplete() const override {
        return m_isComplete;
    }

private slots:
    void performInstallation() {
        // Önceki sayfalarda 'registerField' ile kaydettiğimiz verileri geri çekiyoruz.
        QString destDir = field("installationDirectory").toString();// Kurulum yapılacak klasör
        bool createShortcut = field("createShortcut").toBool();// Kısayol istendi mi?

        statusLabel->setText(tr("Copying files..."));
        progressBar->setValue(25);
        QCoreApplication::processEvents();

        QString errorMsg;
        // Qt'nin .qrc (Resource) dosyasının içine gömdüğümüz dosyaları (:/payload), hedef klasöre çıkarıyoruz.
        if (!InstallerUtils::copyResources(":/payload", destDir, errorMsg)) {
            QMessageBox::critical(this, tr("Installation Error"), tr("Failed to copy files:\n%1").arg(errorMsg));
            statusLabel->setText(tr("Installation failed."));
            return;
        }

        progressBar->setValue(75);
        QCoreApplication::processEvents();
        // Kısayol oluşturma işlemi
        if (createShortcut) {
            statusLabel->setText(tr("Creating shortcut..."));
            QString targetExe = destDir + QDir::separator() + "CanBusSimulator.exe";
            if (!InstallerUtils::createDesktopShortcut(targetExe, "CanBusSimulator", destDir, errorMsg)) {
                QMessageBox::warning(this, tr("Shortcut Error"), tr("Failed to create desktop shortcut:\n%1").arg(errorMsg));
            }
        }

        statusLabel->setText(tr("Registering application..."));
        QCoreApplication::processEvents();

        // UNINSTALLER (Kaldırıcı) OLUŞTURMA MANTIĞI:
        // Aslında ayrı bir uninstaller programımız yok. Şuan çalışan installer'ın (kendisinin) 
        // bir kopyasını hedef klasöre "uninstall.exe" adıyla kopyalıyoruz.
        QString uninstallerPath = destDir + QDir::separator() + "uninstall.exe";
        QFile::copy(QCoreApplication::applicationFilePath(), uninstallerPath);

        // REGISTRY (KAYIT DEFTERİ) İŞLEMLERİ:
        // Programın Windows Denetim Masasında (Program Ekle/Kaldır) görünmesi için gereken anahtarları yazıyoruz.
        // QSettings'in "NativeFormat" parametresi, işlemin doğrudan Windows Registry'ye yapılmasını sağlar.
        QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\CanBusSimulator", QSettings::NativeFormat);
        settings.setValue("DisplayName", "CanBusSimulator");
        
        // Kullanıcı 'Kaldır' dediğinde çalışacak komut (Kendimizi --uninstall parametresiyle çağırıyoruz)
        settings.setValue("UninstallString", "\"" + QDir::toNativeSeparators(uninstallerPath) + "\" --uninstall");
        settings.setValue("InstallLocation", QDir::toNativeSeparators(destDir));
        settings.setValue("Publisher", "Aysenur");
        QString targetExe = destDir + QDir::separator() + "CanBusSimulator.exe";
        settings.setValue("DisplayIcon", QDir::toNativeSeparators(targetExe));
        settings.sync();// Yazılan değerleri Registry'ye kalıcı olarak kaydet.

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
// 5. ADIM: SONUÇ SAYFASI
// Kurulumun başarıyla bittiğini bildiren son ekran.
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
// ANA SİHİRBAZ SINIFI
// Tüm sayfaları bir araya getirip sıraya dizdiğimiz merkez burasıdır.
InstallerWizard::InstallerWizard(QWidget *parent)
    : QWizard(parent)
{
    introPage = new IntroPage;
    directoryPage = new DirectoryPage;
    optionsPage = new OptionsPage;
    progressPage = new ProgressPage;
    conclusionPage = new ConclusionPage;

    // Sayfaları sihirbaza tanımlıyoruz.
    setPage(Page_Intro, introPage);
    setPage(Page_Directory, directoryPage);
    setPage(Page_Options, optionsPage);
    setPage(Page_Progress, progressPage);
    setPage(Page_Conclusion, conclusionPage);

    setStartId(Page_Intro);

    setWindowTitle(tr("CanBusSimulator Installer"));
    resize(500, 400);

    // GÖRÜNÜM (UI) DÜZELTMESİ:
    // Eğer kullanıcının bilgisayarı 'Karanlık Mod'da (Dark Mode) ise, yazılar beyaz olabilir.
    // Sihirbazın arka planı zaten beyaz olduğu için yazılar okunmaz hale gelir.
    // Bunu engellemek için CheckBox, Label ve RadioButton yazılarının rengini zorla Siyah yapıyoruz.
    setStyleSheet("QWizard QCheckBox, QWizard QLabel, QWizard QRadioButton { color: black; }");
}

#include "installerwizard.moc"
