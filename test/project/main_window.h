#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QTextEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QLineEdit>
#include <QGroupBox>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScrollArea>
#include <QSerialPort>
#include <QToolButton>
#include <QFrame>
#include <QVBoxLayout>
#include <QTableWidget>
#include <QDialog>

class SerialManager;
class KeywordHighlighter;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    // 串口连接
    void onPortRefresh1();
    void onPortRefresh2();
    void onConnect1();
    void onConnect2();

    // 数据接收
    void onDataReceived(const QByteArray &data, int portNum);
    void flushRecvBuffer(int portNum);

    // 数据发送
    void onSendClicked();
    void onSendHistoryChanged(int index);

    // 定时发送
    void onAutoSendToggled(bool checked);

    // 保存
    void onSaveClicked();
    void onExportClicked(int portNum);
    void onOpenSaveDir();
    void onOpenLogDir();

    // 状态
    void onClearStats();
    void updateSignalStatus();
    void updateTimeDisplay();
    void onConnectionLost(int portNum);

    // 错误
    void showError(const QString &msg);

    // 颜色设置
    void onRecvColorClicked();
    void onSendColorClicked();
    void onTimestampColorClicked();
    void onBgColorClicked();
    void onAddKeyword();
    void onRemoveKeyword();
    void applyRecvColors();
    void applySendColors();

private:
    void setupUi();
    void setupLeftPanel();
    void setupReceivePanel();
    void setupSendPanel();
    void setupStatusBar();
    void setupConnections();
    void applyStyleSheet();
    void loadConfig();
    void saveConfig();
    void appendToReceive(int portNum, const QString &text, const QColor &color);
    void appendTimestamp(int portNum);
    QStringList splitLogLines(const QString &text);
    QByteArray parseHexString(const QString &hex);
    void updateConnectButton(int portNum, bool connected);
    void createCollapseButton(const QString &title, QFrame *content, bool collapsed, QVBoxLayout *parentLayout);
    void applyGlobalFont();
    void connectToPort(int portNum);
    void showSettingsDialog();
    void setupTitleBar();
    void toggleMaximize();
    void minimizeWindow();

    // UI 组件
    QSplitter *m_splitter;
    QSplitter *m_vSplitter;
    QTextEdit *m_recvText1;
    QTextEdit *m_recvText2;
    QTextEdit *m_sendInput;
    QComboBox *m_sendTarget;

    // 串口1设置
    QComboBox *m_portCombo1;
    QPushButton *m_refreshBtn1;
    QComboBox *m_baudrateCombo1;
    QComboBox *m_dataBitsCombo1;
    QComboBox *m_stopBitsCombo1;
    QComboBox *m_parityCombo1;
    QComboBox *m_flowCtrlCombo1;
    QPushButton *m_connectBtn1;

    // 串口2设置
    QComboBox *m_portCombo2;
    QPushButton *m_refreshBtn2;
    QComboBox *m_baudrateCombo2;
    QComboBox *m_dataBitsCombo2;
    QComboBox *m_stopBitsCombo2;
    QComboBox *m_parityCombo2;
    QComboBox *m_flowCtrlCombo2;
    QPushButton *m_connectBtn2;

    // 接收设置
    QComboBox *m_encodingCombo;
    QSpinBox *m_bufferSizeSpin;
    QCheckBox *m_autoNewlineCheck;
    QCheckBox *m_showTimestampCheck;
    QCheckBox *m_hexDisplayCheck;
    QCheckBox *m_showSendCheck;
    QCheckBox *m_autoScrollCheck;

    // 发送设置
    QCheckBox *m_hexSendCheck;
    QCheckBox *m_autoSendCheck;
    QSpinBox *m_autoSendIntervalSpin;
    QComboBox *m_newlineCombo;

    // 接收区控件
    QPushButton *m_pauseBtn1;
    QPushButton *m_clearBtn1;
    QPushButton *m_scrollBtn1;
    QPushButton *m_exportBtn1;
    QPushButton *m_pauseBtn2;
    QPushButton *m_clearBtn2;
    QPushButton *m_scrollBtn2;
    QPushButton *m_exportBtn2;

    // 同步
    QCheckBox *m_syncCheck;

    // 发送区控件
    QPushButton *m_clearSendBtn;
    QPushButton *m_historyBtn;
    QComboBox *m_historyCombo;
    QPushButton *m_sendBtn;

    // 保存相关
    QLineEdit *m_saveDirEdit;
    QPushButton *m_saveBrowseBtn;
    QPushButton *m_saveBtn;
    QPushButton *m_openDirBtn;
    QPushButton *m_openLogBtn;

    // 自定义标题栏
    QWidget *m_titleBar;
    QLabel *m_titleLabel;
    QPushButton *m_minBtn;
    QPushButton *m_maxBtn;
    QPushButton *m_closeBtn;

    // 状态栏
    QLabel *m_statusLabel1;
    QLabel *m_statusLabel2;
    QLabel *m_sendStatsLabel;
    QLabel *m_recvStatsLabel;
    QPushButton *m_clearStatsBtn;
    QLabel *m_timeLabel;

    // 后端
    SerialManager *m_serialManager1;
    SerialManager *m_serialManager2;

    // 定时发送
    QTimer *m_autoSendTimer;
    QByteArray m_autoSendData;
    int m_autoSendTarget;

    // 状态
    bool m_connected1;
    bool m_connected2;
    bool m_paused1;
    bool m_paused2;
    bool m_autoScroll1;
    bool m_autoScroll2;
    int m_fontSize;
    QString m_fontFamily;

    // 颜色设置
    QColor m_recvColor;
    QColor m_sendColor;
    QColor m_timestampColor;
    QColor m_bgColor;
    QPushButton *m_recvColorBtn;
    QPushButton *m_sendColorBtn;
    QPushButton *m_timestampColorBtn;
    QPushButton *m_bgColorBtn;

    // 关键字高亮
    KeywordHighlighter *m_highlighter1;
    KeywordHighlighter *m_highlighter2;
    QLineEdit *m_keywordEdit;
    QPushButton *m_addKeywordBtn;
    QPushButton *m_removeKeywordBtn;
    QTableWidget *m_keywordTable;

    QByteArray m_recvBuffer1;
    QByteArray m_recvBuffer2;
    QTimer *m_recvMergeTimer1;
    QTimer *m_recvMergeTimer2;
    QTimer *m_signalTimer;
    QTimer *m_timeTimer;
    quint64 m_sendBytes1;
    quint64 m_sendBytes2;
    quint64 m_recvBytes1;
    quint64 m_recvBytes2;
    QStringList m_sendHistory;

    // 窗口拖拽
    bool m_dragging;
    QPoint m_dragPos;
};

#endif // MAIN_WINDOW_H
