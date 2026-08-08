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

MainWindow::MainWindow(QWidget *parent) : QWidget(parent), isHandbrakeOn(true), isHeadlightsOn(false) {
    dbcParser = new DbcParser();
    setupUi();
    
    updater = new Updater(this);
    connect(updateBtn, &QPushButton::clicked, updater, &Updater::checkForUpdates);
    
    // Initial CAN Frame
    generateCanFrame();
}

MainWindow::~MainWindow() {
    delete dbcParser;
}


void MainWindow::setupUi() {
    setWindowTitle("CanBusSimulator v3.2");
    resize(800, 600);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // --- Dashboard Group ---
    QGroupBox *dashboardGroup = new QGroupBox("Vehicle Dashboard");
    QGridLayout *dashboardLayout = new QGridLayout(dashboardGroup);

    // Handbrake
    QLabel *handbrakeLabel = new QLabel("Handbrake:");
    handbrakeBtn = new QPushButton("ENGAGED (ON)");
    handbrakeBtn->setCheckable(true);
    handbrakeBtn->setChecked(true);
    handbrakeBtn->setStyleSheet("background-color: red; color: white; font-weight: bold;");
    dashboardLayout->addWidget(handbrakeLabel, 0, 0);
    dashboardLayout->addWidget(handbrakeBtn, 0, 1);

    // Speed
    QLabel *speedLabel = new QLabel("Vehicle Speed (km/h):");
    speedSlider = new QSlider(Qt::Horizontal);
    speedSlider->setRange(0, 250);
    speedSpinBox = new QSpinBox();
    speedSpinBox->setRange(0, 250);
    
    // Synchronize slider and spinbox
    connect(speedSlider, &QSlider::valueChanged, speedSpinBox, &QSpinBox::setValue);
    connect(speedSpinBox, &QSpinBox::valueChanged, speedSlider, &QSlider::setValue);
    
    dashboardLayout->addWidget(speedLabel, 1, 0);
    dashboardLayout->addWidget(speedSlider, 1, 1);
    dashboardLayout->addWidget(speedSpinBox, 1, 2);

    // Battery
    QLabel *batteryLabel = new QLabel("Battery Charge (%):");
    batterySlider = new QSlider(Qt::Horizontal);
    batterySlider->setRange(0, 100);
    batterySlider->setValue(100);
    batteryProgressBar = new QProgressBar();
    batteryProgressBar->setRange(0, 100);
    batteryProgressBar->setValue(100);
    
    // Synchronize slider and progress bar
    connect(batterySlider, &QSlider::valueChanged, batteryProgressBar, &QProgressBar::setValue);

    dashboardLayout->addWidget(batteryLabel, 2, 0);
    dashboardLayout->addWidget(batterySlider, 2, 1);
    dashboardLayout->addWidget(batteryProgressBar, 2, 2);

    // Headlights
    QLabel *headlightsLabel = new QLabel("Headlights:");
    headlightsBtn = new QPushButton("OFF");
    headlightsBtn->setCheckable(true);
    headlightsBtn->setChecked(false);
    headlightsBtn->setStyleSheet("background-color: gray; color: white; font-weight: bold;");
    dashboardLayout->addWidget(headlightsLabel, 3, 0);
    dashboardLayout->addWidget(headlightsBtn, 3, 1);

    mainLayout->addWidget(dashboardGroup);

    // --- CAN Monitor Group ---
    QGroupBox *monitorGroup = new QGroupBox("CAN Bus Monitor (ID: 0x1F4)");
    QVBoxLayout *monitorLayout = new QVBoxLayout(monitorGroup);
    
    canMonitor = new QTextEdit();
    canMonitor->setReadOnly(true);
    canMonitor->setStyleSheet("background-color: #1e1e1e; color: #00ff00; font-family: monospace;");
    monitorLayout->addWidget(canMonitor);
    
    mainLayout->addWidget(monitorGroup);

    // --- Update Button ---
    updateBtn = new QPushButton("Check for Updates");
    mainLayout->addWidget(updateBtn);

    // --- Load DBC Button ---
    loadDbcBtn = new QPushButton("Load DBC File");
    mainLayout->addWidget(loadDbcBtn);

    // --- Connections ---
    connect(handbrakeBtn, &QPushButton::clicked, this, &MainWindow::toggleHandbrake);
    connect(speedSlider, &QSlider::valueChanged, speedSpinBox, &QSpinBox::setValue);
    connect(speedSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), speedSlider, &QSlider::setValue);
    connect(speedSlider, &QSlider::valueChanged, this, &MainWindow::generateCanFrame);
    
    connect(batterySlider, &QSlider::valueChanged, this, &MainWindow::generateCanFrame);
    
    connect(headlightsBtn, &QPushButton::clicked, this, &MainWindow::toggleHeadlights);
    connect(loadDbcBtn, &QPushButton::clicked, this, &MainWindow::loadDbcFile);
}

void MainWindow::toggleHandbrake() {
    isHandbrakeOn = !isHandbrakeOn;
    if (isHandbrakeOn) {
        handbrakeBtn->setText("ENGAGED (ON)");
        handbrakeBtn->setStyleSheet("background-color: red; color: white; font-weight: bold;");
    } else {
        handbrakeBtn->setText("RELEASED (OFF)");
        handbrakeBtn->setStyleSheet("background-color: green; color: white; font-weight: bold;");
    }
    generateCanFrame();
}

void MainWindow::toggleHeadlights() {
    isHeadlightsOn = !isHeadlightsOn;
    if (isHeadlightsOn) {
        headlightsBtn->setText("ON");
        headlightsBtn->setStyleSheet("background-color: yellow; color: black; font-weight: bold;");
    } else {
        headlightsBtn->setText("OFF");
        headlightsBtn->setStyleSheet("background-color: gray; color: white; font-weight: bold;");
    }
    generateCanFrame();
}

void MainWindow::loadDbcFile() {
    // 1. DBC Yükleme Fonksiyonu
    // Kullanıcıya dosya seçme penceresi açar ve seçilen dosyayı DbcParser'a gönderir.
    QString fileName = QFileDialog::getOpenFileName(this, "Open DBC File", "", "DBC Files (*.dbc);;All Files (*)");
    if (!fileName.isEmpty()) {
        if (dbcParser->parseFile(fileName)) {
            QMessageBox::information(this, "Success", "DBC file loaded successfully!");
            generateCanFrame(); // Update the frame with new DBC layout
        } else {
            QMessageBox::warning(this, "Error", "Failed to parse DBC file!");
        }
    }
}

void MainWindow::packSignal(QByteArray &frame, const DbcSignal &sig, uint64_t rawVal) {
    // 2. Sinyal Paketleme Fonksiyonu
    // Ham değeri alır, DBC'de belirtilen bit uzunluğuna ve başlangıç bitine göre
    // 8 Byte'lık (64 bit) CAN çerçevesinin içine kaydırarak sıkıştırır (Bitwise packing).
    uint64_t mask = (1ULL << sig.length) - 1;
    rawVal = (rawVal & mask);

    if (sig.isLittleEndian) {
        int bitsPacked = 0;
        int currentByte = sig.startBit / 8;
        int bitOffset = sig.startBit % 8;
        
        while (bitsPacked < sig.length && currentByte < frame.size()) {
            int bitsInThisByte = std::min(8 - bitOffset, sig.length - bitsPacked);
            uint8_t byteMask = ((1 << bitsInThisByte) - 1) << bitOffset;
            
            uint8_t valToPack = (rawVal >> bitsPacked) & ((1 << bitsInThisByte) - 1);
            frame[currentByte] = (frame[currentByte] & ~byteMask) | (valToPack << bitOffset);
            
            bitsPacked += bitsInThisByte;
            bitOffset = 0; // After first byte, start at bit 0 of next byte
            currentByte++;
        }
    } else {
        // Big Endian (Motorola)
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
            bitOffset = 7; // After first byte, next byte starts from bit 7 downwards
            currentByte--;
        }
    }
}

void MainWindow::generateCanFrame() {
    // 3. Mesaj Üretme ve Ekrana Yazdırma Fonksiyonu
    // Arayüzdeki her harekette (örn. Hız değişimi) tetiklenir ve ekrana basar.
    canMonitor->clear(); 

    if (dbcParser->isEmpty()) {
        // Yedek Mod (Fallback): DBC yüklenmemişse varsayılan sabit kod çalışır
        QByteArray data(8, 0);
        data[0] = isHandbrakeOn ? 0x01 : 0x00;
        data[1] = static_cast<unsigned char>(batterySlider->value());
        uint16_t speed = static_cast<uint16_t>(speedSpinBox->value());
        data[2] = speed & 0xFF;
        data[3] = (speed >> 8) & 0xFF;
        data[4] = isHeadlightsOn ? 0x01 : 0x00;
        
        QString hexString;
        for (int i = 0; i < 8; ++i) {
            hexString += QString("%1 ").arg(static_cast<quint8>(data[i]), 2, 16, QChar('0')).toUpper();
        }
        QString timeStr = QTime::currentTime().toString("HH:mm:ss.zzz");
        QString logLine = QString("[%1] TX -> ID: 0x1F4 DLC: 8 DATA: %2").arg(timeStr).arg(hexString.trimmed());
        canMonitor->append(logLine);
        return;
    }

    // Dynamic packing based on DBC matching
    bool speedFound = false, batteryFound = false, handbrakeFound = false, headlightsFound = false;
    DbcSignal speedSig = dbcParser->findSignalByKeywords({"Speed", "Spd", "Hiz", "Vel"}, speedFound);
    DbcSignal batterySig = dbcParser->findSignalByKeywords({"Battery", "Bat", "SOC", "Charge", "Pil", "Enerji"}, batteryFound);
    DbcSignal handbrakeSig = dbcParser->findSignalByKeywords({"Brake", "Hand", "Park", "Fren"}, handbrakeFound);
    DbcSignal headlightsSig = dbcParser->findSignalByKeywords({"Light", "Lamp", "Head", "Far", "Isik"}, headlightsFound);

    QMap<uint32_t, QByteArray> frames;

    auto prepareFrame = [&](const DbcSignal &sig, double physicalValue) {
        if (!frames.contains(sig.messageId)) {
            int dlc = dbcParser->getMessages().value(sig.messageId).dlc;
            if (dlc == 0) dlc = 8;
            frames[sig.messageId] = QByteArray(dlc, 0);
        }
        // Convert physical to raw: raw = (physical - offset) / factor
        double f = sig.factor != 0.0 ? sig.factor : 1.0;
        uint64_t rawValue = static_cast<uint64_t>((physicalValue - sig.offset) / f);
        packSignal(frames[sig.messageId], sig, rawValue);
    };

    if (speedFound) prepareFrame(speedSig, speedSlider->value());
    if (batteryFound) prepareFrame(batterySig, batterySlider->value());
    if (handbrakeFound) prepareFrame(handbrakeSig, isHandbrakeOn ? 1.0 : 0.0);
    if (headlightsFound) prepareFrame(headlightsSig, isHeadlightsOn ? 1.0 : 0.0);

    // Send all generated frames
    for (auto it = frames.begin(); it != frames.end(); ++it) {
        uint32_t fullId = it.key();
        uint32_t displayId = fullId & 0x1FFFFFFF; // Mask out Extended ID bit for display
        bool isExtended = (fullId & 0x80000000) != 0;
        QByteArray data = it.value();
        
        QString hexString;
        for (int i = 0; i < data.size(); ++i) {
            hexString += QString("%1 ").arg(static_cast<quint8>(data[i]), 2, 16, QChar('0')).toUpper();
        }

        QString timeStr = QTime::currentTime().toString("HH:mm:ss.zzz");
        QString logLine = QString("[%1] TX -> ID: 0x%2%3 DLC: %4 DATA: %5")
            .arg(timeStr)
            .arg(QString::number(displayId, 16).toUpper())
            .arg(isExtended ? " (EXT)" : "")
            .arg(data.size())
            .arg(hexString.trimmed());

        canMonitor->append(logLine);
    }
}

