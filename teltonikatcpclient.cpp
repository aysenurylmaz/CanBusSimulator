#include "teltonikatcpclient.h"
#include <QDebug>

TeltonikaTcpClient::TeltonikaTcpClient(QObject *parent)
    : QObject(parent)
{
    socket = new QTcpSocket(this);

    connect(socket, &QTcpSocket::readyRead, this, &TeltonikaTcpClient::onReadyRead);
    connect(socket, &QTcpSocket::connected, this, &TeltonikaTcpClient::onConnected);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(socket, &QTcpSocket::errorOccurred, this, &TeltonikaTcpClient::onErrorOccurred);
#else
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &TeltonikaTcpClient::onErrorOccurred);
#endif
}

TeltonikaTcpClient::~TeltonikaTcpClient()
{
    disconnectFromServer();
}

void TeltonikaTcpClient::connectToServer(const QString& ip, quint16 port)
{
    if (socket->state() == QAbstractSocket::UnconnectedState) {
        qDebug() << "[TeltonikaTcpClient] Sunucuya baglaniliyor:" << ip << port;
        socket->connectToHost(ip, port);
    }
}

void TeltonikaTcpClient::sendData(const QByteArray& packet)
{
    if (socket->state() == QAbstractSocket::ConnectedState) {
        qDebug() << "[TeltonikaTcpClient] Paket sunucuya gonderiliyor. Boyut:" << packet.size() << "byte.";
        socket->write(packet);
        socket->flush();
    } else {
        qDebug() << "[TeltonikaTcpClient] Hata: Sunucuya bagli degil! Paket gonderilemedi.";
    }
}

void TeltonikaTcpClient::disconnectFromServer()
{
    if (socket->isOpen()) {
        socket->close();
    }
}

void TeltonikaTcpClient::onReadyRead()
{
    // Sunucu basarili bir kayit aldiginda gonderdigimiz NumberOfData2 degeri kadar (4-byte integer) onay gonderir.
    QByteArray response = socket->readAll();
    
    qDebug() << "[TeltonikaTcpClient] Sunucudan yanit (ACK) geldi! Boyut:" << response.size();
    
    // Gelen 4-byte degeri (Big Endian) tam sayiya (integer) cevirip okuyalim
    if (response.size() >= 4) {
        quint32 acceptedRecords = (static_cast<quint8>(response[0]) << 24) |
                                  (static_cast<quint8>(response[1]) << 16) |
                                  (static_cast<quint8>(response[2]) << 8)  |
                                  static_cast<quint8>(response[3]);
        
        qDebug() << "[TeltonikaTcpClient] Sunucu su kadar kaydi basariyla kabul etti:" << acceptedRecords;
    }
}

void TeltonikaTcpClient::onConnected()
{
    qDebug() << "[TeltonikaTcpClient] Sunucuya basariyla baglanildi!";
}

void TeltonikaTcpClient::onErrorOccurred(QAbstractSocket::SocketError socketError)
{
    qDebug() << "[TeltonikaTcpClient] Soket Hatasi Olustu:" << socket->errorString();
}
