#pragma once
#include "common.h"

#define PANIC(fmt, ...)                                                       \
    do                                                                        \
    {                                                                         \
        printf("PANIC: %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
        while (1)                                                             \
        {                                                                     \
        }                                                                     \
    } while (0)

#define PROCS_MAX 8 // Maximum number of processes

#define PROC_UNUSED 0   // Unused process control structure
#define PROC_RUNNABLE 1 // Runnable process

struct sbiret
{
    long error;
    long value;
};

struct trap_frame
{
    uint32_t ra;
    uint32_t gp;
    uint32_t tp;
    uint32_t t0;
    uint32_t t1;
    uint32_t t2;
    uint32_t t3;
    uint32_t t4;
    uint32_t t5;
    uint32_t t6;
    uint32_t a0;
    uint32_t a1;
    uint32_t a2;
    uint32_t a3;
    uint32_t a4;
    uint32_t a5;
    uint32_t a6;
    uint32_t a7;
    uint32_t s0;
    uint32_t s1;
    uint32_t s2;
    uint32_t s3;
    uint32_t s4;
    uint32_t s5;
    uint32_t s6;
    uint32_t s7;
    uint32_t s8;
    uint32_t s9;
    uint32_t s10;
    uint32_t s11;
    uint32_t sp;
} __attribute__((packed));

#define SATP_SV32 (1u << 31)
#define PAGE_V (1 << 0) // "Valid" bit (entry is enabled)
#define PAGE_R (1 << 1) // Readable
#define PAGE_W (1 << 2) // Writable
#define PAGE_X (1 << 3) // Executable
#define PAGE_U (1 << 4) // User (accessible in user mode)

/*C 語言的「井號 #」與「字串自動拼接」
為了讓巨集參數（例如傳入的 stvec 或 scause）變成字串，C 語言規定：

# 符號：放在巨集參數前面（如 #reg），會將該參數轉換成帶雙引號的字串。
例如：如果 reg 是 stvec，那麼 #reg 就會被預處理器替換成 "stvec"。

C 語言字串自動拼接（String Concatenation）：
在 C 語言中，兩個相鄰的字串常數會自動合併成一個字串。
例如："Hello " "World" 會自動變成 "Hello World"。*/
#define READ_CSR(reg)                                         \
    ({                                                        \
        unsigned long __tmp;                                  \
        __asm__ __volatile__("csrr %0, " #reg : "=r"(__tmp)); \
        __tmp;                                                \
    })

#define WRITE_CSR(reg, value)                                   \
    do                                                          \
    {                                                           \
        uint32_t __tmp = (value);                               \
        __asm__ __volatile__("csrw " #reg ", %0" ::"r"(__tmp)); \
    } while (0)

void putchar(char ch);
void switch_context(uint32_t *prev_sp, uint32_t *next_sp);
void yield(void);
paddr_t alloc_pages(uint32_t n);
struct process *create_process(uint32_t pc);
void delay(void);
void proc_a_entry(void);
void proc_b_entry(void);
void kernel_entry(void);
void kernel_main(void);
void map_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags);