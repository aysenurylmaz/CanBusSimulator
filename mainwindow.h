// mainwindow.h
// Kullanıcı arayüzünün (UI) başlık dosyasıdır. Arayüzdeki butonların, 
// kaydırıcıların ve metin kutularının tanımlarını içerir.
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QProgressBar>
#include <QTextEdit>
#include "updater.h"
#include "dbcparser.h"

class MainWindow : public QWidget {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void generateCanFrame();
    void toggleHandbrake();
    void toggleHeadlights();
    void loadDbcFile();

private:
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
    DbcParser *dbcParser;

    // State Variables
    bool isHandbrakeOn;
    bool isHeadlightsOn;
    
    void setupUi();
    void packSignal(QByteArray &frame, const DbcSignal &sig, uint64_t value);
};

#endif // MAINWINDOW_H
