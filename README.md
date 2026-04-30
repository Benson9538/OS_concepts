# OS Concepts

經由實作學習作業系統中的核心概念。

## 主題

### shell — Mini Shell
實作一個支援以下功能的 shell：
- `fork` / `execvp` 執行外部指令
- pipe（`|`）串接兩個指令
- I/O redirection（`>`、`>>`、`<`）
- 內建指令：`cd`、`exit`

### raceCondition — Race Condition 與 Mutex
從三種狀況去實際觀察 race condition，透過 valgrind , helgrind 關注細節：
- `race.c`：未保護的 counter，展示 race condition
- `newRace.c`：用 local sum 減少 lock 競爭，提升效能
- `hidden_race.c`：隱性 race condition，搭配 valgrind , helgrind 觀察

### semaphore — Producer-Consumer
使用三個 semaphore（mutex / empty / full）實作 bounded buffer，支援多個 producer 與 consumer 同時運作。
