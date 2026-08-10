// mainwindow.cpp
// Kullanıcı arayüzünün (UI) asıl kodlarının bulunduğu yerdir.
// Butonlara basıldığında ne olacağı (Sinyal ve Yuva işlemleri) burada kodlanmıştır.
#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QGroupBox>
#include <QString>
#include <QTime>
#include <QFileDialog>
#include <QMessageBox>
// --- KURUUCU FONKSİYON (Constructor) ---
// Sınıf ilk oluşturulduğunda çalışır. Arayüzü, parser'ı (ayrıştırıcı) ve başlangıç değerlerini ayarlar.
MainWindow::MainWindow(QWidget *parent) : QWidget(parent), isHandbrakeOn(true), isHeadlightsOn(false) {
    // DBC dosyalarını okuyacak nesneyi oluşturuyoruz
    dbcParser = new DbcParser();
    setupUi();// Arayüz elemanlarını (butonlar, sliderlar vs.) oluşturan fonksiyonu çağırıyoruz
    
    // Güncelleme kontrolü yapacak sınıfı başlatıyoruz ve buton ile bağlıyoruz
    updater = new Updater(this);
    connect(updateBtn, &QPushButton::clicked, updater, &Updater::checkForUpdates);
    
   // Uygulama açılır açılmaz ekranda ilk CAN Bus mesajını (frame) göstermesi için tetikliyoruz
    generateCanFrame();
}
// --- YIKICI FONKSİYON (Destructor) ---
// Program kapatıldığında veya pencere yok edildiğinde çalışır.
MainWindow::~MainWindow() {
    delete dbcParser;
}

// --- ARAYÜZ KURULUMU ---
// Ekrandaki tüm görsel öğelerin (buton, yazı, grafik) yaratıldığı ve yerleştirildiği yer.
void MainWindow::setupUi() {
    setWindowTitle("CAN Bus Simulator - v3.4");
    resize(800, 600);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // --- Araç Gösterge Paneli (Dashboard) Grubu ---
    QGroupBox *dashboardGroup = new QGroupBox("Vehicle Dashboard");
    QGridLayout *dashboardLayout = new QGridLayout(dashboardGroup);

    // 1. El Freni (Handbrake)
    QLabel *handbrakeLabel = new QLabel("Handbrake:");
    handbrakeBtn = new QPushButton("ENGAGED (ON)");
    handbrakeBtn->setCheckable(true); // Butonun basılı kalabilme özelliği (Toggle) açılıyor
    handbrakeBtn->setChecked(true);
    handbrakeBtn->setStyleSheet("background-color: red; color: white; font-weight: bold;");
    dashboardLayout->addWidget(handbrakeLabel, 0, 0);
    dashboardLayout->addWidget(handbrakeBtn, 0, 1);

    // 2. Hız (Speed)
    QLabel *speedLabel = new QLabel("Vehicle Speed (km/h):");
    speedSlider = new QSlider(Qt::Horizontal);
    speedSlider->setRange(0, 250);// Hız aralığı 0-250
    speedSpinBox = new QSpinBox();
    speedSpinBox->setRange(0, 250);
    
    // Slider ve Spinbox'ı birbirine bağlıyoruz (biri değişince diğeri de aynı değeri alsın diye)
    connect(speedSlider, &QSlider::valueChanged, speedSpinBox, &QSpinBox::setValue);
    connect(speedSpinBox, &QSpinBox::valueChanged, speedSlider, &QSlider::setValue);
    
    dashboardLayout->addWidget(speedLabel, 1, 0);
    dashboardLayout->addWidget(speedSlider, 1, 1);
    dashboardLayout->addWidget(speedSpinBox, 1, 2);

    // 3. Batarya (Battery)
    QLabel *batteryLabel = new QLabel("Battery Charge (%):");
    batterySlider = new QSlider(Qt::Horizontal);
    batterySlider->setRange(0, 100);
    batterySlider->setValue(100);// Başlangıç şarjı %100
    batteryProgressBar = new QProgressBar();// Şarj durumunu gösteren dolum çubuğu
    batteryProgressBar->setRange(0, 100);
    batteryProgressBar->setValue(100);
    
    // Slider değiştikçe ProgressBar da güncellensin
    connect(batterySlider, &QSlider::valueChanged, batteryProgressBar, &QProgressBar::setValue);

    dashboardLayout->addWidget(batteryLabel, 2, 0);
    dashboardLayout->addWidget(batterySlider, 2, 1);
    dashboardLayout->addWidget(batteryProgressBar, 2, 2);

    // 4. Farlar (Headlights)
    QLabel *headlightsLabel = new QLabel("Headlights:");
    headlightsBtn = new QPushButton("OFF");
    headlightsBtn->setCheckable(true);
    headlightsBtn->setChecked(false);
    headlightsBtn->setStyleSheet("background-color: gray; color: white; font-weight: bold;");
    dashboardLayout->addWidget(headlightsLabel, 3, 0);
    dashboardLayout->addWidget(headlightsBtn, 3, 1);
    
    // Paneli ana düzene ekliyoruz
    mainLayout->addWidget(dashboardGroup);

    // --- CAN İzleyici (Monitor) Grubu ---
    // Üretilen CAN mesajlarının log (kayıt) olarak yazdırılacağı siyah ekran
    QGroupBox *monitorGroup = new QGroupBox("CAN Bus Monitor (ID: 0x1F4)");
    QVBoxLayout *monitorLayout = new QVBoxLayout(monitorGroup);
    
    canMonitor = new QTextEdit();
    canMonitor->setReadOnly(true);// Kullanıcı buraya yazı yazamasın, sadece okusun
    canMonitor->setStyleSheet("background-color: #1e1e1e; color: #00ff00; font-family: monospace;");
    monitorLayout->addWidget(canMonitor);
    
    mainLayout->addWidget(monitorGroup);

    // --- Butonlar (Güncelleme ve DBC Yükleme) ---
    updateBtn = new QPushButton("Check for Updates");
    mainLayout->addWidget(updateBtn);

    // --- Load DBC Button ---
    loadDbcBtn = new QPushButton("Load DBC File");
    mainLayout->addWidget(loadDbcBtn);

    // --- SİNYAL VE YUVA (Signal & Slot) BAĞLANTILARI ---
    // Arayüzdeki eylemlerin hangi fonksiyonları çalıştıracağını belirliyoruz
    connect(handbrakeBtn, &QPushButton::clicked, this, &MainWindow::toggleHandbrake);
    connect(speedSlider, &QSlider::valueChanged, speedSpinBox, &QSpinBox::setValue);// Hız değiştiğinde CAN mesajını yeniden üret
    connect(speedSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), speedSlider, &QSlider::setValue);// Batarya değiştiğinde CAN mesajını yeniden üret
    connect(speedSlider, &QSlider::valueChanged, this, &MainWindow::generateCanFrame);
    
    connect(batterySlider, &QSlider::valueChanged, this, &MainWindow::generateCanFrame);
    
    connect(headlightsBtn, &QPushButton::clicked, this, &MainWindow::toggleHeadlights);
    connect(loadDbcBtn, &QPushButton::clicked, this, &MainWindow::loadDbcFile);
}
// --- EL FRENİ AÇ/KAPA İŞLEMİ ---
void MainWindow::toggleHandbrake() {
    isHandbrakeOn = !isHandbrakeOn;
    if (isHandbrakeOn) {
        handbrakeBtn->setText("ENGAGED (ON)");
        handbrakeBtn->setStyleSheet("background-color: red; color: white; font-weight: bold;");
    } else {
        handbrakeBtn->setText("RELEASED (OFF)");
        handbrakeBtn->setStyleSheet("background-color: green; color: white; font-weight: bold;");
    }
    generateCanFrame();// Durum değiştiği için CAN mesajını güncelle
}
// --- FAR AÇ/KAPA İŞLEMİ ---
void MainWindow::toggleHeadlights() {
    isHeadlightsOn = !isHeadlightsOn;
    if (isHeadlightsOn) {
        headlightsBtn->setText("ON");
        headlightsBtn->setStyleSheet("background-color: yellow; color: black; font-weight: bold;");
    } else {
        headlightsBtn->setText("OFF");
        headlightsBtn->setStyleSheet("background-color: gray; color: white; font-weight: bold;");
    }
    generateCanFrame();// Durum değiştiği için CAN mesajını güncelle
}
// --- DBC DOSYASI YÜKLEME ---
void MainWindow::loadDbcFile() {
    // Kullanıcıya dosya seçme penceresi açar 
    QString fileName = QFileDialog::getOpenFileName(this, "Open DBC File", "", "DBC Files (*.dbc);;All Files (*)");
    if (!fileName.isEmpty()) {
        // Seçilen dosyayı DbcParser'a (ayrıştırıcıya) gönderir.
        if (dbcParser->parseFile(fileName)) {
            QMessageBox::information(this, "Success", "DBC file loaded successfully!");
            generateCanFrame(); // Yeni DBC formatına göre mesajları güncelle
        } else {
            QMessageBox::warning(this, "Error", "Failed to parse DBC file!");
        }
    }
}
// --- SİNYAL PAKETLEME (BİT DÜZEYİNDE İŞLEM) ---
void MainWindow::packSignal(QByteArray &frame, const DbcSignal &sig, uint64_t rawVal) {
    // Ham değeri alır, DBC'de belirtilen bit uzunluğuna ve başlangıç bitine göre
    // 8 Byte'lık (64 bit) CAN çerçevesinin içine kaydırarak sıkıştırır (Bitwise packing).
    
    // Gelen değeri sinyal uzunluğu kadar bir maskeyle sınırlandırıyoruz (Fazlalık bitleri çöpe atıyoruz)
    uint64_t mask = (1ULL << sig.length) - 1;
    rawVal = (rawVal & mask);

    // Little Endian (Intel Formatı): Byte'lar küçükten büyüğe doğru yerleştirilir
    if (sig.isLittleEndian) {
        int bitsPacked = 0;// Şu ana kadar paketlenen bit sayısı
        int currentByte = sig.startBit / 8;// İşleme başlanacak Byte numarası (0-7 arası)
        int bitOffset = sig.startBit % 8;// O Byte içindeki başlangıç biti
        
        // Tüm bitler paketlenene ve çerçeve boyutu aşılmayana kadar dön
        while (bitsPacked < sig.length && currentByte < frame.size()) {
            // Bu turda kaç bit yazacağız? (Kalan byte boşluğu ile kalan sinyal uzunluğunun minimumu)
            int bitsInThisByte = std::min(8 - bitOffset, sig.length - bitsPacked);
            
            // Maske oluşturup hedef bölgeyi sıfırlıyoruz
            uint8_t byteMask = ((1 << bitsInThisByte) - 1) << bitOffset;
            
            uint8_t valToPack = (rawVal >> bitsPacked) & ((1 << bitsInThisByte) - 1);
            
            // Sıfırlanan bölgeye yeni değeri bit düzeyinde (OR ile) ekliyoruz
            frame[currentByte] = (frame[currentByte] & ~byteMask) | (valToPack << bitOffset);
            
            bitsPacked += bitsInThisByte;
            bitOffset = 0; // İlk byte'tan sonra diğer byte'ların 0. bitinden başlanır
            currentByte++; // Bir sonraki byte'a geç
        }
    } else {
        // Big Endian (Motorola Formatı): Byte'lar büyükten küçüğe doğru (tersine) yerleştirilir
        int bitsPacked = 0;
        int currentByte = sig.startBit / 8;
        int bitOffset = sig.startBit % 8; 
        
        while (bitsPacked < sig.length && currentByte >= 0 && currentByte < frame.size()) {
            int bitsInThisByte = std::min(bitOffset + 1, sig.length - bitsPacked);
            int shiftAmount = (bitOffset + 1) - bitsInThisByte;
            uint8_t byteMask = ((1 << bitsInThisByte) - 1) << shiftAmount;
            
            uint8_t valToPack = (rawVal >> (sig.length - bitsPacked - bitsInThisByte)) & ((1 << bitsInThisByte) - 1);
            frame[currentByte] = (frame[currentByte] & ~byteMask) | (valToPack << shiftAmount);
            
            bitsPacked += bitsInThisByte;
            bitOffset = 7;// Big Endian'da bir sonraki byte'a geçildiğinde en üst bitten (7. bit) aşağı inilir
            currentByte--;// Önceki byte'a doğru geri gidilir
        }
    }
}
// --- CAN MESAJI ÜRETME VE EKRANA YAZDIRMA ---
void MainWindow::generateCanFrame() {
    // 3. Arayüzdeki her harekette (örn. Hız değişimi) bu fonksiyon tetiklenir ve sonuçları siyah ekrana yazar.
    canMonitor->clear(); // Ekranı temizle (Sadece son durumu göstermek için)

    // REFACTOR: Tekrar eden Hex çevirimi ve log yazdırma kodlarını bir C++ Lambda fonksiyonuna aldık.
    auto logMessage = [&](uint32_t fullId, const QByteArray &data) {
        uint32_t displayId = fullId & 0x1FFFFFFF; // Ekranda göstermek için Extended flag maskeleniyor
        bool isExtended = (fullId & 0x80000000) != 0; // En üst bit 1 ise mesaj Extended ID'dir
        
        // Veriyi HEX (Onaltılık) string formatına çevir
        QString hexString;
        for (int i = 0; i < data.size(); ++i) {
            hexString += QString("%1 ").arg(static_cast<quint8>(data[i]), 2, 16, QChar('0')).toUpper();
        }
        
        // Log satırını oluştur: [Saat] TX -> ID: 0x... (EXT) DLC: ... DATA: ...
        QString timeStr = QTime::currentTime().toString("HH:mm:ss.zzz");
        QString logLine = QString("[%1] TX -> ID: 0x%2%3 DLC: %4 DATA: %5")
            .arg(timeStr)
            .arg(QString::number(displayId, 16).toUpper())
            .arg(isExtended ? " (EXT)" : "")
            .arg(data.size())
            .arg(hexString.trimmed());

        canMonitor->append(logLine);
    };

    // EĞER DBC DOSYASI YÜKLENMEMİŞSE: Sabit (Hardcoded) varsayılan kurallara göre frame oluştur.
    if (dbcParser->isEmpty()) {
        // varsayılan sabit kod çalışır
        QByteArray data(8, 0);// 8 Baytlık (0 ile dolu) bir CAN veri paketi oluştur
        data[0] = isHandbrakeOn ? 0x01 : 0x00;// 0. Byte: El freni (Açıksa 1, kapalıysa 0)
        data[1] = static_cast<unsigned char>(batterySlider->value());// 1. Byte: Batarya durumu (Doğrudan slider değeri)
        
        // 2. ve 3. Byte: Hız (Hız 255'ten büyük olabileceği için 16-bit yani 2 byte yer ayırıyoruz)
        uint16_t speed = static_cast<uint16_t>(speedSpinBox->value());
        data[2] = speed & 0xFF;
        data[3] = (speed >> 8) & 0xFF;
        data[4] = isHeadlightsOn ? 0x01 : 0x00; // 4. Byte: Farlar (Açıksa 1, kapalıysa 0)
        
        // YENİ YAPI: Hazırlanan veriyi direkt log fonksiyonuna gönder
        logMessage(0x1F4, data);
        return;
    }

    // EĞER DBC DOSYASI YÜKLÜYSE: Sinyalleri DBC'ye göre dinamik olarak paketle.
    bool speedFound = false, batteryFound = false, handbrakeFound = false, headlightsFound = false;
    
    // DBC içindeki sinyalleri isimlerine / anahtar kelimelerine göre aratıyoruz.
    DbcSignal speedSig = dbcParser->findSignalByKeywords({"Speed", "Spd", "Hiz", "Vel"}, speedFound);
    DbcSignal batterySig = dbcParser->findSignalByKeywords({"Battery", "Bat", "SOC", "Charge", "Pil", "Enerji"}, batteryFound);
    DbcSignal handbrakeSig = dbcParser->findSignalByKeywords({"Brake", "Hand", "Park", "Fren"}, handbrakeFound);
    DbcSignal headlightsSig = dbcParser->findSignalByKeywords({"Light", "Lamp", "Head", "Far", "Isik"}, headlightsFound);

    QMap<uint32_t, QByteArray> frames;// Aynı Message ID'ye sahip sinyalleri aynı çerçevenin (frame) içine koyabilmek için bir Map oluşturuyoruz.

    auto prepareFrame = [&](const DbcSignal &sig, double physicalValue) {
        // Eğer bu mesaj ID'si ilk kez ekleniyorsa, o ID'nin boyutunu (DLC) bul ve byte dizisini oluştur
        if (!frames.contains(sig.messageId)) {
            int dlc = dbcParser->getMessages().value(sig.messageId).dlc;
            if (dlc == 0) dlc = 8; // Bulunamazsa varsayılan olarak 8 byte (standart CAN)
            frames[sig.messageId] = QByteArray(dlc, 0);
        }
        // Fiziksel değeri Ham değere dönüştürme formülü: Ham Değer = (Fiziksel Değer - Offset) / Factor
        double f = sig.factor != 0.0 ? sig.factor : 1.0;// Sıfıra bölme hatasını engelle
        uint64_t rawValue = static_cast<uint64_t>((physicalValue - sig.offset) / f);
        
        // Hazırlanan ham değeri ilgili pakete yaz
        packSignal(frames[sig.messageId], sig, rawValue);
    };
    
    // Bulunan sinyallere göre arayüzdeki güncel değerleri gönderip frame'leri hazırlıyoruz
    if (speedFound) prepareFrame(speedSig, speedSlider->value());
    if (batteryFound) prepareFrame(batterySig, batterySlider->value());
    if (handbrakeFound) prepareFrame(handbrakeSig, isHandbrakeOn ? 1.0 : 0.0);
    if (headlightsFound) prepareFrame(headlightsSig, isHeadlightsOn ? 1.0 : 0.0);

    // Oluşturulan tüm CAN mesajlarını sırayla siyah ekrana yazdır (Logla)
    for (auto it = frames.begin(); it != frames.end(); ++it) {
        // YENİ YAPI: Tüm ID hesaplamaları ve Hex formatlaması lambdada yapılıyor
        logMessage(it.key(), it.value());
    }
}

