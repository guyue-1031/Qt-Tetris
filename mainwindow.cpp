#include "mainwindow.h"
#include <QPainter>
#include <QKeyEvent>
#include <QDebug>
#include <QMessageBox>
#include <QInputDialog>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <random>

// JSON
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

// [新增] 確保這些有被 include
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QSoundEffect>

const int GAME_COLS = 10;
const int GAME_ROWS = 20;
const int CELL_SIZE = 30;
const int BOARD_PIXEL_W = GAME_COLS * CELL_SIZE;
const int BOARD_PIXEL_H = GAME_ROWS * CELL_SIZE;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , isGameMode(false), isOnlineMode(false)
    , isPaused(false), isGameOver(false), isWaitingForOpponent(false)
    , score(0), level(1), heldShape(0), canHold(true)
    , opponentHold(0)
    , timer(nullptr), lockTimer(nullptr), socket(nullptr)
    , menuWidget(nullptr), titleLabel(nullptr), nameInput(nullptr)
    , btnLocal(nullptr), btnOnline(nullptr), btnBack(nullptr)
    , bgmPlayer(nullptr), bgmOutput(nullptr), clearSound(nullptr)
{
    resize(1200, 800);
    setWindowTitle("Qt Tetris - Ultimate Battle");

    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(30, 30, 30));
    setPalette(pal);

    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    board.resize(GAME_COLS * GAME_ROWS);
    board.fill(0);
    opponentBoard.resize(GAME_COLS * GAME_ROWS);
    opponentBoard.fill(0);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::gameLoop);

    lockTimer = new QTimer(this);
    lockTimer->setSingleShot(true);
    connect(lockTimer, &QTimer::timeout, this, &MainWindow::pieceDropped);

    socket = new QTcpSocket(this);
    connect(socket, &QTcpSocket::connected, this, &MainWindow::onSocketConnected);
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onSocketReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &MainWindow::onSocketDisconnected);

    // --- [新增] 音樂與音效初始化 ---

    // 取得 .exe 所在的絕對路徑 (用於定位外部音樂檔)
    QString appPath = QCoreApplication::applicationDirPath();

    // 1. 背景音樂 (BGM)
    bgmPlayer = new QMediaPlayer(this);
    bgmOutput = new QAudioOutput(this);
    bgmPlayer->setAudioOutput(bgmOutput);

    // 設定 BGM 檔案路徑：改用 fromLocalFile 指向 .exe 資料夾下的檔案
    // 注意：這要求 bgm.mp3 必須放在 .exe 旁邊
    bgmPlayer->setSource(QUrl::fromLocalFile(appPath + "/bgm.mp3"));

    bgmOutput->setVolume(0.3); // [已調整] 音量調小至 0.3
    bgmPlayer->setLoops(QMediaPlayer::Infinite); // 無限循環

    // 2. 消除音效 (SFX)
    clearSound = new QSoundEffect(this);

    // 設定音效檔案路徑：改用 fromLocalFile 指向 .exe 資料夾下的檔案
    // 注意：這要求 clear.wav 必須放在 .exe 旁邊
    clearSound->setSource(QUrl::fromLocalFile(appPath + "/clear.wav"));
    clearSound->setVolume(1.0);

    initMenu();
}

MainWindow::~MainWindow()
{
}

// --- 初始化選單 ---
void MainWindow::initMenu()
{
    if (menuWidget) return;

    menuWidget = new QWidget(this);
    menuWidget->setGeometry(0, 0, width(), height());

    QVBoxLayout *mainLayout = new QVBoxLayout(menuWidget);
    mainLayout->setSpacing(50);

    // --- 標題 ---
    titleLabel = new QLabel("TETR.IO", this);
    QFont font = titleLabel->font();
    font.setPointSize(80);
    font.setBold(true);
    font.setItalic(true);
    titleLabel->setFont(font);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("QLabel { color : white; letter-spacing: 5px; }");

    // --- 內容區 ---
    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->setContentsMargins(0, 0, 0, 0);

    // 左：介紹
    QVBoxLayout *leftLayout = new QVBoxLayout();
    leftLayout->setAlignment(Qt::AlignTop | Qt::AlignRight);

    QLabel *introTitle = new QLabel("--- 遊戲操作 ---", this);
    introTitle->setStyleSheet("color: #4CAF50; font-size: 18px; font-weight: bold; margin-bottom: 5px;");
    introTitle->setAlignment(Qt::AlignHCenter);

    QLabel *introText = new QLabel(this);
    introText->setText("↑ : 旋轉\n← → : 移動\n↓ : 緩降\nSpace : 硬降\nC : 保留\nESC : 暫停");
    introText->setFixedWidth(200);
    introText->setStyleSheet("QLabel { color: #DDD; font-size: 15px; line-height: 160%; background-color: rgba(0,0,0,0.3); padding: 15px; border-radius: 8px; border: 1px solid #555; }");
    introText->setAlignment(Qt::AlignLeft);
    leftLayout->addWidget(introTitle);
    leftLayout->addWidget(introText);

    // 中：按鈕
    QVBoxLayout *centerLayout = new QVBoxLayout();
    centerLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    centerLayout->setSpacing(25);

    QString btnStyle = "QPushButton { font-size: 20px; padding: 15px; background-color: #4CAF50; color: white; border-radius: 8px; font-weight: bold; min-width: 200px; } QPushButton:hover { background-color: #45a049; } QPushButton:disabled { background-color: #555; color: #aaa; }";

    btnLocal = new QPushButton("單人練習 (Local)", this);
    btnOnline = new QPushButton("多人對戰 (Online)", this);
    btnLocal->setStyleSheet(btnStyle);
    btnOnline->setStyleSheet(btnStyle);
    btnLocal->setFixedSize(280, 70);
    btnOnline->setFixedSize(280, 70);

    centerLayout->addWidget(btnLocal);
    centerLayout->addWidget(btnOnline);

    // 右：輸入
    QVBoxLayout *rightLayout = new QVBoxLayout();
    rightLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    rightLayout->setSpacing(15);

    QLabel *nameLabel = new QLabel("玩家名稱", this);
    nameLabel->setStyleSheet("color: #AAA; font-size: 16px; font-weight: bold;");
    nameLabel->setAlignment(Qt::AlignHCenter);

    nameInput = new QLineEdit(this);
    nameInput->setText("Player 1");
    nameInput->setMaxLength(10);
    nameInput->setFixedWidth(200);
    nameInput->setAlignment(Qt::AlignCenter);
    nameInput->setStyleSheet("QLineEdit { font-size: 20px; padding: 10px; border-radius: 5px; background-color: #eee; color: #333; border: 2px solid #555; } QLineEdit:focus { border: 2px solid #4CAF50; }");

    rightLayout->addWidget(nameLabel);
    rightLayout->addWidget(nameInput);

    // 組合
    contentLayout->addStretch(1);
    contentLayout->addLayout(leftLayout);
    contentLayout->addSpacing(40);
    contentLayout->addLayout(centerLayout);
    contentLayout->addSpacing(40);
    contentLayout->addLayout(rightLayout);
    contentLayout->addStretch(1);

    mainLayout->addStretch(1);
    mainLayout->addWidget(titleLabel);
    mainLayout->addLayout(contentLayout);
    mainLayout->addStretch(1);

    btnBack = new QPushButton("Quit Game", this);
    btnBack->hide();
    btnBack->setGeometry(20, 20, 100, 40);
    btnBack->setStyleSheet("QPushButton { font-size: 14px; background-color: #d32f2f; color: white; border-radius: 4px; }");

    connect(btnLocal, &QPushButton::clicked, this, &MainWindow::onLocalBattleClicked);
    connect(btnOnline, &QPushButton::clicked, this, &MainWindow::onOnlineBattleClicked);
    connect(btnBack, &QPushButton::clicked, this, &MainWindow::onBackClicked);
}

void MainWindow::onLocalBattleClicked()
{
    localPlayerName = nameInput->text().trimmed();
    if(localPlayerName.isEmpty()) localPlayerName = "Player 1";
    opponentName = "CPU";

    isGameMode = true;
    isOnlineMode = false;
    isWaitingForOpponent = false;
    menuWidget->hide();
    startGame();
    this->setFocus();
}

void MainWindow::onOnlineBattleClicked()
{
    localPlayerName = nameInput->text().trimmed();
    if(localPlayerName.isEmpty()) localPlayerName = "Player";
    opponentName = "Waiting...";

    bool ok;
    QString ip = QInputDialog::getText(this, "連線", "請輸入伺服器 IP:", QLineEdit::Normal, "127.0.0.1", &ok);
    if (ok && !ip.isEmpty()) {
        socket->connectToHost(ip, 12345);
        titleLabel->setText("連線中...");
        btnLocal->setEnabled(false);
        btnOnline->setEnabled(false);
        nameInput->setEnabled(false);
    }
}

void MainWindow::onSocketConnected()
{
    menuWidget->hide();
    isOnlineMode = true;
    opponentBoard.fill(0);
    opponentNextPieces.clear();
    opponentHold = 0;
    opponentName = "Connecting...";
    isWaitingForOpponent = true;
    isGameMode = true;

    btnBack->show();
    btnBack->raise();
    this->setFocus();

    sendPlayerName();
    update();
}

void MainWindow::onSocketDisconnected()
{
    isGameMode = false;
    isOnlineMode = false;
    timer->stop();
    bgmPlayer->stop(); // 斷線停音樂
    QMessageBox::warning(this, "斷線", "與伺服器斷開連線。");
    onBackClicked();
}

void MainWindow::onBackClicked()
{
    timer->stop();
    isGameMode = false;
    isOnlineMode = false;
    isPaused = false;

    // [新增] 停止音樂
    bgmPlayer->stop();

    if(socket->isOpen()) socket->disconnectFromHost();

    btnBack->hide();
    if(menuWidget) {
        menuWidget->show();
        menuWidget->raise();
    }

    if(btnLocal) btnLocal->setEnabled(true);
    if(btnOnline) btnOnline->setEnabled(true);
    if(nameInput) nameInput->setEnabled(true);

    if(titleLabel) titleLabel->setText("TETR.IO");
    update();
}

void MainWindow::onSocketReadyRead()
{
    QByteArray data = socket->readAll();
    QList<QByteArray> messages = data.split('\n');

    for(const QByteArray &msg : messages) {
        if(msg.isEmpty()) continue;
        QJsonDocument doc = QJsonDocument::fromJson(msg);
        if(!doc.isObject()) continue;

        QJsonObject root = doc.object();
        QString type = root["type"].toString();

        if (type == "player_info") {
            if(root.contains("name")) {
                opponentName = root["name"].toString();
                if(opponentName.isEmpty()) opponentName = "Opponent";
                update();
            }
        }
        else if (type == "start" || type == "game_start") {
            isWaitingForOpponent = false;
            startGame();
        }
        else if (type == "game_state") {
            if(root.contains("board")) {
                QJsonArray arr = root["board"].toArray();
                if(opponentBoard.size() != GAME_COLS * GAME_ROWS)
                    opponentBoard.resize(GAME_COLS * GAME_ROWS);
                opponentBoard.fill(0);
                for (int i=0; i < qMin(arr.size(), opponentBoard.size()); ++i)
                    opponentBoard[i] = arr[i].toInt();
            }
            if(root.contains("hold")) {
                opponentHold = root["hold"].toInt();
            }
            if(root.contains("next_queue")) {
                QJsonArray nextArr = root["next_queue"].toArray();
                opponentNextPieces.clear();
                for(auto v : nextArr) {
                    opponentNextPieces.append(v.toInt());
                }
            }
            update();
        }
        else if (type == "attack") addGarbageLines(root["lines"].toInt());
        else if (type == "game_over") {
            isGameOver = true;
            timer->stop();
            bgmPlayer->stop(); // 遊戲結束停音樂
            QMessageBox::information(this, "結果", "你贏了！對手輸了。");
            onBackClicked();
        }
    }
}

void MainWindow::sendPlayerName()
{
    if (!isOnlineMode || socket->state() != QAbstractSocket::ConnectedState) return;
    QJsonObject root;
    root["type"] = "player_info";
    root["name"] = localPlayerName;
    socket->write(QJsonDocument(root).toJson(QJsonDocument::Compact) + "\n");
    socket->flush();
}

void MainWindow::sendGameState()
{
    if (!isOnlineMode || socket->state() != QAbstractSocket::ConnectedState) return;

    QJsonObject root;
    root["type"] = "game_state";

    QVector<int> tempBoard = board;
    QVector<QPoint> coords = getShapeCoords(currentShape, currentRotation);
    for(const QPoint &p : coords) {
        int x = currentX + p.x();
        int y = currentY + p.y();
        if (x >= 0 && x < GAME_COLS && y >= 0 && y < GAME_ROWS) {
            tempBoard[y * GAME_COLS + x] = currentShape;
        }
    }
    QJsonArray boardArr;
    for (int val : tempBoard) boardArr.append(val);
    root["board"] = boardArr;
    root["hold"] = heldShape;
    QJsonArray nextArr;
    for(int i=0; i < qMin(nextPieces.size(), 3); i++) {
        nextArr.append(nextPieces[i]);
    }
    root["next_queue"] = nextArr;
    socket->write(QJsonDocument(root).toJson(QJsonDocument::Compact) + "\n");
    socket->flush();
}

void MainWindow::sendAttack(int lines)
{
    if (!isOnlineMode || socket->state() != QAbstractSocket::ConnectedState) return;
    QJsonObject root; root["type"] = "attack"; root["lines"] = lines;
    socket->write(QJsonDocument(root).toJson(QJsonDocument::Compact) + "\n");
    socket->flush();
}

// --- 遊戲邏輯 ---

void MainWindow::startGame()
{
    board.fill(0);
    opponentBoard.fill(0);
    opponentNextPieces.clear();
    opponentHold = 0;

    score = 0;
    level = 1;
    dropSpeed = 1000;
    isGameOver = false;
    isPaused = false;

    btnBack->show();
    btnBack->raise();

    bag.clear();
    nextPieces.clear();
    refillBag();
    refillBag();
    while(nextPieces.size() < 5) {
        if(bag.isEmpty()) refillBag();
        nextPieces.append(bag.takeFirst());
    }

    canHold = true;
    heldShape = 0;

    // [新增] 播放音樂
    if(bgmPlayer->playbackState() != QMediaPlayer::PlayingState) {
        bgmPlayer->play();
    }

    spawnPiece();
    timer->start(dropSpeed);

    if(isOnlineMode) sendGameState();
    update();
}

void MainWindow::spawnPiece() {
    while (nextPieces.size() < 5) {
        if (bag.isEmpty()) refillBag();
        nextPieces.append(bag.takeFirst());
    }

    currentShape = nextPieces.takeFirst();
    canHold = true;
    currentRotation = 0;
    currentX = GAME_COLS / 2 - 1;
    currentY = 0;

    if (!tryMove(currentX, currentY, currentRotation)) {
        isGameOver = true;
        timer->stop();
        bgmPlayer->stop(); // 遊戲結束停音樂

        if(isOnlineMode) {
            QJsonObject root; root["type"] = "game_over";
            socket->write(QJsonDocument(root).toJson(QJsonDocument::Compact) + "\n");
            QMessageBox::information(this, "Game Over", "你輸了！");
            onBackClicked();
        } else {
            QMessageBox::information(this, "Game Over", "遊戲結束！");
            onBackClicked();
        }
    }
    if(isOnlineMode) sendGameState();
}

void MainWindow::holdPiece()
{
    if (isGameOver || isPaused || !canHold) return;

    if (heldShape == 0) {
        heldShape = currentShape;
        spawnPiece();
    } else {
        std::swap(currentShape, heldShape);
        currentX = GAME_COLS / 2 - 1;
        currentY = 0;
        currentRotation = 0;
        if (!tryMove(currentX, currentY, currentRotation)) {
            isGameOver = true; timer->stop();
        }
    }
    canHold = false;
    update();
    if(isOnlineMode) sendGameState();
}

void MainWindow::rotateWithWallKick()
{
    int nextRot = (currentRotation + 1) % 4;
    if (tryMove(currentX, currentY, nextRot)) currentRotation = nextRot;
    else if (tryMove(currentX + 1, currentY, nextRot)) { currentX += 1; currentRotation = nextRot; }
    else if (tryMove(currentX - 1, currentY, nextRot)) { currentX -= 1; currentRotation = nextRot; }
    update();
    if(isOnlineMode) sendGameState();
}

bool MainWindow::tryMove(int newX, int newY, int newRot) {
    QVector<QPoint> coords = getShapeCoords(currentShape, newRot);
    for (const QPoint &p : coords) {
        int x = newX + p.x();
        int y = newY + p.y();
        if (x < 0 || x >= GAME_COLS || y >= GAME_ROWS) return false;
        if (y >= 0) {
            int idx = y * GAME_COLS + x;
            if (idx >= 0 && idx < board.size() && board[idx] != 0) return false;
        }
    }
    return true;
}

void MainWindow::gameLoop() {
    if (isPaused || isGameOver || isWaitingForOpponent) return;
    if (tryMove(currentX, currentY + 1, currentRotation)) currentY++;
    else if (!lockTimer->isActive()) lockTimer->start(500);
    update();
}

void MainWindow::pieceDropped() {
    if (tryMove(currentX, currentY + 1, currentRotation)) return;
    placePiece();
}

void MainWindow::placePiece() {
    QVector<QPoint> coords = getShapeCoords(currentShape, currentRotation);
    for (const QPoint &p : coords) {
        int x = currentX + p.x();
        int y = currentY + p.y();
        if (x >= 0 && x < GAME_COLS && y >= 0 && y < GAME_ROWS) {
            int idx = y * GAME_COLS + x;
            if (idx >= 0 && idx < board.size()) board[idx] = currentShape;
        }
    }
    clearLines();
    spawnPiece();
    if(isOnlineMode) sendGameState();
}

void MainWindow::clearLines() {
    int linesCleared = 0;
    for (int y = GAME_ROWS - 1; y >= 0; y--) {
        bool full = true;
        for (int x = 0; x < GAME_COLS; x++) {
            if (board[y * GAME_COLS + x] == 0) { full = false; break; }
        }
        if (full) {
            linesCleared++;
            for (int k = y; k > 0; k--) {
                for (int x = 0; x < GAME_COLS; x++) board[k * GAME_COLS + x] = board[(k - 1) * GAME_COLS + x];
            }
            for (int x = 0; x < GAME_COLS; x++) board[x] = 0;
            y++;
        }
    }
    if (linesCleared > 0) {
        // [新增] 播放消除音效
        clearSound->play();

        int points[] = {0, 100, 300, 500, 800};
        score += points[std::min(linesCleared, 4)] * level;
        updateGameLevel();
        if (isOnlineMode && linesCleared > 1) sendAttack(linesCleared - 1);
    }
}

void MainWindow::addGarbageLines(int count)
{
    if(count <= 0) return;
    if(count >= GAME_ROWS) count = GAME_ROWS - 1;
    for (int y = 0; y < GAME_ROWS - count; y++) {
        for (int x = 0; x < GAME_COLS; x++) board[y * GAME_COLS + x] = board[(y + count) * GAME_COLS + x];
    }
    for (int y = GAME_ROWS - count; y < GAME_ROWS; y++) {
        int hole = std::rand() % GAME_COLS;
        for (int x = 0; x < GAME_COLS; x++) {
            board[y * GAME_COLS + x] = (x == hole) ? 0 : 8;
        }
    }
    if (!tryMove(currentX, currentY, currentRotation)) currentY -= count;
    update();
}

void MainWindow::updateGameLevel() {
    int newLevel = (score / 1000) + 1;
    if (newLevel > level) {
        level = newLevel;
        dropSpeed = std::max(100, 1000 - (level - 1) * 100);
        timer->setInterval(dropSpeed);
    }
}

// --- 繪圖事件 ---
void MainWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);

    if (menuWidget) menuWidget->resize(width(), height());

    painter.fillRect(rect(), QColor(30, 30, 30));

    if (!isGameMode) {
        return;
    }

    if (isWaitingForOpponent) {
        painter.setPen(Qt::white);
        QFont f = painter.font(); f.setPointSize(24); painter.setFont(f);
        painter.drawText(rect(), Qt::AlignCenter, "等待對手連線 (Waiting for Opponent)...");
        return;
    }

    int myBoardX;
    int oppBoardX = 0;
    int boardY = (height() - BOARD_PIXEL_H) / 2;
    if (boardY < 50) boardY = 50;

    if (!isOnlineMode) {
        myBoardX = (width() - BOARD_PIXEL_W) / 2;
    } else {
        int gap = 300;
        int totalWidth = BOARD_PIXEL_W * 2 + gap;
        int startX = (width() - totalWidth) / 2;
        myBoardX = startX;
        oppBoardX = startX + BOARD_PIXEL_W + gap;
    }

    // YOU
    painter.setPen(Qt::white);
    QFont titleFont = painter.font();
    titleFont.setBold(true); titleFont.setPointSize(16); painter.setFont(titleFont);
    painter.drawText(myBoardX, boardY - 10, localPlayerName);

    QString stats = QString("SCORE: %1  LEVEL: %2").arg(score).arg(level);
    QFont statFont = painter.font(); statFont.setPointSize(12); painter.setFont(statFont);
    painter.drawText(myBoardX, boardY + BOARD_PIXEL_H + 30, stats);

    drawBoard(painter, myBoardX, boardY, board, true);

    int myHoldX = myBoardX - 90;
    drawQueue(painter, myHoldX, boardY, "HOLD", {heldShape}, canHold);

    int myNextX = myBoardX + BOARD_PIXEL_W + 10;
    drawQueue(painter, myNextX, boardY, "NEXT", nextPieces, true);

    // OPPONENT
    if (isOnlineMode) {
        painter.setPen(Qt::white);
        painter.setFont(titleFont);
        painter.drawText(oppBoardX, boardY - 10, opponentName);

        drawBoard(painter, oppBoardX, boardY, opponentBoard, false);

        int oppHoldX = oppBoardX - 90;
        drawQueue(painter, oppHoldX, boardY, "HOLD", {opponentHold}, true);

        int oppNextX = oppBoardX + BOARD_PIXEL_W + 10;
        drawQueue(painter, oppNextX, boardY, "NEXT", opponentNextPieces.toList(), true);
    }

    if (isPaused) {
        painter.fillRect(rect(), QColor(0, 0, 0, 180));
        painter.setPen(Qt::white);
        QFont f = painter.font(); f.setPointSize(40); painter.setFont(f);
        painter.drawText(rect(), Qt::AlignCenter, "PAUSED");
    }
}

void MainWindow::drawQueue(QPainter &painter, int x, int y, QString label, QList<int> shapes, bool isActive)
{
    painter.setPen(Qt::white);
    QFont f = painter.font(); f.setPointSize(10); f.setBold(true); painter.setFont(f);
    painter.drawText(x, y + 20, label);

    int boxH = (label == "HOLD") ? 80 : 250;
    painter.setPen(QColor(100,100,100));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(x, y + 30, 80, boxH);

    if (!isActive && label == "HOLD") {
        painter.fillRect(x, y + 30, 80, 80, QColor(0,0,0,150));
    }

    int count = (label == "HOLD") ? 1 : qMin(shapes.size(), 3);

    for(int i=0; i<count; i++) {
        int shape = shapes[i];
        if (shape == 0) continue;

        QVector<QPoint> coords = getShapeCoords(shape, 0);
        QColor c = getShapeColor(shape);
        int offsetX = 10;
        if (shape == 1) offsetX = 0;
        if (shape == 5) offsetX = 15;

        for(const QPoint &p : coords) {
            int px = x + offsetX + p.x() * 20;
            int py = y + 50 + i*70 + p.y() * 20;
            painter.fillRect(px, py, 20, 20, c);
            painter.setPen(Qt::black);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(px, py, 20, 20);
        }
    }
}

void MainWindow::drawInstructions(QPainter &painter)
{
    Q_UNUSED(painter);
}

void MainWindow::drawBoard(QPainter &painter, int x, int y, const QVector<int> &targetBoard, bool isPlayer)
{
    painter.setPen(QColor(60, 60, 60));
    painter.setBrush(Qt::black);
    painter.drawRect(x, y, BOARD_PIXEL_W, BOARD_PIXEL_H);

    if (targetBoard.size() != GAME_COLS * GAME_ROWS) return;

    for (int r = 0; r < GAME_ROWS; ++r) {
        for (int c = 0; c < GAME_COLS; ++c) {
            int idx = r * GAME_COLS + c;
            if (idx < 0 || idx >= targetBoard.size()) continue;

            int shapeId = targetBoard[idx];
            if (shapeId > 0) {
                QColor color = getShapeColor(shapeId);
                int px = x + c * CELL_SIZE;
                int py = y + r * CELL_SIZE;
                painter.fillRect(px, py, CELL_SIZE, CELL_SIZE, color);
                painter.setPen(Qt::black);
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(px, py, CELL_SIZE, CELL_SIZE);
            } else {
                painter.setPen(QColor(40, 40, 40));
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(x + c * CELL_SIZE, y + r * CELL_SIZE, CELL_SIZE, CELL_SIZE);
            }
        }
    }

    if (isPlayer && !isPaused && !isGameOver) {
        int ghostY = currentY;
        while (tryMove(currentX, ghostY + 1, currentRotation)) ghostY++;

        QVector<QPoint> coords = getShapeCoords(currentShape, currentRotation);

        painter.setBrush(QColor(255, 255, 255, 40));
        painter.setPen(Qt::NoPen);
        for (const QPoint &p : coords) {
            int gx = currentX + p.x();
            int gy = ghostY + p.y();
            if (gy >= 0) painter.drawRect(x + gx * CELL_SIZE, y + gy * CELL_SIZE, CELL_SIZE, CELL_SIZE);
        }

        QColor curColor = getShapeColor(currentShape);
        painter.setBrush(curColor);
        painter.setPen(Qt::black);
        for (const QPoint &p : coords) {
            int cx = currentX + p.x();
            int cy = currentY + p.y();
            if (cy >= 0) painter.drawRect(x + cx * CELL_SIZE, y + cy * CELL_SIZE, CELL_SIZE, CELL_SIZE);
        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        if (isGameMode && !isOnlineMode && !isGameOver) {
            isPaused = !isPaused;
            if (isPaused) {
                timer->stop();
                bgmPlayer->pause(); // 暫停音樂
            } else {
                timer->start(dropSpeed);
                bgmPlayer->play();
            }
            update(); return;
        } else if (isGameMode) {
            onBackClicked(); return;
        }
    }

    if (!isGameMode || isPaused || isGameOver || isWaitingForOpponent) return;

    switch (event->key()) {
    case Qt::Key_Left:
        if (tryMove(currentX - 1, currentY, currentRotation)) { currentX--; update(); if(isOnlineMode) sendGameState(); }
        break;
    case Qt::Key_Right:
        if (tryMove(currentX + 1, currentY, currentRotation)) { currentX++; update(); if(isOnlineMode) sendGameState(); }
        break;
    case Qt::Key_Down:
        if (tryMove(currentX, currentY + 1, currentRotation)) { currentY++; update(); if(isOnlineMode) sendGameState(); }
        break;
    case Qt::Key_Up:
        rotateWithWallKick();
        break;
    case Qt::Key_Space:
        while(tryMove(currentX, currentY + 1, currentRotation)) { currentY++; }
        placePiece();
        update();
        break;
    case Qt::Key_C:
        holdPiece();
        break;
    }
}

void MainWindow::refillBag() {
    for (int i = 1; i <= 7; ++i) bag.append(i);
    auto rd = std::default_random_engine { static_cast<long unsigned int>(time(0)) };
    std::shuffle(bag.begin(), bag.end(), rd);
}

int MainWindow::getNextPieceFromBag() {
    if (bag.isEmpty()) refillBag();
    return bag.takeFirst();
}

QColor MainWindow::getShapeColor(int shapeId)
{
    switch(shapeId) {
    case 1: return Qt::cyan;
    case 2: return QColor(50, 50, 255);
    case 3: return QColor(255, 165, 0);
    case 4: return Qt::yellow;
    case 5: return Qt::green;
    case 6: return Qt::magenta;
    case 7: return Qt::red;
    case 8: return QColor(100, 100, 100);
    default: return Qt::black;
    }
}

QVector<QPoint> MainWindow::getShapeCoords(int shape, int rotation) {
    if (shape < 1 || shape > 7) return {};

    static const int shapes[8][4][4][2] = {
        { { {0,0}, {0,0}, {0,0}, {0,0} }, { {0,0}, {0,0}, {0,0}, {0,0} }, { {0,0}, {0,0}, {0,0}, {0,0} }, { {0,0}, {0,0}, {0,0}, {0,0} } },
        { { {0,1}, {1,1}, {2,1}, {3,1} }, { {2,0}, {2,1}, {2,2}, {2,3} }, { {0,2}, {1,2}, {2,2}, {3,2} }, { {1,0}, {1,1}, {1,2}, {1,3} } },
        { { {0,0}, {0,1}, {1,1}, {2,1} }, { {1,0}, {2,0}, {1,1}, {1,2} }, { {0,1}, {1,1}, {2,1}, {2,2} }, { {1,0}, {1,1}, {0,2}, {1,2} } },
        { { {2,0}, {0,1}, {1,1}, {2,1} }, { {1,0}, {1,1}, {1,2}, {2,2} }, { {0,1}, {1,1}, {2,1}, {0,2} }, { {0,0}, {1,0}, {1,1}, {1,2} } },
        { { {1,0}, {2,0}, {1,1}, {2,1} }, { {1,0}, {2,0}, {1,1}, {2,1} }, { {1,0}, {2,0}, {1,1}, {2,1} }, { {1,0}, {2,0}, {1,1}, {2,1} } },
        { { {1,0}, {2,0}, {0,1}, {1,1} }, { {1,0}, {1,1}, {2,1}, {2,2} }, { {1,1}, {2,1}, {0,2}, {1,2} }, { {0,0}, {0,1}, {1,1}, {1,2} } },
        { { {1,0}, {0,1}, {1,1}, {2,1} }, { {1,0}, {1,1}, {2,1}, {1,2} }, { {0,1}, {1,1}, {2,1}, {1,2} }, { {1,0}, {0,1}, {1,1}, {1,2} } },
        { { {0,0}, {1,0}, {1,1}, {2,1} }, { {2,0}, {1,1}, {2,1}, {1,2} }, { {0,1}, {1,1}, {1,2}, {2,2} }, { {1,0}, {0,1}, {1,1}, {0,2} } }
    };

    QVector<QPoint> coords;
    for(int i=0; i<4; i++) {
        coords.append(QPoint(shapes[shape][rotation][i][0], shapes[shape][rotation][i][1]));
    }
    return coords;
}
