# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

加权平均数的串口调试助手MAX — 基于 Qt 5.14.2 的双串口调试工具，深色主题，支持同时连接两个串口、分屏查看数据。

**技术栈：** Qt 5.14.2 / C++11 / MinGW 64-bit / qmake

## Build Commands

```bash
# Qt 路径
QT_DIR="E:/Esoft/QT/Qt5.14.2/5.14.2/mingw73_64"
TOOLS_DIR="E:/Esoft/QT/Qt5.14.2/Tools/mingw730_64"

# 从构建目录编译
cd build-test-Desktop_Qt_5_14_2_MinGW_64_bit-Debug
"$TOOLS_DIR/bin/mingw32-make.exe" debug    # Debug 版本
"$TOOLS_DIR/bin/mingw32-make.exe" release  # Release 版本

# 重新生成 Makefile
"$QT_DIR/bin/qmake.exe" ../test/test.pro -spec win32-g++
```

可执行文件：`build-test-Desktop_Qt_5_14_2_MinGW_64_bit-Debug/debug/test.exe`

## Architecture

```
main.cpp（入口）
 └─ MainWindow (project/main_window.h/cpp) — UI + 业务逻辑 (~1550行)
     ├─ SerialManager (project/serial_manager.h/cpp) — QSerialPort 封装 (readyRead信号驱动)
     └─ AppLogger (project/logger.h/cpp) — 单例旋转日志系统 (logs/)
```

## Key Implementation Details

- **双串口架构：** 两个独立的 `SerialManager` 实例，readyRead 信号驱动数据接收
- **接收缓冲合并：** `QTimer(30ms)` 合并碎片数据
- **彩色文本：** `QTextCursor` + `QTextCharFormat` 设置颜色
- **Ctrl+滚轮缩放：** `eventFilter()` 拦截 `QEvent::Wheel` + `Qt::ControlModifier`
- **配置持久化：** `QJsonDocument` 读写 `config/config.json`
- **双串口同步：** 同步操作暂停/清空/滚动/导出
- **折叠面板：** 接收设置/发送设置可折叠
- **字体设置：** QFontDialog 自定义接收区字体

## Color Scheme

| 元素 | 颜色 |
|------|------|
| 接收数据 | 绿色 #00FF00 |
| 时间戳 | 青色 #00CED1 |
| 发送数据 | 金色 #FFD700 |
| 按钮 | 蓝色 #0E639C |
| 接收区背景 | 深黑 #080808 |
| 主窗口背景 | #1E1E1E |
