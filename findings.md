# Findings: 代码审查报告

## 审查日期: 2026-05-26

---

## 1. 严重 Bug (会导致功能异常)

### 1.1 停止位映射错误 — `main_window.cpp:1103,1162`
```cpp
config.stopBits = static_cast<QSerialPort::StopBits>(m_stopBitsCombo1->currentIndex() + 1);
```
Combo 索引: 0="1", 1="1.5", 2="2"
直接 +1 得到: 1, 2, 3
但 Qt 枚举值: `OneStop=1`, `OneAndHalfStop=3`, `TwoStop=2`
**结果:** 选择 "1.5" 会实际设置为 TwoStop，选择 "2" 会设置为 OneAndHalfStop。

### 1.2 校验位映射错误 — `main_window.cpp:1104,1163`
```cpp
config.parity = static_cast<QSerialPort::Parity>(m_parityCombo1->currentIndex());
```
Combo 索引: 0=None, 1=Even, 2=Odd, 3=Mark, 4=Space
Qt 枚举值: `NoParity=0`, `EvenParity=2`, `OddParity=3`, `MarkParity=4`, `SpaceParity=5`
**结果:** 选择 "Even" 实际设置为 `Parity(1)`（无效值），所有校验位偏移 1。

### 1.3 流控映射错误 — `main_window.cpp:1104,1164`
```cpp
config.flowControl = static_cast<QSerialPort::FlowControl>(m_flowCtrlCombo1->currentIndex());
```
Combo 索引: 0=None, 1=RTS/CTS, 2=Xon/Xoff
Qt 枚举值: `NoFlowControl=0`, `HardwareControl=2`, `SoftwareControl=1`
**结果:** 选择 "RTS/CTS" 实际设置为 `SoftwareControl(Xon/Xoff)`，两者互换。

### 1.4 waitForBytesWritten 阻塞 UI 线程 — `serial_manager.cpp:97`
```cpp
m_serialPort->waitForBytesWritten(1000);
```
`sendData()` 从 UI 线程调用，`waitForBytesWritten` 会阻塞最多 1 秒。高频发送或设备无响应时 UI 会卡死。

---

## 2. 中等问题

### 2.1 SerialThread 死代码 — `serial_thread.h/cpp`
- `serial_thread.h/cpp` 存在但未被 `.pro` 包含，也未被任何代码使用
- `SerialThread::run()` 调用 `m_manager->readData()`，但 `SerialManager` 没有 `readData()` 方法
- 这是一个废弃的线程读取方案，已被 `readyRead` 信号驱动替代
- **建议:** 删除整个文件

### 2.2 mainwindow.h/cpp 死代码 — `mainwindow.h/cpp`
- 旧版 UI Form 版本的 MainWindow，`main.cpp` 不使用它
- `.pro` 也未包含 `mainwindow.h/cpp`
- **建议:** 删除，连同 `mainwindow.ui`

### 2.3 m_bufferSizeSpin 未使用 — `main_window.cpp:236`
- 缓冲区大小 SpinBox 值被保存/加载，但从未实际控制任何缓冲逻辑
- `m_recvBuffer1`/`m_recvBuffer2` 是无限制增长的 QByteArray

### 2.4 m_autoNewlineCheck 未使用 — `main_window.cpp:240`
- "自动换行" 复选框被创建和保存配置，但从未在接收逻辑中检查

### 2.5 重复代码严重
- `onConnect1()`/`onConnect2()` 几乎完全相同 (~60 行重复)
- `onPortRefresh1()`/`onPortRefresh2()` 完全相同
- `m_recvBuffer1`/`m_recvBuffer2`、`m_paused1`/`m_paused2` 等成对变量可用数组替代
- 信号线轮询 `updateSignalStatus()` 中两个串口的代码完全重复

### 2.6 父子对象生命周期
- `m_serialManager1 = new SerialManager(this)` 创建时 parent=this
- 断开时 `delete m_serialManager1` 显式删除
- 如果窗口销毁时仍连接着，Qt 父子机制也会尝试删除 → 虽然 nullptr 检查可避免双重删除，但析构函数中未显式清理

---

## 3. 轻微问题 / 代码风格

### 3.1 使用废弃的 foreach 宏 — `serial_manager.cpp:117`
```cpp
foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
```
Qt 官方建议使用 C++11 range-based for。

### 3.2 main_window.cpp 过长 (~1667 行)
- UI 构建、业务逻辑、事件处理全在一个文件
- 建议拆分为: UI setup、连接管理、数据处理、配置管理

### 3.3 saveConfig() 不保存 splitter 状态
- 分屏比例未持久化

### 3.4 loadConfig() 不恢复窗口位置
- 只保存了宽高，未保存 position

### 3.5 日志文件大小检查不精确 — `logger.cpp:57`
- `rotateIfNeeded()` 在每次写入时检查，但 `m_file.size()` 可能不反映刚写入的内容（未 flush 前）

---

## 4. 架构观察

### 4.1 正面评价
- readyRead 信号驱动接收，避免了线程安全问题
- 30ms 合并缓冲减少 UI 刷新频率，设计合理
- 深色主题 QSS 样式完整且一致
- 配置持久化覆盖全面
- 双串口同步操作设计合理（blockSignals 防止循环触发）

### 4.2 整体结构
```
main.cpp → MainWindow (UI + 业务逻辑, ~1667行)
              └── SerialManager (QSerialPort 封装)
              └── AppLogger (单例日志)
              └── SerialThread (死代码, 未使用)
              └── mainwindow.h/cpp (死代码, 未使用)
```
