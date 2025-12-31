#include "smart_block.h"
#include "smart_core.h"
#include "smart_uart.h"

#define SECTOR_SIZE 512

/* ========== 内置 Flash 驱动实现 ========== */
/* 使用 lm3s6965evb 内置 Flash 的未使用部分作为文件系统存储
 * Flash 是 NOR Flash，可以直接通过内存映射访问
 */

/* 文件系统存储配置
 * 
 * QEMU 环境：使用 SRAM（Flash 是只读的）
 *   0x20000000 - 0x20000E3F: 程序使用的 BSS/堆栈（约 4KB）
 *   0x20004000 - 0x2000FFFF: 文件系统区域（48KB，可用）
 * 
 * 真实硬件：使用 Flash
 *   0x00010000 - 0x0003FFFF: 文件系统区域 (192KB)
 */
#ifdef QEMU_ENV
#define FLASH_FS_BASE_ADDR       0x20004000  /* SRAM 高地址区域，留出16KB给程序 */
#define FLASH_FS_SIZE            (48 * 1024)  /* 48KB */
#else
#define FLASH_FS_BASE_ADDR       0x00010000  /* Flash 高地址区域 */
#define FLASH_FS_SIZE            (192 * 1024)  /* 192KB */
#endif

/* LM3S6965 Flash 控制寄存器 */
#define FLASH_FMA    (*(volatile uint32_t *)0x400FD000)  /* Flash Memory Address */
#define FLASH_FMD    (*(volatile uint32_t *)0x400FD004)  /* Flash Memory Data */
#define FLASH_FMC    (*(volatile uint32_t *)0x400FD008)  /* Flash Memory Control */
#define FLASH_FCRIS  (*(volatile uint32_t *)0x400FD00C)  /* Flash Controller Raw Interrupt Status */
#define FLASH_FCIM   (*(volatile uint32_t *)0x400FD010)  /* Flash Controller Interrupt Mask */
#define FLASH_FCMISC (*(volatile uint32_t *)0x400FD014)  /* Flash Controller Masked Interrupt Status and Clear */

#define FLASH_FMC_WRKEY  0xA4420000  /* Flash write key */
#define FLASH_FMC_COMT   0x00000008  /* Commit Register Value */
#define FLASH_FMC_MERASE 0x00000004  /* Mass Erase Flash Memory */
#define FLASH_FMC_ERASE  0x00000002  /* Erase a Page of Flash Memory */
#define FLASH_FMC_WRITE  0x00000001  /* Write a Word into Flash Memory */

static uint32_t flash_base = 0;
static uint32_t flash_size = 0;

/* Flash 私有数据 */
typedef struct
{
    uint32_t initialized;
    uint32_t total_sectors;
} flash_priv_t;

/* 检测/初始化存储设备 */
static uint32_t detect_flash(void)
{
    uint32_t addr = FLASH_FS_BASE_ADDR;
    
#ifdef QEMU_ENV
    smart_uart_print("[Storage] Using SRAM at 0x");
#else
    smart_uart_print("[Storage] Using Flash at 0x");
#endif
    smart_uart_print_hex32(addr);
    smart_uart_print(" (");
    smart_uart_print_hex32(FLASH_FS_SIZE / 1024);
    smart_uart_print(" KB)\n");
    
    /* 初始化为 0xFF（模拟未格式化状态） */
    volatile uint8_t *storage = (volatile uint8_t *)addr;
    for (uint32_t i = 0; i < FLASH_FS_SIZE; i++)
    {
        storage[i] = 0xFF;
    }
    
    smart_uart_print("[Storage] Initialized\n");
    
    return addr;
}

/* Flash 读操作（直接内存访问） */
static smart_block_status_t flash_read(smart_block_device_t *dev,
                                       uint32_t sector,
                                       void *buffer,
                                       uint32_t count)
{
    flash_priv_t *priv = (flash_priv_t *)dev->priv;
    
    if (!priv || !priv->initialized || !buffer || count == 0)
    {
        return SMART_BLOCK_INVALID;
    }
    
    if (sector + count > priv->total_sectors)
    {
        return SMART_BLOCK_ERROR;
    }
    
    if (flash_base == 0)
    {
        return SMART_BLOCK_ERROR;
    }
    
    uint32_t offset = sector * SECTOR_SIZE;
    uint32_t bytes = count * SECTOR_SIZE;
    
    if (offset + bytes > flash_size)
    {
        return SMART_BLOCK_ERROR;
    }
    
    /* 直接从内存映射地址读取 */
    volatile uint8_t *src = (volatile uint8_t *)(flash_base + offset);
    uint8_t *dst = (uint8_t *)buffer;
    
    for (uint32_t i = 0; i < bytes; i++)
    {
        dst[i] = src[i];
    }
    
    return SMART_BLOCK_OK;
}

/* Flash 写操作 */
static smart_block_status_t flash_write(smart_block_device_t *dev,
                                        uint32_t sector,
                                        const void *buffer,
                                        uint32_t count)
{
    flash_priv_t *priv = (flash_priv_t *)dev->priv;
    
    if (!priv || !priv->initialized || !buffer || count == 0)
    {
        return SMART_BLOCK_INVALID;
    }
    
    if (sector + count > priv->total_sectors)
    {
        return SMART_BLOCK_ERROR;
    }
    
    if (flash_base == 0)
    {
        return SMART_BLOCK_ERROR;
    }
    
    uint32_t offset = sector * SECTOR_SIZE;
    uint32_t bytes = count * SECTOR_SIZE;
    
    if (offset + bytes > flash_size)
    {
        return SMART_BLOCK_ERROR;
    }
    
    /* 直接内存写入（SRAM 或支持的 Flash） */
    volatile uint8_t *dst = (volatile uint8_t *)(flash_base + offset);
    const uint8_t *src = (const uint8_t *)buffer;
    
    for (uint32_t i = 0; i < bytes; i++)
    {
        dst[i] = src[i];
    }
    
    return SMART_BLOCK_OK;
}

/* 块设备通用读接口 */
smart_block_status_t smart_block_read(smart_block_device_t *dev,
                                      uint32_t sector,
                                      void *buffer,
                                      uint32_t count)
{
    if (!dev || !dev->read)
    {
        return SMART_BLOCK_INVALID;
    }
    
    return dev->read(dev, sector, buffer, count);
}

/* 块设备通用写接口 */
smart_block_status_t smart_block_write(smart_block_device_t *dev,
                                       uint32_t sector,
                                       const void *buffer,
                                       uint32_t count)
{
    if (!dev || !dev->write)
    {
        return SMART_BLOCK_INVALID;
    }
    
    return dev->write(dev, sector, buffer, count);
}

/* 初始化存储设备 */
smart_block_device_t *smart_flash_init(void)
{
    static smart_block_device_t flash_dev;
    static flash_priv_t flash_priv;
    
    /* 检测/初始化存储设备 */
    flash_base = detect_flash();
    if (flash_base == 0)
    {
        smart_uart_print("[Storage] Init failed\n");
        return 0;
    }
    
    flash_size = FLASH_FS_SIZE;
    flash_priv.initialized = 1;
    flash_priv.total_sectors = flash_size / SECTOR_SIZE;
    
    flash_dev.sector_size = SECTOR_SIZE;
    flash_dev.total_sectors = flash_priv.total_sectors;
    flash_dev.base_address = flash_base;
    flash_dev.read = flash_read;
    flash_dev.write = flash_write;
    flash_dev.priv = &flash_priv;
    
    smart_uart_print("[Storage] Ready: ");
    smart_uart_print_hex32(flash_priv.total_sectors);
    smart_uart_print(" sectors\n");
    
    return &flash_dev;
}

void smart_flash_deinit(smart_block_device_t *dev)
{
    (void)dev;
    /* Flash 无需特殊清理 */
}
