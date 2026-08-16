// MainWindow s�n�f�n�n tan�mlamalar�. UI bile�enleri, timer'lar ve fizik/ara� sim�lasyonu mant���na ait fonksiyon imzalar�n� bar�nd�r�r.

// mainwindow.h
// Kullanici arayuzunun (UI) baslik dosyasidir. Arayuzdeki butonlarin, 
// kaydiricilarin ve metin kutularinin tanimlarini icerir.
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QProgressBar>
#include <QTextEdit>
#include <QByteArray>
#include <QTimer>
#include "updater.h"
#include "dbcparser.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QList>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QtMath>

class MainWindow : public QWidget {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void generateCanFrame();
    void physicsLoop();
    void toggleHandbrake();
    void toggleHeadlights();
    void loadDbcFile();
    void onCommandReceived(const QString& commandName, const QString& commandValue);

private:
    void updateToggleButton(QPushButton* btn, bool state, const QString& onText, const QString& offText, const QString& onColor, const QString& offColor);
    // UI Elements
    QPushButton *handbrakeBtn;
    QSlider *speedSlider;
    QSpinBox *speedSpinBox;
    QSlider *batterySlider;
    QProgressBar *batteryProgressBar;
    QTextEdit *canMonitor;
    QPushButton *updateBtn;
    QPushButton *headlightsBtn;
    QPushButton *loadDbcBtn;

    Updater *updater;
    QTimer *physicsTimer;
    DbcParser *dbcParser;
    QNetworkAccessManager *networkManager;

    // GPS & Route State
    struct GeoPoint { double lat; double lng; };
    QList<GeoPoint> currentRoute;
    int currentRouteIndex;
    double currentLat;
    double currentLng;
    double totalRemainingDistance;
    double etaSeconds;

    // State Variables
    bool isHandbrakeOn;
    bool isHeadlightsOn;
    
    // EV Specific Properties
    bool isDoor1Open;
    bool isDoor2Open;
    bool isHvacOn;
    double motorTemp;
    double inverterTemp;
    double currentSpeed;
    
    QMap<QString, double> lastLoggedValues;
    int frameTickCounter;
    
    void setupUi();
    void packSignal(QByteArray &frame, const DbcSignal &sig, uint64_t value);
    void exportDbcToJson(const QString& outputPath);
};

#endif // MAINWINDOW_H


