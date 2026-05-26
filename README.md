# SerialPilot - 串口调试助手MAX

基于 Qt 5.14.2 的双串口调试工具，深色主题，支持同时连接两个串口、分屏查看数据。

## 功能特性

- **双串口同时连接** — 两个独立串口实例，互不干扰
- **分屏数据显示** — 左右分屏查看两个串口的收发数据
- **双串口同步操作** — 同步暂停/清空/滚动/导出
- **深色主题** — 护眼深色配色方案
- **彩色文本** — 接收(绿)、发送(金)、时间戳(青) 颜色区分
- **Ctrl+滚轮缩放** — 自由调整接收区字体大小
- **自定义字体** — QFontDialog 选择接收区字体
- **折叠面板** — 接收设置/发送设置可折叠，节省空间
- **配置持久化** — 自动保存/加载配置 (config/config.json)
- **日志系统** — 单例旋转日志，记录运行状态

## 技术栈

- Qt 5.14.2
- C++11
- MinGW 64-bit
- qmake

## 项目结构

```
test/
├── main.cpp                        # 入口
└── project/
    ├── main_window.h/cpp           # 主窗口 UI + 业务逻辑
    ├── serial_manager.h/cpp        # QSerialPort 封装
    ├── serial_thread.h/cpp         # 串口线程
    ├── keyword_highlighter.h/cpp   # 关键字高亮
    └── logger.h/cpp                # 日志系统
```

## 构建

```bash
# 配置 Qt 路径
QT_DIR="E:/Esoft/QT/Qt5.14.2/5.14.2/mingw73_64"
TOOLS_DIR="E:/Esoft/QT/Qt5.14.2/Tools/mingw730_64"

# 生成 Makefile
"$QT_DIR/bin/qmake.exe" test/test.pro -spec win32-g++

# 编译
"$TOOLS_DIR/bin/mingw32-make.exe" debug    # Debug 版本
"$TOOLS_DIR/bin/mingw32-make.exe" release  # Release 版本
```

## 界面预览

深色主题配色：

| 元素 | 颜色 |
|------|------|
| 接收数据 | ![#00FF00](https://via.placeholder.com/12/00FF00/00FF00.png) `#00FF00` 绿色 |
| 时间戳 | ![#00CED1](https://via.placeholder.com/12/00CED1/00CED1.png) `#00CED1` 青色 |
| 发送数据 | ![#FFD700](https://via.placeholder.com/12/FFD700/FFD700.png) `#FFD700` 金色 |
| 按钮 | ![#0E639C](https://via.placeholder.com/12/0E639C/0E639C.png) `#0E639C` 蓝色 |
| 接收区背景 | ![#080808](https://via.placeholder.com/12/080808/080808.png) `#080808` 深黑 |
| 主窗口背景 | ![#1E1E1E](https://via.placeholder.com/12/1E1E1E/1E1E1E.png) `#1E1E1E` 深灰 |

## 许可证

MIT License
