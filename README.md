學習一個OS能運作的最基本功能

1.底層系統與開機流程：使用 C 語言與 RISC-V 組合語言，在 QEMU 模擬器環境下從零構建作業系統核心，實作啟動引導（Bootstrapping）、異常與中斷處理（Trap/Exception Handling）。

2.虛擬記憶體與特權隔離：實作 Sv32 Page Table，透過 CPU 特權等級切換（S-Mode / U-Mode）與硬體 MMU 權限位元實現使用者空間（User Space）隔離與保護。

3.行程排程與系統呼叫：設計協同式多工作業（Cooperative Multitasking）與上下文切換（Context Switching）機制，並建立 ecall / sret 系統呼叫介面供使用者應用程式與核心溝通。

4.周邊驅動與檔案系統：實作 VirtIO 虛擬磁碟驅動程式（MMIO / Virtqueue 協定），並建構簡易檔案系統（Tar-based File System）支援磁碟讀寫與使用者檔案存取。
