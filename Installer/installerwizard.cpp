#include "installerwizard.h"
#include "installerutils.h"

#include <QtWidgets>

// 1. ADM: HOSGELDNZ SAYFAS
class ntroPage : public QWizardPage
{
public:
    ntroPage(QWidget *parent = nullptr) : QWizardPage(parent) {
        setTitle(tr("Welcome to the CanBusSimulator nstaller"));
        QLabel *label = new QLabel(tr("This wizard will guide you through the installation of CanBusSimulator."));
        label->setWordWrap(true);
        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget(label);
        setLayout(layout);
    }
};

// 2. ADM: KURULUM DZN SECME SAYFAS
class DirectoryPage : public QWizardPage
{
    Q_OBJECT
public:
    DirectoryPage(QWidget *parent = nullptr) : QWizardPage(parent) {
        setTitle(tr("nstallation Directory"));
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
       // sletim sistemine gore standart "AppData" veya "Program Files" klasorunu otomatik bulur.
        QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/CanBusSimulatorApp";
        // Bulunan klasor yolunu Windows formatina (ters slash '\' kullanacak sekilde) cevirip ekrana yazar.
        directoryLineEdit->setText(QDir::toNativeSeparators(defaultPath));
    }

private slots:
// Kullanici "Gozat" butonuna bastiginda calisan fonksiyon.
    void browse() {
        QString dir = QFileDialog::getExistingDirectory(this, tr("Select nstallation Directory"), directoryLineEdit->text());
        if (!dir.isEmpty()) {
            directoryLineEdit->setText(QDir::toNativeSeparators(dir));
        }
    }

private:
    QLineEdit *directoryLineEdit;
};
// 3. ADM: SECENEKLER SAYFAS
// Masaustu kisayolu olusturulsun mu gibi ekstra seceneklerin sunuldugu sayfa.
class OptionsPage : public QWizardPage
{
public:
    OptionsPage(QWidget *parent = nullptr) : QWizardPage(parent) {
        setTitle(tr("nstallation Options"));
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
// 4. ADM: KURULUM (LERLEME) SAYFAS
// Asil islemlerin, dosya kopyalamanin ve Registry kayitlarinin yapildigi kritik sayfadir.
class ProgressPage : public QWizardPage
{
    Q_OBJECT
public:
    ProgressPage(QWidget *parent = nullptr) : QWizardPage(parent) {
        setTitle(tr("nstalling"));
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
        
       // COK ONEML: slemi dogrudan baslatmiyoruz, 100 milisaniye gecikmeyle (QTimer) baslatiyoruz.
        // Eger dogrudan baslatirsak arayuz donar (U Freeze) ve kullanici sayfayi goremez.
        // Bu sayede arayuz ekrana cizilir, ardindan arka planda kopyalama islemi baslar.
        QTimer::singleShot(100, this, &ProgressPage::performnstallation);
    }
        // Sihirbazin 'leri' butonuna basilabilmesi icin bu fonksiyonun 'true' donmesi gerekir.
    // Kurulum bitene kadar 'false' doneriz ki kullanici islem bitmeden sayfayi gecemesin.
    bool isComplete() const override {
        return m_isComplete;
    }

private slots:
    void performnstallation() {
        // Onceki sayfalarda 'registerField' ile kaydettigimiz verileri geri cekiyoruz.
        QString destDir = field("installationDirectory").toString();// Kurulum yapilacak klasor
        bool createShortcut = field("createShortcut").toBool();// Kisayol istendi mi?

        statusLabel->setText(tr("Copying files..."));
        progressBar->setValue(25);
        QCoreApplication::processEvents();

        QString errorMsg;
        // Qt'nin .qrc (Resource) dosyasinin icine gomdugumuz dosyalari (:/payload), hedef klasore cikariyoruz.
        if (!nstallerUtils::copyResources(":/payload", destDir, errorMsg)) {
            QMessageBox::critical(this, tr("nstallation Error"), tr("Failed to copy files:\n%1").arg(errorMsg));
            statusLabel->setText(tr("nstallation failed."));
            return;
        }

        progressBar->setValue(75);
        QCoreApplication::processEvents();
        // Kisayol olusturma islemi
        if (createShortcut) {
            statusLabel->setText(tr("Creating shortcut..."));
            QString targetExe = destDir + QDir::separator() + "CanBusSimulator.exe";
            if (!nstallerUtils::createDesktopShortcut(targetExe, "CanBusSimulator", destDir, errorMsg)) {
                QMessageBox::warning(this, tr("Shortcut Error"), tr("Failed to create desktop shortcut:\n%1").arg(errorMsg));
            }
        }

        statusLabel->setText(tr("Registering application..."));
        QCoreApplication::processEvents();

        // UNNSTALLER (Kaldirici) OLUSTURMA MANTG:
        // Aslinda ayri bir uninstaller programimiz yok. Suan calisan installer'in (kendisinin) 
        // bir kopyasini hedef klasore "uninstall.exe" adiyla kopyaliyoruz.
        QString uninstallerPath = destDir + QDir::separator() + "uninstall.exe";
        QFile::copy(QCoreApplication::applicationFilePath(), uninstallerPath);

        // REGSTRY (KAYT DEFTER) SLEMLER:
        // Programin Windows Denetim Masasinda (Program Ekle/Kaldir) gorunmesi icin gereken anahtarlari yaziyoruz.
        // QSettings'in "NativeFormat" parametresi, islemin dogrudan Windows Registry'ye yapilmasini saglar.
        QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\CanBusSimulator", QSettings::NativeFormat);
        settings.setValue("DisplayName", "CanBusSimulator");
        
        // Kullanici 'Kaldir' dediginde calisacak komut (Kendimizi --uninstall parametresiyle cagiriyoruz)
        settings.setValue("UninstallString", "\"" + QDir::toNativeSeparators(uninstallerPath) + "\" --uninstall");
        settings.setValue("nstallLocation", QDir::toNativeSeparators(destDir));
        settings.setValue("Publisher", "Aysenur");
        QString targetExe = destDir + QDir::separator() + "CanBusSimulator.exe";
        settings.setValue("Displaycon", QDir::toNativeSeparators(targetExe));
        settings.sync();// Yazilan degerleri Registry'ye kalici olarak kaydet.

        progressBar->setValue(100);
        statusLabel->setText(tr("nstallation complete."));
        m_isComplete = true;
        emit completeChanged();
    }

private:
    QProgressBar *progressBar;
    QLabel *statusLabel;
    bool m_isComplete = false;
};
// 5. ADM: SONUC SAYFAS
// Kurulumun basariyla bittigini bildiren son ekran.
class ConclusionPage : public QWizardPage
{
public:
    ConclusionPage(QWidget *parent = nullptr) : QWizardPage(parent) {
        setTitle(tr("Completing the nstaller"));
        QLabel *label = new QLabel(tr("The application has been successfully installed on your computer.\n\nClick Finish to exit."));
        label->setWordWrap(true);
        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget(label);
        setLayout(layout);
    }
};
// ANA SHRBAZ SNF
// Tum sayfalari bir araya getirip siraya dizdigimiz merkez burasidir.
nstallerWizard::nstallerWizard(QWidget *parent)
    : QWizard(parent)
{
    introPage = new ntroPage;
    directoryPage = new DirectoryPage;
    optionsPage = new OptionsPage;
    progressPage = new ProgressPage;
    conclusionPage = new ConclusionPage;

    // Sayfalari sihirbaza tanimliyoruz.
    setPage(Page_ntro, introPage);
    setPage(Page_Directory, directoryPage);
    setPage(Page_Options, optionsPage);
    setPage(Page_Progress, progressPage);
    setPage(Page_Conclusion, conclusionPage);

    setStartd(Page_ntro);

    setWindowTitle(tr("CanBusSimulator nstaller"));
    resize(500, 400);

    // GORUNUM (U) DUZELTMES:
    // Eger kullanicinin bilgisayari 'Karanlik Mod'da (Dark Mode) ise, yazilar beyaz olabilir.
    // Sihirbazin arka plani zaten beyaz oldugu icin yazilar okunmaz hale gelir.
    // Bunu engellemek icin CheckBox, Label ve RadioButton yazilarinin rengini zorla Siyah yapiyoruz.
    setStyleSheet("QWizard QCheckBox, QWizard QLabel, QWizard QRadioButton { color: black; }");
}

#include "installerwizard.moc"
