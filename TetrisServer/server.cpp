#include "server.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket> // 補上這個 include 比較保險

Server::Server(QObject *parent) : QObject(parent)
{
    tcpServer = new QTcpServer(this);
    if(tcpServer->listen(QHostAddress::Any, 12345)){
        qDebug() << "Tetris Server started on port 12345";
    } else {
        qDebug() << "Server failed to start!";
    }
    connect(tcpServer, &QTcpServer::newConnection, this, &Server::onNewConnection);
}

void Server::onNewConnection()
{
    QTcpSocket *clientSocket = tcpServer->nextPendingConnection();
    clients.append(clientSocket);

    connect(clientSocket, &QTcpSocket::readyRead, this, &Server::onReadyRead);
    connect(clientSocket, &QTcpSocket::disconnected, this, &Server::onDisconnected);

    qDebug() << "Client connected. Total:" << clients.size();

    if (clients.size() == 2) {
        qDebug() << "Match Found! Sending start signal.";
        QJsonObject root;
        root["type"] = "start";

        // [修正重點] 使用 Compact 模式，確保 JSON 是一整行，不會被換行符號切斷
        QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Compact) + "\n";

        for (QTcpSocket *socket : clients) {
            if(socket->state() == QAbstractSocket::ConnectedState) {
                socket->write(data);
                socket->flush(); // 確保立即送出
            }
        }
    }
}

void Server::onReadyRead()
{
    QTcpSocket *senderSocket = qobject_cast<QTcpSocket*>(sender());
    if (!senderSocket) return;

    QByteArray data = senderSocket->readAll();

    // 廣播給對手
    for (QTcpSocket *socket : clients) {
        if (socket != senderSocket && socket->state() == QAbstractSocket::ConnectedState) {
            socket->write(data);
            socket->flush();
        }
    }
}

void Server::onDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        clients.removeAll(socket);
        socket->deleteLater();
        qDebug() << "Client disconnected. Remaining:" << clients.size();

        // 如果還有人在，通知他遊戲結束
        if (!clients.isEmpty()) {
            QJsonObject root;
            root["type"] = "game_over";

            // [修正重點] 這裡也要記得加 Compact
            QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Compact) + "\n";

            QTcpSocket* remainingClient = clients.first();
            if(remainingClient->state() == QAbstractSocket::ConnectedState) {
                remainingClient->write(data);
                remainingClient->flush();
            }
        }
    }
}
