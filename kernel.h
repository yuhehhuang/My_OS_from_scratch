#pragma once
#include "common.h"
#define SSTATUS_SPIE (1 << 5)
// 在像 ELF 這樣的可執行檔格式中，載入位址（load address）會被記錄在檔案的檔頭（ELF 中是 program header）。然而，由於我們的應用程式執行映像是純二進位格式（raw binary），所以我們需要手動以固定的位址來處理它
#define USER_BASE 0x1000000
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
#define PROC_EXITED 2
#define SCAUSE_ECALL 8

#define SECTOR_SIZE 512
// Appendix X: virtio-mmio
#define VIRTQ_ENTRY_NUM 16
#define VIRTIO_DEVICE_BLK 2
#define VIRTIO_BLK_PADDR 0x10001000
#define VIRTIO_REG_MAGIC 0x00
#define VIRTIO_REG_VERSION 0x04
#define VIRTIO_REG_DEVICE_ID 0x08
#define VIRTIO_REG_PAGE_SIZE 0x28
#define VIRTIO_REG_QUEUE_SEL 0x30
#define VIRTIO_REG_QUEUE_NUM_MAX 0x34
#define VIRTIO_REG_QUEUE_NUM 0x38
#define VIRTIO_REG_QUEUE_PFN 0x40
#define VIRTIO_REG_QUEUE_READY 0x44
#define VIRTIO_REG_QUEUE_NOTIFY 0x50
#define VIRTIO_REG_DEVICE_STATUS 0x70
#define VIRTIO_REG_DEVICE_CONFIG 0x100
#define VIRTIO_STATUS_ACK 1
#define VIRTIO_STATUS_DRIVER 2
#define VIRTIO_STATUS_DRIVER_OK 4
#define VIRTQ_DESC_F_NEXT 1
#define VIRTQ_DESC_F_WRITE 2
#define VIRTQ_AVAIL_F_NO_INTERRUPT 1
#define VIRTIO_BLK_T_IN 0
#define VIRTIO_BLK_T_OUT 1
#define SSTATUS_SUM (1 << 18)
// Virtqueue Descriptor Table entry.
struct virtq_desc
{
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

// Virtqueue Available Ring.
struct virtq_avail
{
    uint16_t flags;
    uint16_t index;
    uint16_t ring[VIRTQ_ENTRY_NUM]; // The available ring refers to what descriptors we are offering the device
} __attribute__((packed));

// Virtqueue Used Ring entry.
struct virtq_used_elem
{
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

// Virtqueue Used Ring.
struct virtq_used
{
    uint16_t flags;
    uint16_t index;
    struct virtq_used_elem ring[VIRTQ_ENTRY_NUM];
} __attribute__((packed));

// Virtqueue.
struct virtio_virtq
{
    struct virtq_desc descs[VIRTQ_ENTRY_NUM];
    struct virtq_avail avail;
    struct virtq_used used __attribute__((aligned(PAGE_SIZE)));
    int queue_index;
    volatile uint16_t *used_index;
    uint16_t last_used_index;
} __attribute__((packed));

// Virtio-blk request.
struct virtio_blk_req
{
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
    uint8_t data[512];
    uint8_t status;
} __attribute__((packed));
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

#define FILES_MAX 2
#define DISK_MAX_SIZE align_up(sizeof(struct file) * FILES_MAX, SECTOR_SIZE)
// align_up(value, align) 的作用義：向上對齊（Round up）。
// 它會將 value 往上補齊到最接近且大於或等於 value 的 align 整數倍數。
// 硬體傳輸的基本單位：磁碟硬體（virtio-blk）的 I/O 讀寫不是逐個 byte 進行，而是以磁區（Sector，通常為 512 bytes）為最小單位。
// 因為用for (unsigned sector = 0; sector < sizeof(disk) / SECTOR_SIZE; sector++)方式去讀寫
// 假設不用aligned up struct file 1132 bytes, FULEX_MAX=2=>sizeof(struct file) = 2264 (4.x個 Sector)
// 則最後面大小為(0.x個sector資料大小的資料不會被讀到)

// 一個file的格式(這裡就是如hello.txt/meow.txt)
struct tar_header
{
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char type;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
    char data[]; // Array pointing to the data area following the header
                 // (flexible array member)
} __attribute__((packed));

// +----------------+
// |   tar header   |
// +----------------+
// |   file data    |
// +----------------+
// |   tar header   |
// +----------------+
// |   file data    |
// +----------------+
// |      ...       |

struct file
{
    bool in_use;     // Indicates if this file entry is in use
    char name[100];  // File name
    char data[1024]; // File content
    size_t size;     // File size
};

void putchar(char ch);
void switch_context(uint32_t *prev_sp, uint32_t *next_sp);
void yield(void);
paddr_t alloc_pages(uint32_t n);
struct process *create_process(const void *image, size_t image_size);
void delay(void);
void proc_a_entry(void);
void proc_b_entry(void);
void kernel_entry(void);
void kernel_main(void);
void map_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags);
void user_entry(void);
void handle_syscall(struct trap_frame *f);
void virtio_blk_init(void);
void virtq_kick(struct virtio_virtq *vq, int desc_index);
bool virtq_is_busy(struct virtio_virtq *vq);
void read_write_disk(void *buf, unsigned sector, int is_write);
uint32_t virtio_reg_read32(unsigned offset);
uint64_t virtio_reg_read64(unsigned offset);
void virtio_reg_write32(unsigned offset, uint32_t value);
void virtio_reg_fetch_and_or32(unsigned offset, uint32_t value);
struct virtio_virtq *virtq_init(unsigned index);
struct file *fs_lookup(const char *filename);
void fs_flush(void);