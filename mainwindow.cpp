// Ana simulasyon dongusunun (Fizik motoru, hiz/koordinat hesaplamalari) ve kullanici arayuzu etkilesimlerinin gerceklestigi en temel dosyadir.

#include "mainwindow.h"
#include "dbmanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QGroupBox>
#include <QString>
#include <QTime>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>


    // Arayï¿½zï¿½ (UI) kuran, Timer'larï¿½ baï¿½latan ve temel deï¿½iï¿½kenleri sï¿½fï¿½rlayan kurucu fonksiyon (Constructor).
MainWindow::MainWindow(QWidget *parent) : QWidget(parent), isHandbrakeOn(true), isHeadlightsOn(false), isDoor1Open(false), isDoor2Open(false), isHvacOn(false), motorTemp(25.0), inverterTemp(25.0), currentSpeed(0.0), frameTickCounter(0), currentRouteIndex(0), currentLat(0.0), currentLng(0.0), totalRemainingDistance(0.0), etaSeconds(0.0) {
    dbcParser = new DbcParser();
    networkManager = new QNetworkAccessManager(this);
    QFile file("C:/Projeler/CanBusWebPlatform/Shared/parsed_dbc.json");
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write("[]");
        file.close();
    }
    
    QNetworkRequest request(QUrl("http://127.0.0.1:5085/api/dbc/reload"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reply = networkManager->post(request, QByteArray("{}"));
    connect(reply, &QNetworkReply::finished, [reply]() {
        reply->deleteLater();
    });

    connect(&DbManager::instance(), &DbManager::commandReceived, this, &MainWindow::onCommandReceived);
    setupUi();
    
    updater = new Updater(this);
    connect(updateBtn, &QPushButton::clicked, updater, &Updater::checkForUpdates);
    
    physicsTimer = new QTimer(this);
    connect(physicsTimer, &QTimer::timeout, this, &MainWindow::physicsLoop);
    physicsTimer->start(100);
    
    generateCanFrame();
}

MainWindow::~MainWindow() {
    delete dbcParser;
}

void MainWindow::setupUi() {
    setWindowTitle("CAN Bus Simulator - v3.5");
    resize(800, 600);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QGroupBox *dashboardGroup = new QGroupBox("Vehicle Dashboard");
    QGridLayout *dashboardLayout = new QGridLayout(dashboardGroup);

    QLabel *handbrakeLabel = new QLabel("Handbrake:");
    handbrakeBtn = new QPushButton("ENGAGED (ON)");
    handbrakeBtn->setCheckable(true);
    handbrakeBtn->setChecked(true);
    handbrakeBtn->setStyleSheet("background-color: red; color: white; font-weight: bold;");
    dashboardLayout->addWidget(handbrakeLabel, 0, 0);
    dashboardLayout->addWidget(handbrakeBtn, 0, 1);

    QLabel *speedLabel = new QLabel("Vehicle Speed (km/h):");
    speedSlider = new QSlider(Qt::Horizontal);
    speedSlider->setRange(0, 250);
    speedSpinBox = new QSpinBox();
    speedSpinBox->setRange(0, 250);
    connect(speedSlider, &QSlider::valueChanged, speedSpinBox, &QSpinBox::setValue);
    connect(speedSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), speedSlider, &QSlider::setValue);
    dashboardLayout->addWidget(speedLabel, 1, 0);
    dashboardLayout->addWidget(speedSlider, 1, 1);
    dashboardLayout->addWidget(speedSpinBox, 1, 2);

    QLabel *batteryLabel = new QLabel("Battery Charge (%):");
    batterySlider = new QSlider(Qt::Horizontal);
    batterySlider->setRange(0, 100);
    batterySlider->setValue(100);
    batteryProgressBar = new QProgressBar();
    batteryProgressBar->setRange(0, 100);
    batteryProgressBar->setValue(100);
    connect(batterySlider, &QSlider::valueChanged, batteryProgressBar, &QProgressBar::setValue);
    dashboardLayout->addWidget(batteryLabel, 2, 0);
    dashboardLayout->addWidget(batterySlider, 2, 1);
    dashboardLayout->addWidget(batteryProgressBar, 2, 2);

    QLabel *headlightsLabel = new QLabel("Headlights:");
    headlightsBtn = new QPushButton("OFF");
    headlightsBtn->setCheckable(true);
    headlightsBtn->setChecked(false);
    headlightsBtn->setStyleSheet("background-color: gray; color: white; font-weight: bold;");
    dashboardLayout->addWidget(headlightsLabel, 3, 0);
    dashboardLayout->addWidget(headlightsBtn, 3, 1);
    
    mainLayout->addWidget(dashboardGroup);

    QGroupBox *monitorGroup = new QGroupBox("CAN Bus Monitor");
    QVBoxLayout *monitorLayout = new QVBoxLayout(monitorGroup);
    canMonitor = new QTextEdit();
    canMonitor->setReadOnly(true);
    canMonitor->document()->setMaximumBlockCount(100);
    canMonitor->setStyleSheet("background-color: #1e1e1e; color: #00ff00; font-family: monospace;");
    monitorLayout->addWidget(canMonitor);
    mainLayout->addWidget(monitorGroup);

    updateBtn = new QPushButton("Check for Updates");
    mainLayout->addWidget(updateBtn);
    loadDbcBtn = new QPushButton("Load DBC File");
    mainLayout->addWidget(loadDbcBtn);

    connect(handbrakeBtn, &QPushButton::clicked, this, &MainWindow::toggleHandbrake);
    connect(headlightsBtn, &QPushButton::clicked, this, &MainWindow::toggleHeadlights);
    connect(loadDbcBtn, &QPushButton::clicked, this, &MainWindow::loadDbcFile);
}

void MainWindow::toggleHandbrake() {
    isHandbrakeOn = !isHandbrakeOn;
    updateToggleButton(handbrakeBtn, isHandbrakeOn, "ENGAGED (ON)", "RELEASED (OFF)", "red", "green");
}

void MainWindow::toggleHeadlights() {
    isHeadlightsOn = !isHeadlightsOn;
    updateToggleButton(headlightsBtn, isHeadlightsOn, "ON", "OFF", "yellow", "gray");
}

void MainWindow::loadDbcFile() {
    QString fileName = QFileDialog::getOpenFileName(this, "Open DBC File", "", "DBC Files (*.dbc);;All Files (*)");
    if (!fileName.isEmpty()) {
        if (dbcParser->parseFile(fileName)) {
            QMessageBox::information(this, "Success", "DBC file loaded successfully!");
            generateCanFrame();
            
            // Trigger web UI reload via SignalR
            QNetworkRequest request(QUrl("http://127.0.0.1:5085/api/dbc/reload"));
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            QNetworkReply *reply = networkManager->post(request, QByteArray("{}"));
            connect(reply, &QNetworkReply::finished, [reply]() {
                reply->deleteLater();
            });
        } else {
            QMessageBox::warning(this, "Error", "Failed to parse DBC file!");
        }
    }
}

void MainWindow::packSignal(QByteArray &frame, const DbcSignal &sig, uint64_t rawVal) {
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
            bitOffset = 0;
            currentByte++;
        }
    } else {
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
            bitOffset = 7;
            currentByte--;
        }
    }
}


    // Fizik motorunda hesaplanan araï¿½ verilerini (hï¿½z, konum) DBC dosyasï¿½na gï¿½re CAN frame'lerine ï¿½evirip veritabanï¿½na basar.
void MainWindow::generateCanFrame() {
    canMonitor->clear();
    
    // Her 1 saniyede (10 tick) bir cache'i temizle ki Web UI sonradan acilirsa senkronize olabilsin.
    frameTickCounter++;
    if (frameTickCounter >= 10) {
        lastLoggedValues.clear();
        frameTickCounter = 0;
    }

    auto logMessage = [&](uint32_t fullId, const QByteArray &data) {
        uint32_t displayId = fullId & 0x1FFFFFFF;
        bool isExtended = (fullId & 0x80000000) != 0;
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
    };

    if (dbcParser->isEmpty()) {
        QByteArray data(8, 0);
        data[0] = isHandbrakeOn ? 0x01 : 0x00;
        data[1] = static_cast<unsigned char>(batterySlider->value());
        uint16_t speed = static_cast<uint16_t>(speedSpinBox->value());
        data[2] = speed & 0xFF;
        data[3] = (speed >> 8) & 0xFF;
        data[4] = isHeadlightsOn ? 0x01 : 0x00;
        
        logMessage(0x1F4, data);

        QString hexStr;
        for (int i = 0; i < data.size(); ++i) {
            hexStr += QString("%1 ").arg(static_cast<quint8>(data[i]), 2, 16, QChar('0')).toUpper();
        }
        
        auto logIfChanged = [&](const QString& sigName, double val) {
            QString key = "0x1F4_" + sigName;
            double threshold = (sigName == "Latitude" || sigName == "Longitude") ? 0.000001 : 0.01;
            if (!lastLoggedValues.contains(key) || qAbs(lastLoggedValues[key] - val) > threshold) {
                DbManager::instance().logSignal("0x1F4", sigName, val, hexStr.trimmed());
                lastLoggedValues[key] = val;
            }
        };

        logIfChanged("Speed", speed);
        logIfChanged("Battery", batterySlider->value());
        logIfChanged("Handbrake", isHandbrakeOn ? 1 : 0);
        logIfChanged("Headlights", isHeadlightsOn ? 1 : 0);
        if (currentLat != 0.0 && currentLng != 0.0) {
            logIfChanged("Latitude", currentLat);
            logIfChanged("Longitude", currentLng);
        }
        logIfChanged("TotalDistance", totalRemainingDistance);
        logIfChanged("ETA_Seconds", etaSeconds);
        
        DbManager::instance().commit();
        return;
    }

    bool speedFound, batteryFound, handbrakeFound, headlightsFound;
    bool door1Found, door2Found, hvacFound, motorTempFound, inverterTempFound;

    // Use single signal to avoid battery flooding
    DbcSignal speedSig = dbcParser->findSignalByKeywords({"CCVS_WheelBasedSpeed", "Vehicle_Speed", "CCVS", "Speed", "Spd"}, speedFound);
    DbcSignal batterySig = dbcParser->findSignalByKeywords({"SOC", "GenericStateofCharge", "Battery_Level", "Battery", "Bat", "Charge"}, batteryFound);
    DbcSignal handbrakeSig = dbcParser->findSignalByKeywords({"CCVS_ParkingBrakeStatus", "FMS1_ParkingBrake", "Hand_Brake", "Brake", "Hand", "Park"}, handbrakeFound);
    DbcSignal headlightsSig = dbcParser->findSignalByKeywords({"FMS1_LowBeam", "Headlights", "FMS1_PositionLights", "Light", "Lamp", "Head"}, headlightsFound);
    DbcSignal door1Sig = dbcParser->findSignalByKeywords({"LockStatusDoor1", "Door1", "Door_1"}, door1Found);
    DbcSignal door2Sig = dbcParser->findSignalByKeywords({"LockStatusDoor2", "Door2", "Door_2"}, door2Found);
    DbcSignal hvacSig = dbcParser->findSignalByKeywords({"Driver_HVAC_Operation_Mode", "HVAC", "Klima", "A/C", "AC"}, hvacFound);
    DbcSignal motorTempSig = dbcParser->findSignalByKeywords({"Motor1_Temperature", "MotorTemp"}, motorTempFound);
    DbcSignal inverterTempSig = dbcParser->findSignalByKeywords({"Inverter1_Temperature", "InverterTemp"}, inverterTempFound);
    
    bool latFound, lngFound, distFound, etaFound;
    DbcSignal latSig = dbcParser->findSignalByKeywords({"Latitude"}, latFound);
    DbcSignal lngSig = dbcParser->findSignalByKeywords({"Longitude"}, lngFound);
    DbcSignal distSig = dbcParser->findSignalByKeywords({"TotalDistance"}, distFound);
    DbcSignal etaSig = dbcParser->findSignalByKeywords({"ETA_Seconds", "TotalDuration"}, etaFound);

    QMap<uint32_t, QByteArray> frames;
    struct LogInfo { uint32_t messageId; QString name; double physicalValue; };
    QList<LogInfo> signalsToLog;

    auto prepareFrame = [&](const DbcSignal &sig, double physicalValue) {
        if (!frames.contains(sig.messageId)) {
            int dlc = dbcParser->getMessages().value(sig.messageId).dlc;
            if (dlc == 0) dlc = 8;
            frames[sig.messageId] = QByteArray(dlc, 0);
        }
        double f = sig.factor != 0.0 ? sig.factor : 1.0;
        uint64_t rawValue = static_cast<uint64_t>((physicalValue - sig.offset) / f);
        packSignal(frames[sig.messageId], sig, rawValue);
        signalsToLog.append({sig.messageId, sig.name, physicalValue});
    };

    if (speedFound) prepareFrame(speedSig, speedSlider->value());
    if (batteryFound) prepareFrame(batterySig, batterySlider->value());
    if (handbrakeFound) prepareFrame(handbrakeSig, isHandbrakeOn ? 1.0 : 0.0);
    if (headlightsFound) prepareFrame(headlightsSig, isHeadlightsOn ? 1.0 : 0.0);
    if (door1Found) prepareFrame(door1Sig, isDoor1Open ? 1.0 : 0.0);
    if (door2Found) prepareFrame(door2Sig, isDoor2Open ? 1.0 : 0.0);
    if (hvacFound) prepareFrame(hvacSig, isHvacOn ? 1.0 : 0.0);
    if (motorTempFound) prepareFrame(motorTempSig, motorTemp);
    if (inverterTempFound) prepareFrame(inverterTempSig, inverterTemp);
    
    if (latFound && currentLat != 0.0) {
        prepareFrame(latSig, currentLat);
    } else if (currentLat != 0.0) {
        // Fallback for Web UI if DBC lacks GPS signals
        double threshold = 0.000001;
        if (!lastLoggedValues.contains("0x1F4_Latitude") || qAbs(lastLoggedValues["0x1F4_Latitude"] - currentLat) > threshold) {
            DbManager::instance().logSignal("0x1F4", "Latitude", currentLat, "00 00 00 00 00 00 00 00");
            lastLoggedValues["0x1F4_Latitude"] = currentLat;
        }
    }
    
    if (lngFound && currentLng != 0.0) {
        prepareFrame(lngSig, currentLng);
    } else if (currentLng != 0.0) {
        // Fallback for Web UI if DBC lacks GPS signals
        double threshold = 0.000001;
        if (!lastLoggedValues.contains("0x1F4_Longitude") || qAbs(lastLoggedValues["0x1F4_Longitude"] - currentLng) > threshold) {
            DbManager::instance().logSignal("0x1F4", "Longitude", currentLng, "00 00 00 00 00 00 00 00");
            lastLoggedValues["0x1F4_Longitude"] = currentLng;
        }
    }
    
    if (distFound) {
        prepareFrame(distSig, totalRemainingDistance);
    } else {
        // Fallback for Web UI
        double threshold = 1.0;
        if (!lastLoggedValues.contains("0x1F5_TotalDistance") || qAbs(lastLoggedValues["0x1F5_TotalDistance"] - totalRemainingDistance) > threshold) {
            DbManager::instance().logSignal("0x1F5", "TotalDistance", totalRemainingDistance, "00 00 00 00 00 00 00 00");
            lastLoggedValues["0x1F5_TotalDistance"] = totalRemainingDistance;
        }
    }
    
    if (etaFound) {
        prepareFrame(etaSig, etaSeconds);
    } else {
        // Fallback for Web UI
        double threshold = 1.0;
        if (!lastLoggedValues.contains("0x1F5_ETA_Seconds") || qAbs(lastLoggedValues["0x1F5_ETA_Seconds"] - etaSeconds) > threshold) {
            DbManager::instance().logSignal("0x1F5", "ETA_Seconds", etaSeconds, "00 00 00 00 00 00 00 00");
            lastLoggedValues["0x1F5_ETA_Seconds"] = etaSeconds;
        }
    }

    for (auto it = frames.begin(); it != frames.end(); ++it) {
        logMessage(it.key(), it.value());
    }

    for (const auto& logItem : signalsToLog) {
        QString messageIdHex = "0x" + QString::number(logItem.messageId, 16).toUpper();
        QString key = messageIdHex + "_" + logItem.name;
        
        double threshold = (logItem.name == "Latitude" || logItem.name == "Longitude") ? 0.000001 : 0.01;

        if (!lastLoggedValues.contains(key) || qAbs(lastLoggedValues[key] - logItem.physicalValue) > threshold) {
            QString hexStr;
            QByteArray data = frames[logItem.messageId];
            for (int i = 0; i < data.size(); ++i) {
                hexStr += QString("%1 ").arg(static_cast<quint8>(data[i]), 2, 16, QChar('0')).toUpper();
            }
            DbManager::instance().logSignal(messageIdHex, logItem.name, logItem.physicalValue, hexStr.trimmed());
            lastLoggedValues[key] = logItem.physicalValue;
        }
    }
    DbManager::instance().commit();
}


    // Fizik motorunun ana dï¿½ngï¿½sï¿½: Hï¿½z, mesafe, batarya tï¿½ketimi ve motor sï¿½caklï¿½k deï¿½iï¿½imlerini 50ms'de bir hesaplar.
void MainWindow::physicsLoop() {
    currentSpeed = speedSlider->value();
    if (isHandbrakeOn && currentSpeed > 0) {
        speedSlider->setValue(0);
        currentSpeed = 0;
    }
    
    // Daha dinamik motor sicaligi fizigi
    if (currentSpeed > 0 && motorTemp < 105.0) {
        // Hiz arttikca daha hizli isinir (hiz 100 ise 100ms'de +0.1 derece -> saniyede +1 derece)
        motorTemp += (currentSpeed / 1000.0); 
        inverterTemp += (currentSpeed / 1200.0);
    } else if (currentSpeed == 0 && motorTemp > 25.0 && !isHvacOn) {
        // Arac durdugunda yavasca ortam sicakligina (25) sogur
        motorTemp -= 0.05; 
        inverterTemp -= 0.05;
    } 

    if (isHvacOn) {
        // Klima aciksa aktif sogutma (ortam sicakligi 20'ye kadar)
        if (motorTemp > 20.0) motorTemp -= 0.15; 
        if (inverterTemp > 20.0) inverterTemp -= 0.15;
    }
    
    // GPS / Rota interpolasyonu
    if (!currentRoute.isEmpty() && currentRouteIndex < currentRoute.size() - 1 && currentSpeed > 0) {
        double speedMs = currentSpeed / 3.6; // km/h to m/s
        double distanceToMove = speedMs * 0.1; // 100ms per tick
        
        while (distanceToMove > 0 && currentRouteIndex < currentRoute.size() - 1) {
            double targetLat = currentRoute[currentRouteIndex + 1].lat;
            double targetLng = currentRoute[currentRouteIndex + 1].lng;
            
            // Mesafe hesapla (haversine)
            double lat1 = qDegreesToRadians(currentLat);
            double lng1 = qDegreesToRadians(currentLng);
            double lat2 = qDegreesToRadians(targetLat);
            double lng2 = qDegreesToRadians(targetLng);
            double dlon = lng2 - lng1;
            double dlat = lat2 - lat1;
            double a = qPow(qSin(dlat/2), 2) + qCos(lat1) * qCos(lat2) * qPow(qSin(dlon/2), 2);
            double c = 2 * qAsin(qSqrt(a));
            double distanceToTarget = 6371000 * c;
            
            if (distanceToMove >= distanceToTarget) {
                // Noktaya ulasti, sonrakine gec
                currentLat = targetLat;
                currentLng = targetLng;
                distanceToMove -= distanceToTarget;
                totalRemainingDistance -= distanceToTarget;
                currentRouteIndex++;
            } else {
                // Hedefe dogru vektorel ilerle
                double ratio = distanceToMove / distanceToTarget;
                currentLat += (targetLat - currentLat) * ratio;
                currentLng += (targetLng - currentLng) * ratio;
                totalRemainingDistance -= distanceToMove;
                distanceToMove = 0;
            }
        }
        
        if (totalRemainingDistance < 0) totalRemainingDistance = 0;
        
        // ETA hesapla
        if (currentSpeed > 0) {
            etaSeconds = totalRemainingDistance / (currentSpeed / 3.6);
        } else {
            etaSeconds = 0;
        }
        
        // Surus bittiyse
        if (currentRouteIndex >= currentRoute.size() - 1) {
            speedSlider->setValue(0);
            currentSpeed = 0;
            currentRoute.clear();
        }
    }
    
    generateCanFrame();
}

void MainWindow::onCommandReceived(const QString& commandName, const QString& commandValue) {
    auto parseBool = [](const QString& val, bool current) {
        return (val == "1" || val.toLower() == "true") ? true : (val == "0" || val.toLower() == "false" ? false : !current);
    };

    if (commandName == "speed_override" || commandName == "Set_Speed") { speedSlider->setValue(commandValue.toInt()); }
    else if (commandName == "battery_override" || commandName == "Set_Battery") { batterySlider->setValue(commandValue.toInt()); }
    else if (commandName == "handbrake_toggle" || commandName == "Toggle_Handbrake") { 
        isHandbrakeOn = parseBool(commandValue, isHandbrakeOn);
        updateToggleButton(handbrakeBtn, isHandbrakeOn, "ENGAGED (ON)", "RELEASED (OFF)", "red", "green");
    }
    else if (commandName == "headlights_toggle" || commandName == "Toggle_Headlights") { 
        isHeadlightsOn = parseBool(commandValue, isHeadlightsOn);
        updateToggleButton(headlightsBtn, isHeadlightsOn, "ON", "OFF", "yellow", "gray");
    }
    else if (commandName == "door1_toggle" || commandName == "Toggle_Door1") { isDoor1Open = parseBool(commandValue, isDoor1Open); }
    else if (commandName == "door2_toggle" || commandName == "Toggle_Door2") { isDoor2Open = parseBool(commandValue, isDoor2Open); }
    else if (commandName == "hvac_toggle" || commandName == "Toggle_HVAC") { isHvacOn = parseBool(commandValue, isHvacOn); }
    else if (commandName == "Load_DBC_File") {
        QString sharedDir = "C:\\Projeler\\CanBusWebPlatform\\Shared\\";
        QString fullPath = sharedDir + commandValue;
        dbcParser->parseFile(fullPath);
    }
    else if (commandName == "Set_Route_Data") {
        QJsonDocument doc = QJsonDocument::fromJson(commandValue.toUtf8());
        if (doc.isArray()) {
            QJsonArray arr = doc.array();
            currentRoute.clear();
            for (int i = 0; i < arr.size(); ++i) {
                QJsonObject obj = arr[i].toObject();
                currentRoute.append({obj["lat"].toDouble(), obj["lng"].toDouble()});
            }
            if (!currentRoute.isEmpty()) {
                currentRouteIndex = 0;
                currentLat = currentRoute[0].lat;
                currentLng = currentRoute[0].lng;
                  
                  DbManager::instance().logSignal("0x1F4", "Latitude", currentLat, "");
                  DbManager::instance().logSignal("0x1F4", "Longitude", currentLng, "");
                  lastLoggedValues["0x1F4_Latitude"] = currentLat;
                  lastLoggedValues["0x1F4_Longitude"] = currentLng;
                  
                  totalRemainingDistance = 0;
                for (int i = 0; i < currentRoute.size() - 1; ++i) {
                    double lat1 = qDegreesToRadians(currentRoute[i].lat);
                    double lng1 = qDegreesToRadians(currentRoute[i].lng);
                    double lat2 = qDegreesToRadians(currentRoute[i+1].lat);
                    double lng2 = qDegreesToRadians(currentRoute[i+1].lng);
                    double dlon = lng2 - lng1;
                    double dlat = lat2 - lat1;
                    double a = qPow(qSin(dlat/2), 2) + qCos(lat1) * qCos(lat2) * qPow(qSin(dlon/2), 2);
                    double c = 2 * qAsin(qSqrt(a));
                    totalRemainingDistance += 6371000 * c;
                }
            }
        }
    }
    else if (commandName == "Set_Route") {
        QStringList parts = commandValue.split(';');
        if (parts.size() == 2) {
            QStringList start = parts[0].split(',');
            QStringList end = parts[1].split(',');
            if (start.size() == 2 && end.size() == 2) {
                currentRoute.clear();
                currentRoute.append({start[0].toDouble(), start[1].toDouble()});
                currentRoute.append({end[0].toDouble(), end[1].toDouble()});
                currentRouteIndex = 0;
                currentLat = currentRoute[0].lat;
                currentLng = currentRoute[0].lng;
                
                double lat1 = qDegreesToRadians(currentRoute[0].lat);
                double lng1 = qDegreesToRadians(currentRoute[0].lng);
                double lat2 = qDegreesToRadians(currentRoute[1].lat);
                double lng2 = qDegreesToRadians(currentRoute[1].lng);
                double dlon = lng2 - lng1;
                double dlat = lat2 - lat1;
                double a = qPow(qSin(dlat/2), 2) + qCos(lat1) * qCos(lat2) * qPow(qSin(dlon/2), 2);
                double c = 2 * qAsin(qSqrt(a));
                totalRemainingDistance = 6371000 * c;
            }
        }
    }
    else if (commandName == "Stop_Driving") {
        currentRoute.clear();
        speedSlider->setValue(0);
        currentSpeed = 0;
        totalRemainingDistance = 0;
        etaSeconds = 0;
    }
}

void MainWindow::updateToggleButton(QPushButton* btn, bool state, const QString& onText, const QString& offText, const QString& onColor, const QString& offColor) {
    if (!btn) return;
    btn->setChecked(state);
    if (state) {
        btn->setText(onText);
        btn->setStyleSheet(QString("background-color: %1; color: %2; font-weight: bold;").arg(onColor, onColor == "yellow" ? "black" : "white"));
    } else {
        btn->setText(offText);
        btn->setStyleSheet(QString("background-color: %1; color: white; font-weight: bold;").arg(offColor));
    }
}





