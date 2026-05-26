#include "main_window.h"
#include "logger.h"
#include "serial_manager.h"
#include "keyword_highlighter.h"
#include <QSerialPort>
#include <QColorDialog>
#include <QHeaderView>
#include <QFontComboBox>
#include <QScreen>
#include <QGuiApplication>
#include <QPixmap>
#include <QIcon>


// 枚举映射表：combo index → Qt 枚举值
static const QSerialPort::DataBits DATA_BITS_MAP[] = {
    QSerialPort::Data5, QSerialPort::Data6, QSerialPort::Data7, QSerialPort::Data8
};
static const QSerialPort::StopBits STOP_BITS_MAP[] = {
    QSerialPort::OneStop, QSerialPort::OneAndHalfStop, QSerialPort::TwoStop
};
static const QSerialPort::Parity PARITY_MAP[] = {
    QSerialPort::NoParity, QSerialPort::EvenParity, QSerialPort::OddParity,
    QSerialPort::MarkParity, QSerialPort::SpaceParity
};
static const QSerialPort::FlowControl FLOW_CTRL_MAP[] = {
    QSerialPort::NoFlowControl, QSerialPort::HardwareControl, QSerialPort::SoftwareControl
};
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollBar>
#include <QStatusBar>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextCodec>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_serialManager1(nullptr)
    , m_serialManager2(nullptr)
    , m_autoSendTimer(new QTimer(this))
    , m_autoSendTarget(-1)
    , m_connected1(false)
    , m_connected2(false)
    , m_paused1(false)
    , m_paused2(false)
    , m_autoScroll1(false)
    , m_autoScroll2(false)
    , m_fontSize(12)
    , m_sendFontSize(12)
    , m_fontFamily("Consolas")
    , m_recvColor("#00FF00")
    , m_sendColor("#FFD700")
    , m_timestampColor("#00CED1")
    , m_bgColor("#000000")
    , m_sendBytes1(0)
    , m_sendBytes2(0)
    , m_recvBytes1(0)
    , m_recvBytes2(0)
    , m_dragging(false)
    , m_dragPos()
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect geo = screen->availableGeometry();
        resize(static_cast<int>(geo.width() * 0.8), static_cast<int>(geo.height() * 0.8));
        setMinimumSize(1200, 800);
    } else {
        setMinimumSize(1500, 950);
    }

    setupTitleBar();
    setupUi();
    setupConnections();
    applyStyleSheet();
    loadConfig();
    applyGlobalFont();
    applyRecvColors();
    applySendColors();

    m_highlighter1 = new KeywordHighlighter(m_recvText1->document());
    m_highlighter2 = new KeywordHighlighter(m_recvText2->document());

    // 初始化定时器
    m_recvMergeTimer1 = new QTimer(this);
    m_recvMergeTimer1->setSingleShot(true);
    connect(m_recvMergeTimer1, &QTimer::timeout, this, [this]() { flushRecvBuffer(1); });

    m_recvMergeTimer2 = new QTimer(this);
    m_recvMergeTimer2->setSingleShot(true);
    connect(m_recvMergeTimer2, &QTimer::timeout, this, [this]() { flushRecvBuffer(2); });

    m_signalTimer = new QTimer(this);
    connect(m_signalTimer, &QTimer::timeout, this, &MainWindow::updateSignalStatus);
    m_signalTimer->start(500);

    m_timeTimer = new QTimer(this);
    connect(m_timeTimer, &QTimer::timeout, this, &MainWindow::updateTimeDisplay);
    m_timeTimer->start(1000);
    updateTimeDisplay();

    // 定时发送定时器
    connect(m_autoSendTimer, &QTimer::timeout, this, [this]() {
        if (m_autoSendTarget >= 0 && !m_autoSendData.isEmpty()) {
            SerialManager *manager = (m_autoSendTarget == 0) ? m_serialManager1 : m_serialManager2;
            if (manager && manager->isOpen()) {
                manager->sendData(m_autoSendData);

                // 更新统计
                if (m_autoSendTarget == 0) {
                    m_sendBytes1 += m_autoSendData.size();
                } else {
                    m_sendBytes2 += m_autoSendData.size();
                }
                m_sendStatsLabel->setText(tr("发送: %1/%2 Bytes").arg(m_sendBytes1).arg(m_sendBytes2));

                // 显示在接收区
                if (m_showSendCheck->isChecked()) {
                    int portNum = m_autoSendTarget + 1;
                    if (m_showTimestampCheck->isChecked()) {
                        appendTimestamp(portNum);
                    }
                    appendToReceive(portNum, "SEND: " + QString::fromUtf8(m_autoSendData), m_sendColor);
                }
            }
        }
    });

    AppLogger::instance().info("应用程序启动");
}

MainWindow::~MainWindow()
{
    saveConfig();
    AppLogger::instance().info("应用程序退出");
}

void MainWindow::setupUi()
{
    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    QVBoxLayout *outerLayout = new QVBoxLayout(central);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(m_titleBar);

    QWidget *contentWidget = new QWidget();
    QHBoxLayout *mainLayout = new QHBoxLayout(contentWidget);
    outerLayout->addWidget(contentWidget, 1);

    // 左侧面板
    QScrollArea *leftScroll = new QScrollArea(this);
    leftScroll->setFixedWidth(350);
    leftScroll->setWidgetResizable(true);
    leftScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget *leftPanel = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setSpacing(6);

    // 系统设置
    QPushButton *sysBtn = new QPushButton(tr("系统设置"));
    sysBtn->setObjectName("fontBtn");
    connect(sysBtn, &QPushButton::clicked, this, &MainWindow::showSettingsDialog);
    leftLayout->addWidget(sysBtn);

    // 串口1设置
    QGroupBox *portGroup1 = new QGroupBox(tr("串口1设置"));
    QFormLayout *portLayout1 = new QFormLayout(portGroup1);
    m_portCombo1 = new QComboBox();
    m_refreshBtn1 = new QPushButton(tr("🔄"));
    m_refreshBtn1->setFixedWidth(40);
    QHBoxLayout *portRow1 = new QHBoxLayout();
    portRow1->addWidget(m_portCombo1);
    portRow1->addWidget(m_refreshBtn1);
    portLayout1->addRow(tr("端口"), portRow1);

    m_baudrateCombo1 = new QComboBox();
    m_baudrateCombo1->addItems({"300", "600", "1200", "2400", "4800", "9600",
                                "19200", "38400", "57600", "115200", "230400", "460800"});
    m_baudrateCombo1->setCurrentText("115200");
    portLayout1->addRow(tr("波特率"), m_baudrateCombo1);

    m_dataBitsCombo1 = new QComboBox();
    m_dataBitsCombo1->addItems({"5", "6", "7", "8"});
    m_dataBitsCombo1->setCurrentText("8");
    portLayout1->addRow(tr("数据位"), m_dataBitsCombo1);

    m_stopBitsCombo1 = new QComboBox();
    m_stopBitsCombo1->addItems({"1", "1.5", "2"});
    portLayout1->addRow(tr("停止位"), m_stopBitsCombo1);

    m_parityCombo1 = new QComboBox();
    m_parityCombo1->addItems({tr("None"), tr("Even"), tr("Odd"), tr("Mark"), tr("Space")});
    portLayout1->addRow(tr("校验位"), m_parityCombo1);

    m_flowCtrlCombo1 = new QComboBox();
    m_flowCtrlCombo1->addItems({tr("None"), "RTS/CTS", "Xon/Xoff"});
    portLayout1->addRow(tr("流控"), m_flowCtrlCombo1);

    m_connectBtn1 = new QPushButton(tr("打开连接"));
    m_connectBtn1->setObjectName("connectBtn1");
    portLayout1->addRow(m_connectBtn1);

    leftLayout->addWidget(portGroup1);

    // 串口2设置
    QGroupBox *portGroup2 = new QGroupBox(tr("串口2设置"));
    QFormLayout *portLayout2 = new QFormLayout(portGroup2);
    m_portCombo2 = new QComboBox();
    m_refreshBtn2 = new QPushButton(tr("🔄"));
    m_refreshBtn2->setFixedWidth(40);
    QHBoxLayout *portRow2 = new QHBoxLayout();
    portRow2->addWidget(m_portCombo2);
    portRow2->addWidget(m_refreshBtn2);
    portLayout2->addRow(tr("端口"), portRow2);

    m_baudrateCombo2 = new QComboBox();
    m_baudrateCombo2->addItems({"300", "600", "1200", "2400", "4800", "9600",
                                "19200", "38400", "57600", "115200", "230400", "460800"});
    m_baudrateCombo2->setCurrentText("115200");
    portLayout2->addRow(tr("波特率"), m_baudrateCombo2);

    m_dataBitsCombo2 = new QComboBox();
    m_dataBitsCombo2->addItems({"5", "6", "7", "8"});
    m_dataBitsCombo2->setCurrentText("8");
    portLayout2->addRow(tr("数据位"), m_dataBitsCombo2);

    m_stopBitsCombo2 = new QComboBox();
    m_stopBitsCombo2->addItems({"1", "1.5", "2"});
    portLayout2->addRow(tr("停止位"), m_stopBitsCombo2);

    m_parityCombo2 = new QComboBox();
    m_parityCombo2->addItems({tr("None"), tr("Even"), tr("Odd"), tr("Mark"), tr("Space")});
    portLayout2->addRow(tr("校验位"), m_parityCombo2);

    m_flowCtrlCombo2 = new QComboBox();
    m_flowCtrlCombo2->addItems({tr("None"), "RTS/CTS", "Xon/Xoff"});
    portLayout2->addRow(tr("流控"), m_flowCtrlCombo2);

    m_connectBtn2 = new QPushButton(tr("打开连接"));
    m_connectBtn2->setObjectName("connectBtn2");
    portLayout2->addRow(m_connectBtn2);

    leftLayout->addWidget(portGroup2);

    // 接收设置
    // 接收设置（可折叠）
    QFrame *recvContent = new QFrame();
    QFormLayout *recvLayout = new QFormLayout(recvContent);
    recvLayout->setContentsMargins(0, 4, 0, 4);

    m_encodingCombo = new QComboBox();
    m_encodingCombo->addItems({"UTF-8", "GBK", "ASCII"});
    recvLayout->addRow(tr("编码"), m_encodingCombo);

    m_bufferSizeSpin = new QSpinBox();
    m_bufferSizeSpin->setRange(64, 65535);
    m_bufferSizeSpin->setValue(4096);
    recvLayout->addRow(tr("缓冲区"), m_bufferSizeSpin);

    m_autoNewlineCheck = new QCheckBox(tr("自动换行"));
    m_autoNewlineCheck->setChecked(true);
    recvLayout->addRow(m_autoNewlineCheck);

    m_showTimestampCheck = new QCheckBox(tr("显示时间戳"));
    m_showTimestampCheck->setChecked(true);
    recvLayout->addRow(m_showTimestampCheck);

    m_hexDisplayCheck = new QCheckBox(tr("十六进制显示"));
    recvLayout->addRow(m_hexDisplayCheck);

    m_showSendCheck = new QCheckBox(tr("显示发送数据"));
    m_showSendCheck->setChecked(true);
    recvLayout->addRow(m_showSendCheck);

    m_autoScrollCheck = new QCheckBox(tr("自动滚动"));
    recvLayout->addRow(m_autoScrollCheck);

    m_saveDirEdit = new QLineEdit();
    m_saveDirEdit->setReadOnly(true);
    m_saveBrowseBtn = new QPushButton(tr("浏览"));
    m_saveBrowseBtn->setFixedWidth(50);
    QHBoxLayout *saveRow = new QHBoxLayout();
    saveRow->addWidget(m_saveDirEdit);
    saveRow->addWidget(m_saveBrowseBtn);
    recvLayout->addRow(tr("保存目录"), saveRow);

    m_saveBtn = new QPushButton(tr("一键保存"));
    m_saveBtn->setObjectName("saveBtn");
    recvLayout->addRow(m_saveBtn);

    m_openDirBtn = new QPushButton(tr("打开文件夹"));
    m_openDirBtn->setObjectName("openDirBtn");
    recvLayout->addRow(m_openDirBtn);

    m_openLogBtn = new QPushButton(tr("打开日志"));
    m_openLogBtn->setObjectName("openLogBtn");
    recvLayout->addRow(m_openLogBtn);

    createCollapseButton(tr("接收设置"), recvContent, false, leftLayout);

    // 发送设置控件（在系统设置对话框中使用）
    m_hexSendCheck = new QCheckBox(tr("HEX发送"));
    m_autoSendCheck = new QCheckBox(tr("定时发送"));
    m_autoSendIntervalSpin = new QSpinBox();
    m_autoSendIntervalSpin->setRange(10, 99999);
    m_autoSendIntervalSpin->setValue(1000);
    m_newlineCombo = new QComboBox();
    m_newlineCombo->addItems({"CRLF", "LF", "CR", tr("无")});

    // 颜色设置控件（在系统设置对话框中使用）
    m_recvColorBtn = new QPushButton();
    m_recvColorBtn->setObjectName("colorBtn");
    m_recvColorBtn->setFixedSize(60, 26);
    m_sendColorBtn = new QPushButton();
    m_sendColorBtn->setObjectName("colorBtn");
    m_sendColorBtn->setFixedSize(60, 26);
    m_timestampColorBtn = new QPushButton();
    m_timestampColorBtn->setObjectName("colorBtn");
    m_timestampColorBtn->setFixedSize(60, 26);
    m_bgColorBtn = new QPushButton();
    m_bgColorBtn->setObjectName("colorBtn");
    m_bgColorBtn->setFixedSize(60, 26);

    // 关键字控件（在系统设置对话框中使用）
    m_keywordEdit = new QLineEdit();
    m_keywordEdit->setPlaceholderText(tr("输入关键字"));
    m_addKeywordBtn = new QPushButton(tr("+"));
    m_addKeywordBtn->setObjectName("toolbarBtn");
    m_addKeywordBtn->setFixedSize(30, 26);
    m_removeKeywordBtn = new QPushButton(tr("-"));
    m_removeKeywordBtn->setObjectName("toolbarBtn");
    m_removeKeywordBtn->setFixedSize(30, 26);
    m_keywordTable = new QTableWidget(0, 2);
    m_keywordTable->setHorizontalHeaderLabels({tr("关键字"), tr("颜色")});
    m_keywordTable->horizontalHeader()->setStretchLastSection(true);
    m_keywordTable->setMaximumHeight(150);
    m_keywordTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_keywordTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    leftLayout->addStretch();

    leftScroll->setWidget(leftPanel);
    mainLayout->addWidget(leftScroll);

    // 中间面板
    QVBoxLayout *centerLayout = new QVBoxLayout();
    mainLayout->addLayout(centerLayout, 1);

    // 同步控制栏（顶部）
    m_syncCheck = new QCheckBox(tr("双串口同步操作"));
    m_syncCheck->setToolTip(tr("开启后，暂停/清空/自动滚动/导出 同时作用于两个串口"));
    m_syncCheck->setObjectName("syncCheck");
    centerLayout->addWidget(m_syncCheck);

    // 接收区分屏
    m_splitter = new QSplitter(Qt::Horizontal);

    // 串口1接收区
    QWidget *recvPanel1 = new QWidget();
    QVBoxLayout *recvLayout1 = new QVBoxLayout(recvPanel1);
    recvLayout1->setContentsMargins(0, 0, 0, 0);

    QLabel *title1 = new QLabel(tr("串口1 接收"));
    title1->setStyleSheet("color: #00CED1; font-size: 14px; font-weight: bold;");
    recvLayout1->addWidget(title1);

    QHBoxLayout *toolbar1 = new QHBoxLayout();
    m_pauseBtn1 = new QPushButton(tr("暂停"));
    m_pauseBtn1->setCheckable(true);
    m_pauseBtn1->setObjectName("toolbarBtn");
    m_clearBtn1 = new QPushButton(tr("清空"));
    m_clearBtn1->setObjectName("toolbarBtn");
    m_scrollBtn1 = new QPushButton(tr("自动滚动"));
    m_scrollBtn1->setCheckable(true);
    m_scrollBtn1->setObjectName("toolbarBtn");
    m_exportBtn1 = new QPushButton(tr("导出"));
    m_exportBtn1->setObjectName("toolbarBtn");
    toolbar1->addWidget(m_pauseBtn1);
    toolbar1->addWidget(m_clearBtn1);
    toolbar1->addWidget(m_scrollBtn1);
    toolbar1->addWidget(m_exportBtn1);
    toolbar1->addStretch();
    recvLayout1->addLayout(toolbar1);

    m_recvText1 = new QTextEdit();
    m_recvText1->setReadOnly(true);
    m_recvText1->setFont(QFont(m_fontFamily, m_fontSize));
    m_recvText1->installEventFilter(this);
    recvLayout1->addWidget(m_recvText1);

    // 串口2接收区
    QWidget *recvPanel2 = new QWidget();
    QVBoxLayout *recvLayout2 = new QVBoxLayout(recvPanel2);
    recvLayout2->setContentsMargins(0, 0, 0, 0);

    QLabel *title2 = new QLabel(tr("串口2 接收"));
    title2->setStyleSheet("color: #00CED1; font-size: 14px; font-weight: bold;");
    recvLayout2->addWidget(title2);

    QHBoxLayout *toolbar2 = new QHBoxLayout();
    m_pauseBtn2 = new QPushButton(tr("暂停"));
    m_pauseBtn2->setCheckable(true);
    m_pauseBtn2->setObjectName("toolbarBtn");
    m_clearBtn2 = new QPushButton(tr("清空"));
    m_clearBtn2->setObjectName("toolbarBtn");
    m_scrollBtn2 = new QPushButton(tr("自动滚动"));
    m_scrollBtn2->setCheckable(true);
    m_scrollBtn2->setObjectName("toolbarBtn");
    m_exportBtn2 = new QPushButton(tr("导出"));
    m_exportBtn2->setObjectName("toolbarBtn");
    toolbar2->addWidget(m_pauseBtn2);
    toolbar2->addWidget(m_clearBtn2);
    toolbar2->addWidget(m_scrollBtn2);
    toolbar2->addWidget(m_exportBtn2);
    toolbar2->addStretch();
    recvLayout2->addLayout(toolbar2);

    m_recvText2 = new QTextEdit();
    m_recvText2->setReadOnly(true);
    m_recvText2->setFont(QFont(m_fontFamily, m_fontSize));
    m_recvText2->installEventFilter(this);
    recvLayout2->addWidget(m_recvText2);

    m_splitter->addWidget(recvPanel1);
    m_splitter->addWidget(recvPanel2);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);

    m_vSplitter = new QSplitter(Qt::Vertical);
    m_vSplitter->addWidget(m_splitter);

    centerLayout->addWidget(m_vSplitter, 1);

    // 发送区
    QGroupBox *sendGroupBox = new QGroupBox(tr("数据发送"));
    QVBoxLayout *sendMainLayout = new QVBoxLayout(sendGroupBox);

    QHBoxLayout *sendTopLayout = new QHBoxLayout();
    m_clearSendBtn = new QPushButton(tr("清空"));
    m_clearSendBtn->setObjectName("toolbarBtn");
    m_historyBtn = new QPushButton(tr("历史记录"));
    m_historyBtn->setObjectName("toolbarBtn");
    m_historyBtn->setCheckable(true);
    m_historyCombo = new QComboBox();
    m_historyCombo->setVisible(false);
    m_historyCombo->setMaximumWidth(300);
    m_sendTarget = new QComboBox();
    m_sendTarget->addItems({tr("串口1"), tr("串口2")});
    sendTopLayout->addWidget(m_clearSendBtn);
    sendTopLayout->addWidget(m_historyBtn);
    sendTopLayout->addWidget(m_historyCombo);
    sendTopLayout->addStretch();
    sendTopLayout->addWidget(new QLabel(tr("发送到:")));
    sendTopLayout->addWidget(m_sendTarget);
    sendMainLayout->addLayout(sendTopLayout);

    QHBoxLayout *sendInputLayout = new QHBoxLayout();
    m_sendInput = new QTextEdit();
    m_sendInput->setMinimumHeight(60);
    m_sendInput->setFont(QFont(m_fontFamily, m_sendFontSize));
    m_sendInput->setPlaceholderText(tr("请输入要发送的内容... (Enter发送 | Shift+Enter换行)"));
    m_sendInput->installEventFilter(this);
    m_sendBtn = new QPushButton(tr("发送"));
    m_sendBtn->setObjectName("sendBtn");
    m_sendBtn->setMinimumHeight(60);
    sendInputLayout->addWidget(m_sendInput);
    sendInputLayout->addWidget(m_sendBtn);
    sendMainLayout->addLayout(sendInputLayout);

    QLabel *sendHint = new QLabel(tr("↑ Enter 发送 | Shift+Enter 换行"));
    sendHint->setStyleSheet("color: #BBBBBB; font-size: 11px;");
    sendMainLayout->addWidget(sendHint);

    m_vSplitter->addWidget(sendGroupBox);
    m_vSplitter->setStretchFactor(0, 3);
    m_vSplitter->setStretchFactor(1, 1);

    // 状态栏
    m_statusLabel1 = new QLabel(tr("串口1: 就绪"));
    m_statusLabel1->setStyleSheet("color: #BBBBBB;");
    m_statusLabel2 = new QLabel(tr("串口2: 就绪"));
    m_statusLabel2->setStyleSheet("color: #BBBBBB;");
    m_sendStatsLabel = new QLabel(tr("发送: 0/0 Bytes"));
    m_recvStatsLabel = new QLabel(tr("接收: 0/0 Bytes"));
    m_clearStatsBtn = new QPushButton(tr("清零统计"));
    m_clearStatsBtn->setObjectName("toolbarBtn");
    m_timeLabel = new QLabel();

    statusBar()->addWidget(m_statusLabel1);
    statusBar()->addWidget(m_statusLabel2);
    statusBar()->addWidget(m_sendStatsLabel);
    statusBar()->addWidget(m_recvStatsLabel);
    statusBar()->addWidget(m_clearStatsBtn);
    statusBar()->addPermanentWidget(m_timeLabel);
}

void MainWindow::setupConnections()
{
    // 串口刷新
    connect(m_refreshBtn1, &QPushButton::clicked, this, &MainWindow::onPortRefresh1);
    connect(m_refreshBtn2, &QPushButton::clicked, this, &MainWindow::onPortRefresh2);

    // 串口连接
    connect(m_connectBtn1, &QPushButton::clicked, this, &MainWindow::onConnect1);
    connect(m_connectBtn2, &QPushButton::clicked, this, &MainWindow::onConnect2);

    // 接收区控件 — 暂停
    connect(m_pauseBtn1, &QPushButton::toggled, this, [this](bool checked) {
        m_paused1 = checked;
        m_pauseBtn1->setText(checked ? tr("继续") : tr("暂停"));
        m_pauseBtn1->setStyleSheet(checked ? "background-color: #DC3545;" : "");
        if (m_syncCheck->isChecked()) {
            m_pauseBtn2->blockSignals(true);
            m_pauseBtn2->setChecked(checked);
            m_pauseBtn2->blockSignals(false);
            m_paused2 = checked;
            m_pauseBtn2->setText(checked ? tr("继续") : tr("暂停"));
            m_pauseBtn2->setStyleSheet(checked ? "background-color: #DC3545;" : "");
        }
    });
    connect(m_pauseBtn2, &QPushButton::toggled, this, [this](bool checked) {
        m_paused2 = checked;
        m_pauseBtn2->setText(checked ? tr("继续") : tr("暂停"));
        m_pauseBtn2->setStyleSheet(checked ? "background-color: #DC3545;" : "");
        if (m_syncCheck->isChecked()) {
            m_pauseBtn1->blockSignals(true);
            m_pauseBtn1->setChecked(checked);
            m_pauseBtn1->blockSignals(false);
            m_paused1 = checked;
            m_pauseBtn1->setText(checked ? tr("继续") : tr("暂停"));
            m_pauseBtn1->setStyleSheet(checked ? "background-color: #DC3545;" : "");
        }
    });

    // 清空
    connect(m_clearBtn1, &QPushButton::clicked, this, [this]() {
        m_recvText1->clear();
        m_recvBuffer1.clear();
        if (m_syncCheck->isChecked()) {
            m_recvText2->clear();
            m_recvBuffer2.clear();
        }
    });
    connect(m_clearBtn2, &QPushButton::clicked, this, [this]() {
        m_recvText2->clear();
        m_recvBuffer2.clear();
        if (m_syncCheck->isChecked()) {
            m_recvText1->clear();
            m_recvBuffer1.clear();
        }
    });

    // 自动滚动
    connect(m_scrollBtn1, &QPushButton::toggled, this, [this](bool checked) {
        m_autoScroll1 = checked;
        if (m_syncCheck->isChecked()) {
            m_scrollBtn2->blockSignals(true);
            m_scrollBtn2->setChecked(checked);
            m_scrollBtn2->blockSignals(false);
            m_autoScroll2 = checked;
        }
    });
    connect(m_scrollBtn2, &QPushButton::toggled, this, [this](bool checked) {
        m_autoScroll2 = checked;
        if (m_syncCheck->isChecked()) {
            m_scrollBtn1->blockSignals(true);
            m_scrollBtn1->setChecked(checked);
            m_scrollBtn1->blockSignals(false);
            m_autoScroll1 = checked;
        }
    });

    connect(m_autoScrollCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_autoScroll1 = checked;
        m_autoScroll2 = checked;
        m_scrollBtn1->blockSignals(true);
        m_scrollBtn2->blockSignals(true);
        m_scrollBtn1->setChecked(checked);
        m_scrollBtn2->setChecked(checked);
        m_scrollBtn1->blockSignals(false);
        m_scrollBtn2->blockSignals(false);
    });

    // 导出
    connect(m_exportBtn1, &QPushButton::clicked, this, [this]() {
        onExportClicked(1);
        if (m_syncCheck->isChecked()) {
            onExportClicked(2);
        }
    });
    connect(m_exportBtn2, &QPushButton::clicked, this, [this]() {
        onExportClicked(2);
        if (m_syncCheck->isChecked()) {
            onExportClicked(1);
        }
    });

    // 发送区
    connect(m_clearSendBtn, &QPushButton::clicked, m_sendInput, &QTextEdit::clear);
    connect(m_historyBtn, &QPushButton::toggled, m_historyCombo, &QComboBox::setVisible);
    connect(m_historyCombo, QOverload<int>::of(&QComboBox::activated), this, &MainWindow::onSendHistoryChanged);
    connect(m_sendBtn, &QPushButton::clicked, this, &MainWindow::onSendClicked);

    // 自动发送
    connect(m_autoSendCheck, &QCheckBox::toggled, this, &MainWindow::onAutoSendToggled);

    // 保存相关
    connect(m_saveBrowseBtn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, tr("选择保存目录"));
        if (!dir.isEmpty()) {
            m_saveDirEdit->setText(dir);
        }
    });
    connect(m_saveBtn, &QPushButton::clicked, this, &MainWindow::onSaveClicked);
    connect(m_openDirBtn, &QPushButton::clicked, this, &MainWindow::onOpenSaveDir);
    connect(m_openLogBtn, &QPushButton::clicked, this, &MainWindow::onOpenLogDir);

    // 状态栏
    connect(m_clearStatsBtn, &QPushButton::clicked, this, &MainWindow::onClearStats);

    // 颜色设置
    connect(m_recvColorBtn, &QPushButton::clicked, this, &MainWindow::onRecvColorClicked);
    connect(m_sendColorBtn, &QPushButton::clicked, this, &MainWindow::onSendColorClicked);
    connect(m_timestampColorBtn, &QPushButton::clicked, this, &MainWindow::onTimestampColorClicked);
    connect(m_bgColorBtn, &QPushButton::clicked, this, &MainWindow::onBgColorClicked);

    // 关键字高亮
    connect(m_addKeywordBtn, &QPushButton::clicked, this, &MainWindow::onAddKeyword);
    connect(m_removeKeywordBtn, &QPushButton::clicked, this, &MainWindow::onRemoveKeyword);
    connect(m_keywordEdit, &QLineEdit::returnPressed, this, &MainWindow::onAddKeyword);

    // 左侧面板控件：未聚焦时忽略滚轮，避免误操作
    for (QComboBox *cb : {m_portCombo1, m_baudrateCombo1, m_dataBitsCombo1, m_stopBitsCombo1,
                          m_parityCombo1, m_flowCtrlCombo1, m_portCombo2, m_baudrateCombo2,
                          m_dataBitsCombo2, m_stopBitsCombo2, m_parityCombo2, m_flowCtrlCombo2,
                          m_encodingCombo, m_newlineCombo, m_historyCombo, m_sendTarget}) {
        cb->installEventFilter(this);
        cb->setFocusPolicy(Qt::ClickFocus);
    }
    for (QSpinBox *sb : {m_bufferSizeSpin, m_autoSendIntervalSpin}) {
        sb->installEventFilter(this);
        sb->setFocusPolicy(Qt::ClickFocus);
    }
}

void MainWindow::applyStyleSheet()
{
    setStyleSheet(R"(
        QMainWindow, QWidget { background-color: #111111; color: #D0D0D0; }
        QLabel, QPushButton, QComboBox, QCheckBox, QSpinBox, QLineEdit, QToolButton { }

        #titleBar {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #0D1B2A, stop:0.5 #1B2838, stop:1 #0D1B2A);
            border-bottom: 1px solid #2A3A4A;
        }
        #titleLabel {
            color: #E0E0E0;
            font-size: 15px;
            font-weight: 600;
            letter-spacing: 2px;
        }
        #titleBtn {
            background: transparent;
            color: #999999;
            border: none;
            font-size: 14px;
            font-weight: bold;
            border-radius: 0;
            padding: 0;
        }
        #titleBtn:hover {
            background-color: #2D2D2D;
            color: #FFFFFF;
        }
        #closeBtn {
            background: transparent;
            color: #999999;
            border: none;
            font-size: 14px;
            font-weight: bold;
            border-radius: 0;
            padding: 0;
        }
        #closeBtn:hover {
            background-color: #E81123;
            color: #FFFFFF;
        }

        #colorBtn {
            border: 1px solid #555555;
            border-radius: 4px;
            padding: 2px;
            min-width: 50px;
        }
        #colorBtn:hover { border: 1px solid #0E639C; }

        QMenuBar { background-color: #111111; color: #D0D0D0; border: none; }
        QMenuBar::item { background: transparent; padding: 4px 8px; }
        QMenuBar::item:selected { background-color: #2D2D2D; }
        QToolBar { background-color: #111111; border: none; }
        QToolTip {
            background-color: #2D2D2D;
            color: #D0D0D0;
            border: 1px solid #444444;
            padding: 4px;
        }

        QScrollArea { background-color: #111111; border: none; }
        QScrollArea > QWidget > QWidget { background-color: #111111; }
        QFrame { background-color: #111111; }
        QSplitter { background-color: #111111; }
        QSplitter > QWidget { background-color: #111111; }

        QGroupBox {
            background-color: #252526;
            color: #CCCCCC;
            border: 1px solid #333333;
            border-radius: 6px;
            padding: 14px 10px 8px 10px;
            font-size: 13px;
            font-weight: bold;
            margin-top: 14px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 6px;
            color: #569CD6;
        }

        QPushButton {
            background-color: #0E639C;
            color: #FFFFFF;
            border: none;
            border-radius: 4px;
            padding: 8px 18px;
            font-size: 13px;
            font-weight: bold;
        }
        QPushButton:hover { background-color: #1177BB; }
        QPushButton:pressed { background-color: #094771; }
        QPushButton:disabled { background-color: #333333; color: #555555; }
        QPushButton:checkable { background-color: #2D5F8A; }
        QPushButton:checked { background-color: #0E639C; }

        #connectBtn1, #connectBtn2 {
            background-color: #0E639C;
            margin-top: 4px;
        }
        #sendBtn {
            background-color: #0E639C;
            font-size: 16px;
            font-weight: bold;
            border-radius: 6px;
            min-width: 70px;
        }
        #sendBtn:hover { background-color: #1177BB; }
        #sendBtn:pressed { background-color: #094771; }
        #saveBtn { background-color: #2D7D46; }
        #saveBtn:hover { background-color: #3A9A58; }
        #openDirBtn, #openLogBtn { background-color: #2D5F8A; }
        #openDirBtn:hover, #openLogBtn:hover { background-color: #3A7AB0; }
        #fontBtn { background-color: #4EC9B0; color: #FFFFFF; }
        #fontBtn:hover { background-color: #6EDDC5; }

        QTextEdit { border: 1px solid #2A2A2A; border-radius: 4px; padding: 8px; }
        QTextEdit:focus { border: 1px solid #0E639C; }

        QComboBox {
            background-color: #333333;
            color: #D0D0D0;
            border: 1px solid #444444;
            border-radius: 4px;
            padding: 5px 8px;
            font-size: 13px;
        }
        QComboBox:hover { border: 1px solid #555555; }
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        QComboBox QAbstractItemView {
            background-color: #2D2D2D;
            color: #D0D0D0;
            border: 1px solid #444444;
            selection-background-color: #094771;
            outline: none;
        }

        QCheckBox { color: #D0D0D0; spacing: 6px; }
        QCheckBox::indicator {
            width: 14px;
            height: 14px;
            background-color: #333333;
            border: 1px solid #555555;
            border-radius: 3px;
        }
        QCheckBox::indicator:checked {
            background-color: #0E639C;
            border: 1px solid #1177BB;
        }
        QCheckBox::indicator:hover { border: 1px solid #0E639C; }

        QLineEdit, QSpinBox {
            background-color: #333333;
            color: #D0D0D0;
            border: 1px solid #444444;
            border-radius: 4px;
            padding: 6px;
        }
        QLineEdit:focus, QSpinBox:focus { border: 1px solid #0E639C; }

        QLabel { color: #D0D0D0; background: transparent; }

        QStatusBar {
            background-color: #007ACC;
            color: #FFFFFF;
            border: none;
            font-size: 12px;
        }
        QStatusBar::item { border: none; }

        QSplitter::handle {
            background-color: #333333;
            width: 2px;
        }
        QSplitter::handle:hover { background-color: #0E639C; }

        QScrollBar:vertical {
            background-color: #111111;
            width: 8px;
            border: none;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background-color: #444444;
            border-radius: 4px;
            min-height: 20px;
        }
        QScrollBar::handle:vertical:hover { background-color: #0E639C; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }

        QScrollBar:horizontal {
            background-color: #111111;
            height: 8px;
            border: none;
        }
        QScrollBar::handle:horizontal {
            background-color: #444444;
            border-radius: 4px;
            min-width: 20px;
        }
        QScrollBar::handle:horizontal:hover { background-color: #0E639C; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }

        #toolbarBtn {
            background-color: #333333;
            color: #CCCCCC;
            border: 1px solid #3C3C3C;
            border-radius: 3px;
            padding: 3px 10px;
            font-size: 12px;
            font-weight: normal;
        }
        #toolbarBtn:hover {
            background-color: #3C3C3C;
            border: 1px solid #0E639C;
            color: #FFFFFF;
        }
        #toolbarBtn:pressed { background-color: #0E639C; }
        #toolbarBtn:checked { background-color: #0E639C; color: #FFFFFF; }

        #syncCheck {
            color: #569CD6;
            font-size: 13px;
            font-weight: bold;
            spacing: 6px;
            padding: 2px 0;
        }
        #syncCheck::indicator {
            width: 14px;
            height: 14px;
            background-color: #333333;
            border: 1px solid #555555;
            border-radius: 3px;
        }
        #syncCheck::indicator:checked {
            background-color: #0E639C;
            border: 1px solid #569CD6;
        }

        #collapseBtn {
            background-color: #2D2D2D;
            color: #AAAAAA;
            border: none;
            border-radius: 4px;
            padding: 6px 10px;
            font-size: 13px;
            font-weight: bold;
            text-align: left;
        }
        #collapseBtn:hover {
            background-color: #333333;
            color: #D0D0D0;
        }
        #collapseBtn:checked {
            background-color: #2D2D2D;
            color: #CCCCCC;
            border-bottom: 1px solid #333333;
            border-bottom-left-radius: 0;
            border-bottom-right-radius: 0;
        }

        QMenu {
            background-color: #2D2D2D;
            color: #D0D0D0;
            border: 1px solid #444444;
            padding: 4px 0;
        }
        QMenu::item {
            padding: 6px 28px 6px 16px;
        }
        QMenu::item:selected {
            background-color: #094771;
            color: #FFFFFF;
        }
        QMenu::item:disabled {
            color: #555555;
        }
        QMenu::separator {
            height: 1px;
            background-color: #444444;
            margin: 4px 8px;
        }
    )");
}

void MainWindow::loadConfig()
{
    QString configPath = QCoreApplication::applicationDirPath() + "/config/config.json";
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        AppLogger::instance().warning("配置文件不存在，使用默认配置");
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        AppLogger::instance().error("配置文件格式错误");
        return;
    }

    QJsonObject config = doc.object();

    // 串口1配置
    m_portCombo1->setCurrentText(config["port_1"].toString());
    m_baudrateCombo1->setCurrentText(config["baudrate_1"].toString());
    m_dataBitsCombo1->setCurrentText(config["data_bits_1"].toString());
    m_stopBitsCombo1->setCurrentText(config["stop_bits_1"].toString());
    m_parityCombo1->setCurrentText(config["parity_1"].toString());
    m_flowCtrlCombo1->setCurrentText(config["flow_control_1"].toString());

    // 串口2配置
    m_portCombo2->setCurrentText(config["port_2"].toString());
    m_baudrateCombo2->setCurrentText(config["baudrate_2"].toString());
    m_dataBitsCombo2->setCurrentText(config["data_bits_2"].toString());
    m_stopBitsCombo2->setCurrentText(config["stop_bits_2"].toString());
    m_parityCombo2->setCurrentText(config["parity_2"].toString());
    m_flowCtrlCombo2->setCurrentText(config["flow_control_2"].toString());

    // 接收设置
    m_encodingCombo->setCurrentText(config["encoding"].toString("UTF-8"));
    m_bufferSizeSpin->setValue(config["buffer_size"].toInt(4096));
    m_autoNewlineCheck->setChecked(config["auto_newline"].toBool(true));
    m_showTimestampCheck->setChecked(config["show_timestamp"].toBool(true));
    m_hexDisplayCheck->setChecked(config["hex_display"].toBool(false));
    m_autoScrollCheck->setChecked(config["auto_scroll"].toBool(false));
    m_syncCheck->setChecked(config["sync_mode"].toBool(false));

    // 发送设置
    m_hexSendCheck->setChecked(config["hex_send"].toBool(false));
    m_autoSendIntervalSpin->setValue(config["auto_send_interval"].toInt(1000));
    m_newlineCombo->setCurrentText(config["newline"].toString("CRLF"));
    m_saveDirEdit->setText(config["save_directory"].toString());
    m_showSendCheck->setChecked(config["show_send"].toBool(true));

    // 发送历史
    if (config.contains("send_history")) {
        QJsonArray historyArray = config["send_history"].toArray();
        for (const QJsonValue &v : historyArray) {
            m_sendHistory.append(v.toString());
        }
        m_historyCombo->addItems(m_sendHistory);
    }

    // 窗口设置
    if (config.contains("window_geometry")) {
        QJsonArray geo = config["window_geometry"].toArray();
        if (geo.size() >= 2) {
            resize(geo[0].toInt(1500), geo[1].toInt(950));
        }
    }
    m_fontSize = config["font_size"].toInt(12);
    m_sendFontSize = config["send_font_size"].toInt(m_fontSize);
    m_fontFamily = config["font_family"].toString("Consolas");
    QFont applyFont(m_fontFamily, m_fontSize);
    m_recvText1->setFont(applyFont);
    m_recvText2->setFont(applyFont);
    m_sendInput->setFont(applyFont);

    // 颜色设置
    if (config.contains("recv_color")) m_recvColor.setNamedColor(config["recv_color"].toString());
    if (config.contains("send_color")) m_sendColor.setNamedColor(config["send_color"].toString());
    if (config.contains("timestamp_color")) m_timestampColor.setNamedColor(config["timestamp_color"].toString());
    if (config.contains("bg_color")) m_bgColor.setNamedColor(config["bg_color"].toString());

    // 关键字高亮
    if (config.contains("keywords")) {
        QJsonArray kwArray = config["keywords"].toArray();
        for (const QJsonValue &v : kwArray) {
            QJsonObject kw = v.toObject();
            QString word = kw["word"].toString();
            QColor color;
            color.setNamedColor(kw["color"].toString());
            if (!word.isEmpty() && color.isValid()) {
                m_highlighter1->addKeyword(word, color);
                m_highlighter2->addKeyword(word, color);
                int row = m_keywordTable->rowCount();
                m_keywordTable->insertRow(row);
                m_keywordTable->setItem(row, 0, new QTableWidgetItem(word));
                QTableWidgetItem *colorItem = new QTableWidgetItem(color.name());
                colorItem->setBackground(color);
                colorItem->setForeground(color.lightness() > 128 ? Qt::black : Qt::white);
                m_keywordTable->setItem(row, 1, colorItem);
            }
        }
    }

    AppLogger::instance().info("配置加载完成");
}

void MainWindow::saveConfig()
{
    QJsonObject config;

    // 串口1配置
    config["port_1"] = m_portCombo1->currentText();
    config["baudrate_1"] = m_baudrateCombo1->currentText();
    config["data_bits_1"] = m_dataBitsCombo1->currentText();
    config["stop_bits_1"] = m_stopBitsCombo1->currentText();
    config["parity_1"] = m_parityCombo1->currentText();
    config["flow_control_1"] = m_flowCtrlCombo1->currentText();

    // 串口2配置
    config["port_2"] = m_portCombo2->currentText();
    config["baudrate_2"] = m_baudrateCombo2->currentText();
    config["data_bits_2"] = m_dataBitsCombo2->currentText();
    config["stop_bits_2"] = m_stopBitsCombo2->currentText();
    config["parity_2"] = m_parityCombo2->currentText();
    config["flow_control_2"] = m_flowCtrlCombo2->currentText();

    // 接收设置
    config["encoding"] = m_encodingCombo->currentText();
    config["buffer_size"] = m_bufferSizeSpin->value();
    config["auto_newline"] = m_autoNewlineCheck->isChecked();
    config["show_timestamp"] = m_showTimestampCheck->isChecked();
    config["hex_display"] = m_hexDisplayCheck->isChecked();
    config["auto_scroll"] = m_autoScrollCheck->isChecked();
    config["sync_mode"] = m_syncCheck->isChecked();

    // 发送设置
    config["hex_send"] = m_hexSendCheck->isChecked();
    config["auto_send_interval"] = m_autoSendIntervalSpin->value();
    config["newline"] = m_newlineCombo->currentText();
    config["save_directory"] = m_saveDirEdit->text();
    config["show_send"] = m_showSendCheck->isChecked();

    // 发送历史
    QJsonArray historyArray;
    for (const QString &h : m_sendHistory) {
        historyArray.append(h);
    }
    config["send_history"] = historyArray;

    // 窗口设置
    QJsonArray geo;
    geo.append(width());
    geo.append(height());
    config["window_geometry"] = geo;
    config["font_size"] = m_fontSize;
    config["send_font_size"] = m_sendFontSize;
    config["font_family"] = m_fontFamily;

    // 颜色设置
    config["recv_color"] = m_recvColor.name();
    config["send_color"] = m_sendColor.name();
    config["timestamp_color"] = m_timestampColor.name();
    config["bg_color"] = m_bgColor.name();

    // 关键字高亮
    QJsonArray kwArray;
    for (const auto &pair : m_highlighter1->keywords()) {
        QJsonObject kw;
        kw["word"] = pair.first;
        kw["color"] = pair.second.name();
        kwArray.append(kw);
    }
    config["keywords"] = kwArray;

    // 写入文件
    QString configDir = QCoreApplication::applicationDirPath() + "/config";
    QDir().mkpath(configDir);
    QString configPath = configDir + "/config.json";

    QFile file(configPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(config).toJson());
        file.close();
        AppLogger::instance().info("配置保存完成");
    } else {
        AppLogger::instance().error("无法保存配置文件");
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // Ctrl+滚轮缩放
    if (event->type() == QEvent::Wheel) {
        QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
        if (wheelEvent->modifiers() & Qt::ControlModifier) {
            if (obj == m_recvText1 || obj == m_recvText2) {
                int delta = wheelEvent->angleDelta().y();
                m_fontSize += (delta > 0) ? 1 : -1;
                m_fontSize = qBound(6, m_fontSize, 36);
                QFont f(m_fontFamily, m_fontSize);
                m_recvText1->setFont(f);
                m_recvText2->setFont(f);
                return true;
            }
            if (obj == m_sendInput) {
                int delta = wheelEvent->angleDelta().y();
                m_sendFontSize += (delta > 0) ? 1 : -1;
                m_sendFontSize = qBound(6, m_sendFontSize, 36);
                m_sendInput->setFont(QFont(m_fontFamily, m_sendFontSize));
                return true;
            }
        }

        // ComboBox/SpinBox: 始终屏蔽滚轮，必须点击才能修改
        QWidget *w = qobject_cast<QWidget*>(obj);
        if (w && (qobject_cast<QComboBox*>(w) || qobject_cast<QSpinBox*>(w))) {
            return true;
        }
    }

    // Enter/Shift+Enter 发送
    if (obj == m_sendInput && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (keyEvent->modifiers() & Qt::ShiftModifier) {
                // Shift+Enter: 换行
                return false;
            } else {
                // Enter: 发送
                onSendClicked();
                return true;
            }
        }
    }

    // 标题栏双击最大化/还原
    if (obj == m_titleBar && event->type() == QEvent::MouseButtonDblClick) {
        toggleMaximize();
        return true;
    }

    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_titleBar->underMouse()) {
        m_dragging = true;
        m_dragPos = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPos() - m_dragPos);
        event->accept();
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    m_dragging = false;
}

void MainWindow::onPortRefresh1()
{
    m_portCombo1->clear();
    m_portCombo1->addItems(SerialManager::availablePorts());
}

void MainWindow::onPortRefresh2()
{
    m_portCombo2->clear();
    m_portCombo2->addItems(SerialManager::availablePorts());
}

void MainWindow::onConnect1()
{
    if (m_connected1) {
        if (m_serialManager1) {
            m_serialManager1->closePort();
            delete m_serialManager1;
            m_serialManager1 = nullptr;
        }
        m_connected1 = false;
        updateConnectButton(1, false);
        m_statusLabel1->setText(tr("串口1: 已断开"));
        m_statusLabel1->setStyleSheet("color: #FF8800;");
        AppLogger::instance().info("串口1已断开");
    } else {
        connectToPort(1);
    }
}

void MainWindow::onConnect2()
{
    if (m_connected2) {
        if (m_serialManager2) {
            m_serialManager2->closePort();
            delete m_serialManager2;
            m_serialManager2 = nullptr;
        }
        m_connected2 = false;
        updateConnectButton(2, false);
        m_statusLabel2->setText(tr("串口2: 已断开"));
        m_statusLabel2->setStyleSheet("color: #FF8800;");
        AppLogger::instance().info("串口2已断开");
    } else {
        connectToPort(2);
    }
}

void MainWindow::onDataReceived(const QByteArray &data, int portNum)
{
    if (portNum == 1) {
        if (m_paused1) return;
        m_recvBuffer1.append(data);
        m_recvBytes1 += data.size();
        if (!m_recvMergeTimer1->isActive()) {
            m_recvMergeTimer1->start(30);
        }
    } else {
        if (m_paused2) return;
        m_recvBuffer2.append(data);
        m_recvBytes2 += data.size();
        if (!m_recvMergeTimer2->isActive()) {
            m_recvMergeTimer2->start(30);
        }
    }
    m_recvStatsLabel->setText(tr("接收: %1/%2 Bytes").arg(m_recvBytes1).arg(m_recvBytes2));
}

void MainWindow::flushRecvBuffer(int portNum)
{
    QByteArray buffer;
    QTextEdit *textEdit;

    if (portNum == 1) {
        buffer = m_recvBuffer1;
        m_recvBuffer1.clear();
        textEdit = m_recvText1;
    } else {
        buffer = m_recvBuffer2;
        m_recvBuffer2.clear();
        textEdit = m_recvText2;
    }

    if (buffer.isEmpty()) return;

    QString text;
    if (m_hexDisplayCheck->isChecked()) {
        text = buffer.toHex(' ').toUpper();
    } else {
        QString encoding = m_encodingCombo->currentText();
        QTextCodec *codec = QTextCodec::codecForName(encoding.toUtf8());
        if (codec) {
            text = codec->toUnicode(buffer);
        } else {
            text = QString::fromUtf8(buffer);
        }
    }

    // 分割行
    QStringList lines = splitLogLines(text);

    foreach (const QString &line, lines) {
        if (m_showTimestampCheck->isChecked()) {
            appendTimestamp(portNum);
        }
        appendToReceive(portNum, line, m_recvColor);
    }

    // 行数限制
    QTextDocument *doc = textEdit->document();
    if (doc->blockCount() > 5000) {
        QTextCursor cursor(doc);
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 2500);
        cursor.removeSelectedText();
    }

    // 自动滚动
    if ((portNum == 1 && m_autoScroll1) || (portNum == 2 && m_autoScroll2)) {
        QScrollBar *scrollBar = textEdit->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    }
}

void MainWindow::onSendClicked()
{
    QString text = m_sendInput->toPlainText().trimmed();
    if (text.isEmpty()) return;

    QByteArray data;
    if (m_hexSendCheck->isChecked()) {
        data = parseHexString(text);
        if (data.isEmpty()) {
            showError(tr("HEX格式错误"));
            return;
        }
    } else {
        QString encoding = m_encodingCombo->currentText();
        QTextCodec *codec = QTextCodec::codecForName(encoding.toUtf8());
        if (codec) {
            data = codec->fromUnicode(text);
        } else {
            data = text.toUtf8();
        }
    }

    // 添加换行符
    QString newline = m_newlineCombo->currentText();
    if (newline == "CRLF") {
        data.append("\r\n");
    } else if (newline == "LF") {
        data.append("\n");
    } else if (newline == "CR") {
        data.append("\r");
    }

    // 发送到目标串口
    int target = m_sendTarget->currentIndex();
    SerialManager *manager = (target == 0) ? m_serialManager1 : m_serialManager2;

    if (!manager || !manager->isOpen()) {
        showError(tr("串口未打开"));
        return;
    }

    if (manager->sendData(data)) {
        // 更新统计
        if (target == 0) {
            m_sendBytes1 += data.size();
        } else {
            m_sendBytes2 += data.size();
        }
        m_sendStatsLabel->setText(tr("发送: %1/%2 Bytes").arg(m_sendBytes1).arg(m_sendBytes2));

        // 显示在接收区（金色）
        if (m_showSendCheck->isChecked()) {
            int portNum = target + 1;
            if (m_showTimestampCheck->isChecked()) {
                appendTimestamp(portNum);
            }
            appendToReceive(portNum, "SEND: " + text, m_sendColor);
        }

        // 记录历史
        if (!m_sendHistory.contains(text)) {
            m_sendHistory.prepend(text);
            if (m_sendHistory.size() > 15) {
                m_sendHistory.removeLast();
            }
            m_historyCombo->clear();
            m_historyCombo->addItems(m_sendHistory);
        }
    }
}

void MainWindow::onSendHistoryChanged(int index)
{
    if (index >= 0 && index < m_sendHistory.size()) {
        m_sendInput->setText(m_sendHistory.at(index));
        m_historyCombo->setVisible(false);
        m_historyBtn->setChecked(false);
    }
}

void MainWindow::onAutoSendToggled(bool checked)
{
    int target = m_sendTarget->currentIndex();
    SerialManager *manager = (target == 0) ? m_serialManager1 : m_serialManager2;

    if (!manager || !manager->isOpen()) {
        m_autoSendCheck->setChecked(false);
        showError(tr("串口未打开"));
        return;
    }

    if (checked) {
        QString text = m_sendInput->toPlainText().trimmed();
        if (text.isEmpty()) {
            m_autoSendCheck->setChecked(false);
            showError(tr("请输入发送内容"));
            return;
        }

        QByteArray data;
        if (m_hexSendCheck->isChecked()) {
            data = parseHexString(text);
            if (data.isEmpty()) {
                m_autoSendCheck->setChecked(false);
                showError(tr("HEX格式错误"));
                return;
            }
        } else {
            QString encoding = m_encodingCombo->currentText();
            QTextCodec *codec = QTextCodec::codecForName(encoding.toUtf8());
            if (codec) {
                data = codec->fromUnicode(text);
            } else {
                data = text.toUtf8();
            }
        }

        // 添加换行符
        QString newline = m_newlineCombo->currentText();
        if (newline == "CRLF") {
            data.append("\r\n");
        } else if (newline == "LF") {
            data.append("\n");
        } else if (newline == "CR") {
            data.append("\r");
        }

        m_autoSendData = data;
        m_autoSendTarget = target;
        int interval = m_autoSendIntervalSpin->value();
        m_autoSendTimer->start(interval);
        AppLogger::instance().info(tr("定时发送已启动，间隔: %1ms").arg(interval));
    } else {
        m_autoSendTimer->stop();
        m_autoSendData.clear();
        m_autoSendTarget = -1;
        AppLogger::instance().info("定时发送已停止");
    }

    m_autoSendIntervalSpin->setEnabled(!checked);
    m_sendTarget->setEnabled(!checked);
    m_hexSendCheck->setEnabled(!checked);
    m_newlineCombo->setEnabled(!checked);
}

void MainWindow::onSaveClicked()
{
    QString saveDir = m_saveDirEdit->text();
    if (saveDir.isEmpty()) {
        saveDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    }

    QString fileName = saveDir + "/串口数据_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".txt";
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly)) {
        file.write("\xEF\xBB\xBF");
        QTextStream out(&file);
        out.setCodec("UTF-8");
        out << "=== 串口1 接收数据 ===" << endl;
        out << m_recvText1->toPlainText() << endl;
        out << "=== 串口2 接收数据 ===" << endl;
        out << m_recvText2->toPlainText() << endl;
        out.flush();
        file.close();
        AppLogger::instance().info("一键保存到: " + fileName);
        QMessageBox::information(this, tr("保存成功"), tr("数据已保存到: %1").arg(fileName));
    } else {
        showError(tr("无法保存文件"));
    }
}

void MainWindow::onExportClicked(int portNum)
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("导出数据"),
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
        tr("文本文件 (*.txt);;所有文件 (*.*)"));

    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly)) {
        // UTF-8 BOM
        file.write("\xEF\xBB\xBF");
        QTextStream out(&file);
        out.setCodec("UTF-8");
        if (portNum == 1) {
            out << m_recvText1->toPlainText();
        } else {
            out << m_recvText2->toPlainText();
        }
        out.flush();
        file.close();
        AppLogger::instance().info("导出数据到: " + fileName);
    }
}

void MainWindow::onOpenSaveDir()
{
    QString dir = m_saveDirEdit->text();
    if (dir.isEmpty()) {
        dir = QCoreApplication::applicationDirPath();
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void MainWindow::onOpenLogDir()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(AppLogger::instance().logDir()));
}

void MainWindow::onClearStats()
{
    m_sendBytes1 = 0;
    m_sendBytes2 = 0;
    m_recvBytes1 = 0;
    m_recvBytes2 = 0;
    m_sendStatsLabel->setText(tr("发送: 0/0 Bytes"));
    m_recvStatsLabel->setText(tr("接收: 0/0 Bytes"));
}

void MainWindow::updateSignalStatus()
{
    if (m_connected1 && m_serialManager1) {
        QSerialPort::PinoutSignals pinSignals = m_serialManager1->pinoutSignals();
        QStringList sigs;
        if (pinSignals & QSerialPort::DataTerminalReadySignal) sigs << "DTR";
        if (pinSignals & QSerialPort::RequestToSendSignal) sigs << "RTS";
        if (pinSignals & QSerialPort::ClearToSendSignal) sigs << "CTS";
        if (pinSignals & QSerialPort::DataSetReadySignal) sigs << "DSR";
        if (pinSignals & QSerialPort::DataCarrierDetectSignal) sigs << "DCD";
        if (pinSignals & QSerialPort::RingIndicatorSignal) sigs << "RI";
        QString portName = m_serialManager1->portName();
        m_statusLabel1->setText(tr("串口1: %1 [%2]").arg(portName, sigs.join(" ")));
    }

    if (m_connected2 && m_serialManager2) {
        QSerialPort::PinoutSignals pinSignals = m_serialManager2->pinoutSignals();
        QStringList sigs;
        if (pinSignals & QSerialPort::DataTerminalReadySignal) sigs << "DTR";
        if (pinSignals & QSerialPort::RequestToSendSignal) sigs << "RTS";
        if (pinSignals & QSerialPort::ClearToSendSignal) sigs << "CTS";
        if (pinSignals & QSerialPort::DataSetReadySignal) sigs << "DSR";
        if (pinSignals & QSerialPort::DataCarrierDetectSignal) sigs << "DCD";
        if (pinSignals & QSerialPort::RingIndicatorSignal) sigs << "RI";
        QString portName = m_serialManager2->portName();
        m_statusLabel2->setText(tr("串口2: %1 [%2]").arg(portName, sigs.join(" ")));
    }
}

void MainWindow::updateTimeDisplay()
{
    m_timeLabel->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
}

void MainWindow::onConnectionLost(int portNum)
{
    if (portNum == 1) {
        if (m_serialManager1) {
            m_serialManager1->closePort();
            delete m_serialManager1;
            m_serialManager1 = nullptr;
        }
        m_connected1 = false;
        m_statusLabel1->setText(tr("串口1: 连接丢失"));
        m_statusLabel1->setStyleSheet("color: #FF8800;");
        m_connectBtn1->setText(tr("打开连接"));
        m_connectBtn1->setStyleSheet("");
    } else {
        if (m_serialManager2) {
            m_serialManager2->closePort();
            delete m_serialManager2;
            m_serialManager2 = nullptr;
        }
        m_connected2 = false;
        m_statusLabel2->setText(tr("串口2: 连接丢失"));
        m_statusLabel2->setStyleSheet("color: #FF8800;");
        m_connectBtn2->setText(tr("打开连接"));
        m_connectBtn2->setStyleSheet("");
    }
    AppLogger::instance().warning(tr("串口%1 连接丢失").arg(portNum));
}

void MainWindow::applyGlobalFont()
{
    QFont recvFont(m_fontFamily, m_fontSize);
    QFont sendFont(m_fontFamily, m_sendFontSize);
    setFont(recvFont);
    m_recvText1->setFont(recvFont);
    m_recvText2->setFont(recvFont);
    m_sendInput->setFont(sendFont);
    QFont titleFont("Microsoft YaHei UI", 14, QFont::DemiBold);
    m_titleLabel->setFont(titleFont);
}

void MainWindow::setupTitleBar()
{
    m_titleBar = new QWidget(this);
    m_titleBar->setObjectName("titleBar");
    m_titleBar->setFixedHeight(42);
    m_titleBar->installEventFilter(this);

    QHBoxLayout *layout = new QHBoxLayout(m_titleBar);
    layout->setContentsMargins(12, 0, 8, 0);
    layout->setSpacing(4);

    QPixmap logoPixmap(":/logo.png");
    if (!logoPixmap.isNull()) {
        setWindowIcon(QIcon(logoPixmap));
        QLabel *logoLabel = new QLabel();
        logoLabel->setPixmap(logoPixmap.scaled(22, 22, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        logoLabel->setFixedSize(26, 26);
        layout->addWidget(logoLabel);
    }

    m_titleLabel = new QLabel(tr("  加权平均数的串口调试助手 MAX"));
    m_titleLabel->setObjectName("titleLabel");
    layout->addWidget(m_titleLabel);
    layout->addStretch();

    m_minBtn = new QPushButton("─");
    m_minBtn->setObjectName("titleBtn");
    m_minBtn->setFixedSize(44, 32);
    connect(m_minBtn, &QPushButton::clicked, this, &MainWindow::minimizeWindow);
    layout->addWidget(m_minBtn);

    m_maxBtn = new QPushButton("□");
    m_maxBtn->setObjectName("titleBtn");
    m_maxBtn->setFixedSize(44, 32);
    connect(m_maxBtn, &QPushButton::clicked, this, &MainWindow::toggleMaximize);
    layout->addWidget(m_maxBtn);

    m_closeBtn = new QPushButton("✕");
    m_closeBtn->setObjectName("closeBtn");
    m_closeBtn->setFixedSize(44, 32);
    connect(m_closeBtn, &QPushButton::clicked, this, &QWidget::close);
    layout->addWidget(m_closeBtn);
}

void MainWindow::minimizeWindow()
{
    showMinimized();
}

void MainWindow::toggleMaximize()
{
    if (isMaximized()) {
        showNormal();
        m_maxBtn->setText("□");
    } else {
        showMaximized();
        m_maxBtn->setText("❐");
    }
}

void MainWindow::connectToPort(int portNum)
{
    QComboBox *portCombo = (portNum == 1) ? m_portCombo1 : m_portCombo2;
    QComboBox *baudCombo = (portNum == 1) ? m_baudrateCombo1 : m_baudrateCombo2;
    QComboBox *dataBitsCombo = (portNum == 1) ? m_dataBitsCombo1 : m_dataBitsCombo2;
    QComboBox *stopBitsCombo = (portNum == 1) ? m_stopBitsCombo1 : m_stopBitsCombo2;
    QComboBox *parityCombo = (portNum == 1) ? m_parityCombo1 : m_parityCombo2;
    QComboBox *flowCombo = (portNum == 1) ? m_flowCtrlCombo1 : m_flowCtrlCombo2;
    SerialManager *&manager = (portNum == 1) ? m_serialManager1 : m_serialManager2;
    bool &connected = (portNum == 1) ? m_connected1 : m_connected2;
    QLabel *statusLabel = (portNum == 1) ? m_statusLabel1 : m_statusLabel2;

    QString portText = portCombo->currentText();
    if (portText.isEmpty()) {
        showError(tr("请选择串口"));
        return;
    }

    QString portName = portText.split(" - ").first().trimmed();

    SerialManager::PortConfig config;
    config.portName = portName;
    config.baudRate = baudCombo->currentText().toInt();
    config.dataBits = DATA_BITS_MAP[dataBitsCombo->currentIndex()];
    config.stopBits = STOP_BITS_MAP[stopBitsCombo->currentIndex()];
    config.parity = PARITY_MAP[parityCombo->currentIndex()];
    config.flowControl = FLOW_CTRL_MAP[flowCombo->currentIndex()];

    manager = new SerialManager(this);
    if (!manager->openPort(config)) {
        delete manager;
        manager = nullptr;
        statusLabel->setText(tr("串口%1: 连接失败").arg(portNum));
        statusLabel->setStyleSheet("color: #FF0000;");
        return;
    }

    connect(manager, &SerialManager::dataReceived, this, [this, portNum](const QByteArray &data) {
        onDataReceived(data, portNum);
    });
    connect(manager, &SerialManager::errorOccurred, this, &MainWindow::showError);
    connect(manager, &SerialManager::connectionLost, this, [this, portNum]() {
        onConnectionLost(portNum);
    });

    connected = true;
    updateConnectButton(portNum, true);
    statusLabel->setText(tr("串口%1: %2").arg(portNum).arg(portName));
    statusLabel->setStyleSheet("color: #00FF00;");
    AppLogger::instance().info(tr("串口%1已连接: %2").arg(portNum).arg(portName));
}

void MainWindow::applyRecvColors()
{
    QString style = QString(
        "background-color: %1; color: %2; border: 1px solid #2A2A2A; border-radius: 4px; padding: 8px;"
        "selection-background-color: #264F78;"
    ).arg(m_bgColor.name(), m_recvColor.name());
    m_recvText1->setStyleSheet(style);
    m_recvText2->setStyleSheet(style);
    m_recvColorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid #555;").arg(m_recvColor.name()));
    m_sendColorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid #555;").arg(m_sendColor.name()));
    m_timestampColorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid #555;").arg(m_timestampColor.name()));
    m_bgColorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid #555;").arg(m_bgColor.name()));
}

void MainWindow::applySendColors()
{
    m_sendInput->setStyleSheet(QString(
        "background-color: #1E1E1E; color: %1; border: 1px solid #2A2A2A; border-radius: 4px; padding: 8px;"
        "selection-background-color: #264F78;"
    ).arg(m_sendColor.name()));
}

void MainWindow::onRecvColorClicked()
{
    QColor color = QColorDialog::getColor(m_recvColor, this, tr("选择接收文字颜色"));
    if (color.isValid()) {
        m_recvColor = color;
        applyRecvColors();
        saveConfig();
    }
}

void MainWindow::onSendColorClicked()
{
    QColor color = QColorDialog::getColor(m_sendColor, this, tr("选择发送文字颜色"));
    if (color.isValid()) {
        m_sendColor = color;
        applySendColors();
        saveConfig();
    }
}

void MainWindow::onTimestampColorClicked()
{
    QColor color = QColorDialog::getColor(m_timestampColor, this, tr("选择时间戳颜色"));
    if (color.isValid()) {
        m_timestampColor = color;
        applyRecvColors();
        saveConfig();
    }
}

void MainWindow::onBgColorClicked()
{
    QColor color = QColorDialog::getColor(m_bgColor, this, tr("选择接收区背景颜色"));
    if (color.isValid()) {
        m_bgColor = color;
        applyRecvColors();
        saveConfig();
    }
}

void MainWindow::onAddKeyword()
{
    QString keyword = m_keywordEdit->text().trimmed();
    if (keyword.isEmpty()) return;

    QColor color = QColorDialog::getColor(Qt::yellow, this, tr("选择关键字 \"%1\" 的颜色").arg(keyword));
    if (!color.isValid()) return;

    m_highlighter1->addKeyword(keyword, color);
    m_highlighter2->addKeyword(keyword, color);

    int row = m_keywordTable->rowCount();
    m_keywordTable->insertRow(row);
    m_keywordTable->setItem(row, 0, new QTableWidgetItem(keyword));
    QTableWidgetItem *colorItem = new QTableWidgetItem(color.name());
    colorItem->setBackground(color);
    colorItem->setForeground(color.lightness() > 128 ? Qt::black : Qt::white);
    m_keywordTable->setItem(row, 1, colorItem);

    m_keywordEdit->clear();
    saveConfig();
}

void MainWindow::onRemoveKeyword()
{
    int row = m_keywordTable->currentRow();
    if (row < 0) return;

    QString keyword = m_keywordTable->item(row, 0)->text();
    m_highlighter1->removeKeyword(keyword);
    m_highlighter2->removeKeyword(keyword);
    m_keywordTable->removeRow(row);
    saveConfig();
}

void MainWindow::showError(const QString &msg)
{
    statusBar()->showMessage(msg, 5000);
    statusBar()->setStyleSheet("QStatusBar { background-color: #3C1515; color: #FF6B6B; border-top: 1px solid #5C2020; }");
    QTimer::singleShot(5000, this, [this]() {
        statusBar()->setStyleSheet("");
    });
    AppLogger::instance().warning(msg);
}

void MainWindow::appendToReceive(int portNum, const QString &text, const QColor &color)
{
    QTextEdit *textEdit = (portNum == 1) ? m_recvText1 : m_recvText2;
    QTextCursor cursor = textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat format;
    format.setForeground(color);
    cursor.setCharFormat(format);
    cursor.insertText(text + "\n");
}

void MainWindow::appendTimestamp(int portNum)
{
    QTextEdit *textEdit = (portNum == 1) ? m_recvText1 : m_recvText2;
    QTextCursor cursor = textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat format;
    format.setForeground(m_timestampColor);
    cursor.setCharFormat(format);
    cursor.insertText("[" + QTime::currentTime().toString("HH:mm:ss.zzz") + "] ");
}

QStringList MainWindow::splitLogLines(const QString &text)
{
    QStringList lines;
    QString current;

    for (int i = 0; i < text.length(); ++i) {
        QChar ch = text[i];
        current.append(ch);

        // 检测 } 后的 [ 或时间戳格式
        if (ch == '}' && i + 1 < text.length() && text[i + 1] == '[') {
            lines.append(current);
            current.clear();
        }
        // 检测换行符
        else if (ch == '\n') {
            lines.append(current);
            current.clear();
        }
    }

    if (!current.isEmpty()) {
        lines.append(current);
    }

    return lines;
}

QByteArray MainWindow::parseHexString(const QString &hex)
{
    QString cleanHex = hex.simplified().remove(' ');
    if (cleanHex.length() % 2 != 0) {
        return QByteArray();
    }

    QByteArray result;
    for (int i = 0; i < cleanHex.length(); i += 2) {
        bool ok;
        quint8 byte = cleanHex.mid(i, 2).toUInt(&ok, 16);
        if (!ok) {
            return QByteArray();
        }
        result.append(byte);
    }
    return result;
}

void MainWindow::updateConnectButton(int portNum, bool connected)
{
    QPushButton *btn = (portNum == 1) ? m_connectBtn1 : m_connectBtn2;
    QLabel *statusLabel = (portNum == 1) ? m_statusLabel1 : m_statusLabel2;

    if (connected) {
        btn->setText(tr("断开连接"));
        btn->setStyleSheet("background-color: #DC3545;");
        statusLabel->setStyleSheet("color: #00FF00;");
    } else {
        btn->setText(tr("打开连接"));
        btn->setStyleSheet("");
        statusLabel->setStyleSheet("color: #BBBBBB;");
    }
}

void MainWindow::showSettingsDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("系统设置"));
    dialog.setMinimumSize(750, 550);
    dialog.setStyleSheet(R"(
        QDialog { background-color: #1E1E1E; color: #D0D0D0; }
        QGroupBox {
            background-color: #252526; color: #CCCCCC;
            border: 1px solid #333333; border-radius: 6px;
            padding: 14px 10px 8px 10px; font-size: 13px; font-weight: bold; margin-top: 14px;
        }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 6px; color: #569CD6; }
        QPushButton {
            background-color: #0E639C; color: #FFFFFF; border: none; border-radius: 4px;
            padding: 8px 18px; font-size: 13px; font-weight: bold;
        }
        QPushButton:hover { background-color: #1177BB; }
        QPushButton:pressed { background-color: #094771; }
        #fontBtn { background-color: #4EC9B0; color: #FFFFFF; }
        #fontBtn:hover { background-color: #6EDDC5; }
        #toolbarBtn { background-color: #2D5F8A; padding: 4px 10px; }
        #toolbarBtn:hover { background-color: #3A7AB0; }
        #navList {
            background-color: #252526; border: none; border-radius: 8px;
            outline: none; padding: 6px;
        }
        #navList::item {
            padding: 12px 16px; border-radius: 6px; margin: 2px 4px;
            color: #CCCCCC; font-size: 13px;
        }
        #navList::item:hover { background-color: #2D2D2D; }
        #navList::item:selected {
            background-color: #0E639C; color: #FFFFFF; font-weight: bold;
        }
        #pageTitle { font-size: 18px; font-weight: bold; color: #FFFFFF; }
        QLineEdit, QSpinBox {
            background-color: #333333; color: #D0D0D0;
            border: 1px solid #444444; border-radius: 4px; padding: 6px;
        }
        QLineEdit:focus, QSpinBox:focus { border: 1px solid #0E639C; }
        QLabel { color: #D0D0D0; background: transparent; }
        QTableWidget {
            background-color: #1E1E1E; color: #D0D0D0;
            border: 1px solid #333333; border-radius: 4px;
            gridline-color: #2D2D2D; selection-background-color: #094771;
        }
        QHeaderView::section {
            background-color: #2D2D2D; color: #D0D0D0;
            border: 1px solid #333333; padding: 4px; font-weight: bold;
        }
        QScrollBar:vertical { background-color: #1E1E1E; width: 8px; border: none; }
        QScrollBar::handle:vertical { background-color: #444444; border-radius: 4px; min-height: 20px; }
        QScrollBar::handle:vertical:hover { background-color: #0E639C; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )");

    QHBoxLayout *mainLayout = new QHBoxLayout(&dialog);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // === 左侧导航栏 ===
    QListWidget *navList = new QListWidget();
    navList->setObjectName("navList");
    navList->setFixedWidth(180);
    navList->setIconSize(QSize(18, 18));
    navList->addItem(tr("  字体设置"));
    navList->addItem(tr("  关键字高亮"));
    navList->addItem(tr("  发送设置"));
    navList->addItem(tr("  颜色设置"));
    navList->setCurrentRow(0);
    mainLayout->addWidget(navList);

    // === 右侧内容区 ===
    QStackedWidget *stack = new QStackedWidget();
    stack->setStyleSheet("QStackedWidget { background-color: #1E1E1E; }");

    // --- 页面1: 字体设置 ---
    QWidget *fontPage = new QWidget();
    QVBoxLayout *fontPageLayout = new QVBoxLayout(fontPage);
    fontPageLayout->setContentsMargins(30, 24, 30, 24);
    fontPageLayout->setSpacing(16);

    QLabel *fontTitle = new QLabel(tr("字体设置"));
    fontTitle->setObjectName("pageTitle");
    fontPageLayout->addWidget(fontTitle);

    QLabel *fontDesc = new QLabel(tr("设置接收区和发送区的字体样式和大小。也可使用 Ctrl+滚轮 直接缩放。"));
    fontDesc->setWordWrap(true);
    fontDesc->setStyleSheet("color: #999999; font-size: 12px;");
    fontPageLayout->addWidget(fontDesc);

    QFrame *fontSep = new QFrame();
    fontSep->setFrameShape(QFrame::HLine);
    fontSep->setStyleSheet("color: #333333;");
    fontPageLayout->addWidget(fontSep);

    // 字体族选择
    QLabel *familyLabel = new QLabel(tr("字体:"));
    familyLabel->setStyleSheet("font-size: 13px; color: #CCCCCC;");
    fontPageLayout->addWidget(familyLabel);

    QFontComboBox *fontCombo = new QFontComboBox();
    fontCombo->setCurrentFont(QFont(m_fontFamily));
    fontCombo->setStyleSheet(R"(
        QFontComboBox { background-color: #333333; color: #D0D0D0; border: 1px solid #444444; border-radius: 4px; padding: 6px; font-size: 13px; }
        QFontComboBox:hover { border: 1px solid #555555; }
        QFontComboBox::drop-down { border: none; width: 20px; }
        QFontComboBox QAbstractItemView { background-color: #2D2D2D; color: #D0D0D0; border: 1px solid #444444; selection-background-color: #094771; }
    )");
    fontPageLayout->addWidget(fontCombo);

    // 字号选择
    QLabel *sizeLabel = new QLabel(tr("字号:"));
    sizeLabel->setStyleSheet("font-size: 13px; color: #CCCCCC; margin-top: 8px;");
    fontPageLayout->addWidget(sizeLabel);

    QSpinBox *sizeSpin = new QSpinBox();
    sizeSpin->setRange(6, 36);
    sizeSpin->setValue(m_fontSize);
    sizeSpin->setFixedWidth(100);
    sizeSpin->setStyleSheet(R"(
        QSpinBox { background-color: #333333; color: #D0D0D0; border: 1px solid #444444; border-radius: 4px; padding: 6px; font-size: 13px; }
        QSpinBox:focus { border: 1px solid #0E639C; }
    )");
    fontPageLayout->addWidget(sizeSpin);

    // 实时预览
    QLabel *previewLabel = new QLabel(tr("效果预览 AaBbCc 0123"));
    previewLabel->setStyleSheet("font-size: 14px; padding: 12px; background-color: #252526; border: 1px solid #333333; border-radius: 6px; margin-top: 8px;");
    previewLabel->setFont(QFont(m_fontFamily, m_fontSize));
    fontPageLayout->addWidget(previewLabel);

    // 实时应用
    connect(fontCombo, &QFontComboBox::currentFontChanged, this, [this, sizeSpin, previewLabel](const QFont &f) {
        m_fontFamily = f.family();
        previewLabel->setFont(QFont(m_fontFamily, sizeSpin->value()));
        applyGlobalFont();
        saveConfig();
    });
    connect(sizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, fontCombo, previewLabel](int size) {
        m_fontSize = size;
        previewLabel->setFont(QFont(fontCombo->currentFont().family(), size));
        applyGlobalFont();
        saveConfig();
    });

    fontPageLayout->addStretch();
    stack->addWidget(fontPage);

    // --- 页面2: 关键字高亮 ---
    QWidget *kwPage = new QWidget();
    QVBoxLayout *kwPageLayout = new QVBoxLayout(kwPage);
    kwPageLayout->setContentsMargins(30, 24, 30, 24);
    kwPageLayout->setSpacing(16);

    QLabel *kwTitle = new QLabel(tr("关键字高亮"));
    kwTitle->setObjectName("pageTitle");
    kwPageLayout->addWidget(kwTitle);

    QLabel *kwDesc = new QLabel(tr("添加关键字后，接收区中匹配的文本会以指定颜色高亮显示。"));
    kwDesc->setWordWrap(true);
    kwDesc->setStyleSheet("color: #999999; font-size: 12px;");
    kwPageLayout->addWidget(kwDesc);

    QFrame *kwSep = new QFrame();
    kwSep->setFrameShape(QFrame::HLine);
    kwSep->setStyleSheet("color: #333333;");
    kwPageLayout->addWidget(kwSep);

    QHBoxLayout *kwInputLayout = new QHBoxLayout();
    kwInputLayout->addWidget(m_keywordEdit);
    kwInputLayout->addWidget(m_addKeywordBtn);
    kwInputLayout->addWidget(m_removeKeywordBtn);
    kwPageLayout->addLayout(kwInputLayout);
    kwPageLayout->addWidget(m_keywordTable);
    kwPageLayout->addStretch();
    stack->addWidget(kwPage);

    // --- 页面3: 发送设置 ---
    QWidget *sendPage = new QWidget();
    QVBoxLayout *sendPageLayout = new QVBoxLayout(sendPage);
    sendPageLayout->setContentsMargins(30, 24, 30, 24);
    sendPageLayout->setSpacing(16);

    QLabel *sendTitle = new QLabel(tr("发送设置"));
    sendTitle->setObjectName("pageTitle");
    sendPageLayout->addWidget(sendTitle);

    QLabel *sendDesc = new QLabel(tr("配置数据发送的格式和定时发送参数。"));
    sendDesc->setWordWrap(true);
    sendDesc->setStyleSheet("color: #999999; font-size: 12px;");
    sendPageLayout->addWidget(sendDesc);

    QFrame *sendSep = new QFrame();
    sendSep->setFrameShape(QFrame::HLine);
    sendSep->setStyleSheet("color: #333333;");
    sendPageLayout->addWidget(sendSep);

    QFormLayout *sendFormLayout = new QFormLayout();
    sendFormLayout->setSpacing(12);
    sendFormLayout->addRow(m_hexSendCheck);
    sendFormLayout->addRow(m_autoSendCheck);
    sendFormLayout->addRow(tr("间隔(ms)"), m_autoSendIntervalSpin);
    sendFormLayout->addRow(tr("换行符"), m_newlineCombo);
    sendPageLayout->addLayout(sendFormLayout);
    sendPageLayout->addStretch();
    stack->addWidget(sendPage);

    // --- 页面4: 颜色设置 ---
    QWidget *colorPage = new QWidget();
    QVBoxLayout *colorPageLayout = new QVBoxLayout(colorPage);
    colorPageLayout->setContentsMargins(30, 24, 30, 24);
    colorPageLayout->setSpacing(16);

    QLabel *colorTitle = new QLabel(tr("颜色设置"));
    colorTitle->setObjectName("pageTitle");
    colorPageLayout->addWidget(colorTitle);

    QLabel *colorDesc = new QLabel(tr("自定义接收区和发送区的显示颜色。"));
    colorDesc->setWordWrap(true);
    colorDesc->setStyleSheet("color: #999999; font-size: 12px;");
    colorPageLayout->addWidget(colorDesc);

    QFrame *colorSep = new QFrame();
    colorSep->setFrameShape(QFrame::HLine);
    colorSep->setStyleSheet("color: #333333;");
    colorPageLayout->addWidget(colorSep);

    QFormLayout *colorFormLayout = new QFormLayout();
    colorFormLayout->setSpacing(12);
    colorFormLayout->addRow(tr("接收文字"), m_recvColorBtn);
    colorFormLayout->addRow(tr("发送文字"), m_sendColorBtn);
    colorFormLayout->addRow(tr("时间戳"), m_timestampColorBtn);
    colorFormLayout->addRow(tr("接收区背景"), m_bgColorBtn);
    colorPageLayout->addLayout(colorFormLayout);
    colorPageLayout->addStretch();
    stack->addWidget(colorPage);

    mainLayout->addWidget(stack);

    // 导航切换
    connect(navList, &QListWidget::currentRowChanged, stack, &QStackedWidget::setCurrentIndex);

    dialog.exec();

    // 对话框关闭后，将控件从对话框中脱离（防止被 Qt 自动删除）
    m_keywordEdit->setParent(nullptr);
    m_addKeywordBtn->setParent(nullptr);
    m_removeKeywordBtn->setParent(nullptr);
    m_keywordTable->setParent(nullptr);
    m_hexSendCheck->setParent(nullptr);
    m_autoSendCheck->setParent(nullptr);
    m_autoSendIntervalSpin->setParent(nullptr);
    m_newlineCombo->setParent(nullptr);
    m_recvColorBtn->setParent(nullptr);
    m_sendColorBtn->setParent(nullptr);
    m_timestampColorBtn->setParent(nullptr);
    m_bgColorBtn->setParent(nullptr);
}

void MainWindow::createCollapseButton(const QString &title, QFrame *content, bool collapsed, QVBoxLayout *parentLayout)
{
    QToolButton *btn = new QToolButton();
    btn->setText("  " + title);
    btn->setCheckable(true);
    btn->setChecked(!collapsed);
    btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn->setArrowType(collapsed ? Qt::ArrowType::RightArrow : Qt::ArrowType::DownArrow);
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btn->setObjectName("collapseBtn");

    content->setVisible(!collapsed);

    connect(btn, &QToolButton::toggled, this, [content, btn](bool expanded) {
        content->setVisible(expanded);
        btn->setArrowType(expanded ? Qt::ArrowType::DownArrow : Qt::ArrowType::RightArrow);
    });

    parentLayout->addWidget(btn);
    parentLayout->addWidget(content);
}
