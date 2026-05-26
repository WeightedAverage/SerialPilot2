# Task Plan: 串口调试助手代码审查与修复

## Goal
对现有代码进行详细审查，发现并修复 bug、清理死代码、改进代码质量。

## Status: `in_progress`

---

## Phase 1: 代码审查 `complete`
- [x] 阅读所有源码文件
- [x] 分析架构设计
- [x] 发现 bug 和问题
- [x] 写入 findings.md 审查报告

## Phase 2: 修复严重 Bug `pending`
- [ ] 修复停止位映射 (index→QSerialPort::StopBits)
- [ ] 修复校验位映射 (index→QSerialPort::Parity)
- [ ] 修复流控映射 (index→QSerialPort::FlowControl)
- [ ] 解决 waitForBytesWritten 阻塞 UI 问题

## Phase 3: 清理死代码 `pending`
- [ ] 删除 serial_thread.h/cpp
- [ ] 删除 mainwindow.h/cpp 和 mainwindow.ui
- [ ] 从 .pro 中确认无残留引用

## Phase 4: 修复中等问题 `pending`
- [ ] 移除未使用的 m_bufferSizeSpin 或实现缓冲区限制
- [ ] 移除未使用的 m_autoNewlineCheck 或实现自动换行逻辑
- [ ] 重构重复代码 (onConnect1/2 等)

---

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
| (none yet) | | |

## Decisions
| Decision | Rationale |
|----------|-----------|
| 优先修复枚举映射 bug | 直接影响串口参数配置正确性 |
| 删除死代码 | 减少维护负担，避免混淆 |
