typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef uint32_t size_t;
#include "common.h"
#include "kernel.h"

extern char __bss[], __bss_end[], __stack_top[], __free_ram[], __free_ram_end[];

/// 實體地址由32bits決定，一個位編號地址以存放1bytes ==>記憶體空間 2^32次方bytes  = 4GB;
// OS把4KB當作一個page,所以需要一個12bits (offeset) 來取一個page內的某個位址;
// 總共有2^20個page，所以用20個bits決定which page,所以剛好20+12個bits等於一個bytes的變數就能控制所有記憶體位址
// 我們把前20bits分成兩階段10+10bits當作vpn[0],vpn[1]
void map_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags)
{
    if (!is_aligned(vaddr, PAGE_SIZE))
        PANIC("unaligned vaddr %x", vaddr);

    if (!is_aligned(paddr, PAGE_SIZE))
        PANIC("unaligned paddr %x", paddr);
    /** 31                               10  9   8 7 6 5 4 3 2 1 0
        +-----------------------------------+-----+-+-+-+-+-+-+-+-+
        |       PPN (Physical Page Number)  | RSW |D|A|G|U|X|W|R|V|
        |            (22 bits)              |     |     Flags     |
        +-----------------------------------+-----+-+-+-+-+-+-+-+-+**/
    uint32_t vpn1 = (vaddr >> 22) & 0x3ff; // 只取vaddr的最大10個bits
    if ((table1[vpn1] & PAGE_V) == 0)
    {
        // Create the 2nd level page table if it doesn't exist.
        uint32_t pt_paddr = alloc_pages(1);
        table1[vpn1] = ((pt_paddr / PAGE_SIZE) << 10) | PAGE_V; // RISC-VSv32 硬體規定 PTE(上圖) 的 Bit 10 ~ 31 用來存放 PPN((pt_paddr / PAGE_SIZE))，所以向左位移 10 個 bit，推到正確的硬體欄位/
        // Bit 0 ~ 9（共 10 個 bit）：留給權限與狀態旗標（如 PAGE_V、PAGE_R 等）。
    }

    // Set the 2nd level page table entry to map the physical page.
    uint32_t vpn0 = (vaddr >> 12) & 0x3ff;
    uint32_t *table0 = (uint32_t *)((table1[vpn1] >> 10) * PAGE_SIZE);
    table0[vpn0] = ((paddr / PAGE_SIZE) << 10) | flags | PAGE_V;
}

// allocate n pages size of memory and return begin address;
paddr_t alloc_pages(uint32_t n)
{
    static paddr_t next_paddr = (paddr_t)__free_ram; // 別忘了static 只會第一次初始化會執行 ，第二次呼叫alloc_pages()就不會執行這行!
    paddr_t paddr = next_paddr;
    next_paddr += n * PAGE_SIZE;
    if (next_paddr > (paddr_t)__free_ram_end)
    {
        PANIC("out of memory");
    }
    memset((void *)paddr, 0, n * PAGE_SIZE);
    return paddr;
}
struct process
{
    int pid;             // Process ID
    int state;           // Process state: PROC_UNUSED or PROC_RUNNABLE
    vaddr_t sp;          // Stack pointer
    uint8_t stack[8192]; // Kernel stack /*核心堆疊（kernel stack）中包含了儲存的 CPU 暫存器、返回位址（也就是從哪裡被呼叫）、以及區域變數。
                         /*透過為每個行程準備一個核心堆疊，我們就可以實作「上下文切換（context switching）」：儲存與還原 CPU 暫存器，以及切換堆疊指標。**/
};
struct process procs[PROCS_MAX];

struct process *create_process(uint32_t pc)
{
    struct process *proc = NULL;
    int i = 0;
    for (i = 0; i < PROCS_MAX; ++i)
    {
        if (procs[i].state == PROC_UNUSED)
        {
            proc = &procs[i];
            break;
        }
    }
    if (!proc)
    {
        PANIC("No free process slots");
    }
    uint32_t *sp = (uint32_t *)&proc->stack[sizeof(proc->stack)]; // 取得process中stack的最高位址
    // 把日後用來存放register內容的空間先初始化
    *--sp = 0;            // s11
    *--sp = 0;            // s10
    *--sp = 0;            // s9
    *--sp = 0;            // s8
    *--sp = 0;            // s7
    *--sp = 0;            // s6
    *--sp = 0;            // s5
    *--sp = 0;            // s4
    *--sp = 0;            // s3
    *--sp = 0;            // s2
    *--sp = 0;            // s1
    *--sp = 0;            // s0
    *--sp = (uint32_t)pc; // ra ,紀錄entry point;

    proc->pid = i + 1;
    proc->state = PROC_RUNNABLE;
    proc->sp = (uint32_t)sp;
    return proc;
};

void delay(void)
{
    for (int i = 0; i < 30000000; ++i)
    {
        __asm__ __volatile__("nop");
    }
}
struct process *proc_a;
struct process *proc_b;
struct process *current_proc;
struct process *idle_proc;

void yield(void)
{
    struct process *next = idle_proc;
    for (int i = 0; i < PROCS_MAX; ++i)
    {
        struct process *proc = &procs[(current_proc->pid + i) % PROCS_MAX];
        if (proc->state == PROC_RUNNABLE && proc->pid > 0) // proc->pid>0為了避免 Idle process時候觸發yield又next =Idle process 無法正確context switch
        {
            next = proc;
            break;
        }
    }
    // If there's no runnable process other than the current one, return and continue processing
    if (next == current_proc)
    {
        return;
    }

    /**預先把『即將要執行的行程（next）』的核心堆疊頂端位址設定到 RISC-V 的 sscratch 控制暫存器中 **/
    /**未來隨時發生的 Exception / Syscall 做準備**/
    __asm__ __volatile__(
        "csrw sscratch, %[sscratch]\n"
        :
        : [sscratch] "r"((uint32_t)&next->stack[sizeof(next->stack)]));

    // context switch
    struct process *prev = current_proc;
    current_proc = next;
    switch_context(&prev->sp, &next->sp);
}

void proc_a_entry(void)
{
    printf("starting process A\n");
    while (1)
    {
        putchar('A');
        switch_context(&proc_a->sp, &proc_b->sp);
        yield();
    }
}

void proc_b_entry(void)
{
    printf("starting process B\n");
    while (1)
    {
        putchar('B');
        switch_context(&proc_b->sp, &proc_a->sp);
        yield();
    }
}
struct sbiret sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4,
                       long arg5, long fid, long eid)
{ /**register：這是 C 語言原本就有的關鍵字，用來暗示編譯器：「這個變數非常重要且常用，請盡量把它放在 CPU 的暫存器裡，不要放在外面存取緩慢的記憶體，以加快速度」。
   原本的 C 語言只能「建議」使用暫存器，但「不能指定」具體要放在哪一個。 加上 __asm__("a0") 後，你等於是在強迫 Clang/GCC 編譯器：「不准自作主張，請你絕對、精準地把這個變數，死死綁定在實體 CPU 的 a0 暫存器上！」。
   最後，把你呼叫函式時傳進來的第一個參數 arg0，直接塞進這個已經與實體 a0 暫存器融為一體的變數裡。**/
    register long a0 __asm__("a0") = arg0;
    register long a1 __asm__("a1") = arg1;
    register long a2 __asm__("a2") = arg2;
    register long a3 __asm__("a3") = arg3;
    register long a4 __asm__("a4") = arg4;
    register long a5 __asm__("a5") = arg5;
    register long a6 __asm__("a6") = fid;
    register long a7 __asm__("a7") = eid; // extension id
    /**ECALL 指令被用作 Supervisor 模式（S 模式）與 SEE（SBI 執行環境，例如 OspenSBI）之間的控制轉移指令。 **/
    /*各種功能可以到sbi_ecall_interface.h 查找*(https://github.com/riscv-software-src/opensbi/blob/0ad866067d7853683d88c10ea9269ae6001bcf6f/include/sbi/sbi_ecall_interface.h)*/
    //  /** SBI Extension IDs **/
    // #define SBI_EXT_0_1_SET_TIMER			0x0
    // #define SBI_EXT_0_1_CONSOLE_PUTCHAR		0x1
    // #define SBI_EXT_0_1_CONSOLE_GETCHAR		0x2
    // #define SBI_EXT_0_1_CLEAR_IPI			0x3
    // #define SBI_EXT_0_1_SEND_IPI			0x4
    // #define SBI_EXT_0_1_REMOTE_FENCE_I		0x5
    // #define SBI_EXT_0_1_REMOTE_SFENCE_VMA		0x6
    // #define SBI_EXT_0_1_REMOTE_SFENCE_VMA_ASID	0x7
    // #define SBI_EXT_0_1_SHUTDOWN			0x8
    // #define SBI_EXT_BASE				0x10
    // #define SBI_EXT_TIME				0x54494D45
    // #define SBI_EXT_IPI				0x735049
    // #define SBI_EXT_RFENCE				0x52464E43
    // #define SBI_EXT_HSM				0x48534D
    // #define SBI_EXT_SRST				0x53525354
    // #define SBI_EXT_PMU				0x504D55
    // #define SBI_EXT_DBCN				0x4442434E
    // #define SBI_EXT_SUSP				0x53555350
    // #define SBI_EXT_CPPC				0x43505043
    //
    __asm__ __volatile__("ecall"
                         : "=r"(a0), "=r"(a1)
                         : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7)
                         : "memory");
    return (struct sbiret){.error = a0, .value = a1};
}

void putchar(char ch)
{
    /*#define SBI_EXT_0_1_CONSOLE_PUTCHAR		0x1*/
    sbi_call(ch, 0, 0, 0, 0, 0, 0, 1 /* Console Putchar */);
}

__attribute__((naked))
__attribute__((aligned(4))) void
kernel_entry(void)
{ /**sp 現在指向的是目前執行中的程序的「核心（而非使用者）」堆疊。而 sscratch 則保存了例外發生當下原本的 sp 值（也就是使用者堆疊指標）。 */
    __asm__ __volatile__(
        /**csrrw sp, sscratch, sp $\rightarrow$ sp 成功取得剛才在 yield() 中預先寫入 sscratch 的核心堆疊頂端位址。**/
        "csrrw sp, sscratch, sp\n"
        "addi sp, sp, -4 * 31\n"
        "sw ra,  4 * 0(sp)\n"
        "sw gp,  4 * 1(sp)\n"
        "sw tp,  4 * 2(sp)\n"
        "sw t0,  4 * 3(sp)\n"
        "sw t1,  4 * 4(sp)\n"
        "sw t2,  4 * 5(sp)\n"
        "sw t3,  4 * 6(sp)\n"
        "sw t4,  4 * 7(sp)\n"
        "sw t5,  4 * 8(sp)\n"
        "sw t6,  4 * 9(sp)\n"
        "sw a0,  4 * 10(sp)\n"
        "sw a1,  4 * 11(sp)\n"
        "sw a2,  4 * 12(sp)\n"
        "sw a3,  4 * 13(sp)\n"
        "sw a4,  4 * 14(sp)\n"
        "sw a5,  4 * 15(sp)\n"
        "sw a6,  4 * 16(sp)\n"
        "sw a7,  4 * 17(sp)\n"
        "sw s0,  4 * 18(sp)\n"
        "sw s1,  4 * 19(sp)\n"
        "sw s2,  4 * 20(sp)\n"
        "sw s3,  4 * 21(sp)\n"
        "sw s4,  4 * 22(sp)\n"
        "sw s5,  4 * 23(sp)\n"
        "sw s6,  4 * 24(sp)\n"
        "sw s7,  4 * 25(sp)\n"
        "sw s8,  4 * 26(sp)\n"
        "sw s9,  4 * 27(sp)\n"
        "sw s10, 4 * 28(sp)\n"
        "sw s11, 4 * 29(sp)\n"

        "csrr a0, sscratch\n" /*把進入 Kernel 前的原始 sp 讀到 a0*/ /**從sscratch讀取存到a0 **/
        "sw a0, 4 * 30(sp)\n"                                       /*把原始 sp 存入 Stack Frame 的第 30 個位置*/

        // Reset the kernel stack.
        "addi a0, sp, 4 * 31\n"
        "csrw sscratch, a0\n"

        "mv a0, sp\n"        /*「目前這個 Stack Frame 的起始位址」複製到 a0*/
        "call handle_trap\n" /*call 是一個用來「呼叫函式（Function Call）」的Pseudo-instruction。=====handle_trap(a0)*/

        "lw ra,  4 * 0(sp)\n"
        "lw gp,  4 * 1(sp)\n"
        "lw tp,  4 * 2(sp)\n"
        "lw t0,  4 * 3(sp)\n"
        "lw t1,  4 * 4(sp)\n"
        "lw t2,  4 * 5(sp)\n"
        "lw t3,  4 * 6(sp)\n"
        "lw t4,  4 * 7(sp)\n"
        "lw t5,  4 * 8(sp)\n"
        "lw t6,  4 * 9(sp)\n"
        "lw a0,  4 * 10(sp)\n"
        "lw a1,  4 * 11(sp)\n"
        "lw a2,  4 * 12(sp)\n"
        "lw a3,  4 * 13(sp)\n"
        "lw a4,  4 * 14(sp)\n"
        "lw a5,  4 * 15(sp)\n"
        "lw a6,  4 * 16(sp)\n"
        "lw a7,  4 * 17(sp)\n"
        "lw s0,  4 * 18(sp)\n"
        "lw s1,  4 * 19(sp)\n"
        "lw s2,  4 * 20(sp)\n"
        "lw s3,  4 * 21(sp)\n"
        "lw s4,  4 * 22(sp)\n"
        "lw s5,  4 * 23(sp)\n"
        "lw s6,  4 * 24(sp)\n"
        "lw s7,  4 * 25(sp)\n"
        "lw s8,  4 * 26(sp)\n"
        "lw s9,  4 * 27(sp)\n"
        "lw s10, 4 * 28(sp)\n"
        "lw s11, 4 * 29(sp)\n"
        "lw sp,  4 * 30(sp)\n"
        "sret\n");
}

void handle_trap(struct trap_frame *f)
{
    uint32_t scause = READ_CSR(scause);
    uint32_t stval = READ_CSR(stval);
    uint32_t user_pc = READ_CSR(sepc);
    uint32_t omg = READ_CSR(stvec);
    PANIC("unexpected trap scause=%x, stval=%x, sepc=%x\n,stvec=%x", scause, stval, user_pc, omg);
}

__attribute__((naked)) void switch_context(uint32_t *prev_sp,
                                           uint32_t *next_sp)
{
    __asm__ __volatile__(
        // TLDR: 假設是從process A 轉到 proces B:前半段表示把目前process A在register的資料存到記憶體 (之後才有正確繼續執行process A)
        // 中間兩行把存放所有資料後stack pointer的位址存到a0，然後把stack pointer存入next_sp，所以現在sp是process B 的位址了
        // 後半段就是把process B每個位址的資料讀回register，去執行process B後續的任務。
        "addi sp, sp, -13 * 4\n" // Allocate stack space for 13 4-byte registers
        "sw ra,  0  * 4(sp)\n"   // Save callee-saved registers only
        "sw s0,  1  * 4(sp)\n"
        "sw s1,  2  * 4(sp)\n"
        "sw s2,  3  * 4(sp)\n"
        "sw s3,  4  * 4(sp)\n"
        "sw s4,  5  * 4(sp)\n"
        "sw s5,  6  * 4(sp)\n"
        "sw s6,  7  * 4(sp)\n"
        "sw s7,  8  * 4(sp)\n"
        "sw s8,  9  * 4(sp)\n"
        "sw s9,  10 * 4(sp)\n"
        "sw s10, 11 * 4(sp)\n"
        "sw s11, 12 * 4(sp)\n"

        // Switch the stack pointer.
        "sw sp, (a0)\n" // *prev_sp = sp;
        "lw sp, (a1)\n" //  sp=*next_sp

        // Restore callee-saved registers from the next process's stack.
        "lw ra,  0  * 4(sp)\n" // Restore callee-saved registers only
        "lw s0,  1  * 4(sp)\n"
        "lw s1,  2  * 4(sp)\n"
        "lw s2,  3  * 4(sp)\n"
        "lw s3,  4  * 4(sp)\n"
        "lw s4,  5  * 4(sp)\n"
        "lw s5,  6  * 4(sp)\n"
        "lw s6,  7  * 4(sp)\n"
        "lw s7,  8  * 4(sp)\n"
        "lw s8,  9  * 4(sp)\n"
        "lw s9,  10 * 4(sp)\n"
        "lw s10, 11 * 4(sp)\n"
        "lw s11, 12 * 4(sp)\n"
        "addi sp, sp, 13 * 4\n" // We've popped 13 4-byte registers from the stack
        "ret\n");
}

void kernel_main(void)
{
    memset(__bss, 0, (size_t)__bss_end - (size_t)__bss);
    printf("\n\nHello %s\n", "World!");
    printf("1 + 2 = %d, %x\n", 1 + 2, 0x1234abcd);
    WRITE_CSR(stvec, (uint32_t)kernel_entry); // stvec就是儲存例外發生時候要跳到哪個位址處理
    paddr_t paddr0 = alloc_pages(2);
    paddr_t paddr1 = alloc_pages(1);
    printf("alloc_pages test: paddr0=%x\n", paddr0);
    printf("alloc_pages test: paddr1=%x\n", paddr1);

    idle_proc = create_process((uint32_t)NULL);
    idle_proc->pid = 0; // idle
    current_proc = idle_proc;
    proc_a = create_process((uint32_t)proc_a_entry);
    proc_b = create_process((uint32_t)proc_b_entry);

    yield();
    PANIC("switched to idle process");
    //__asm__ __volatile__("unimp");            // new
    PANIC("booted!");
    printf("unreachable here!\n");
    for (;;)
    {
        __asm__ __volatile__("wfi");
    }
}

__attribute__((section(".text.boot")))
__attribute__((naked)) void
boot(void)
{
    __asm__ __volatile__(
        "mv sp, %[stack_top]\n" // Set the stack pointer
        "j kernel_main\n"       // Jump to the kernel main function
        :
        : [stack_top] "r"(__stack_top) // Pass the stack top address as %[stack_top]
    );
}
