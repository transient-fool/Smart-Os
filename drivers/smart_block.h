#ifndef __SMART_BLOCK_H__
#define __SMART_BLOCK_H__

#include <stdint.h>
#include <stddef.h>

/* 块设备状态 */
typedef enum
{
    SMART_BLOCK_OK = 0,
    SMART_BLOCK_ERROR,
    SMART_BLOCK_NOT_READY,
    SMART_BLOCK_INVALID
} smart_block_status_t;

/* 块设备操作接口 */
typedef struct smart_block_device
{
    /* 设备信息 */
    uint32_t sector_size;      /* 扇区大小（通常512字节） */
    uint32_t total_sectors;    /* 总扇区数 */
    uint32_t base_address;     /* 设备基地址 */
    
    /* 操作函数 */
    smart_block_status_t (*read)(struct smart_block_device *dev,
                                 uint32_t sector,
                                 void *buffer,
                                 uint32_t count);
    smart_block_status_t (*write)(struct smart_block_device *dev,
                                  uint32_t sector,
                                  const void *buffer,
                                  uint32_t count);
    
    /* 设备私有数据 */
    void *priv;
} smart_block_device_t;

/* 块设备API */
smart_block_status_t smart_block_read(smart_block_device_t *dev,
                                      uint32_t sector,
                                      void *buffer,
                                      uint32_t count);

smart_block_status_t smart_block_write(smart_block_device_t *dev,
                                       uint32_t sector,
                                       const void *buffer,
                                       uint32_t count);

/* CFI Flash 驱动实现（真实硬件存储） */
smart_block_device_t *smart_flash_init(void);
void smart_flash_deinit(smart_block_device_t *dev);

/* 获取全局Flash设备（由应用层提供） */
smart_block_device_t *smart_get_flash_device(void);

#endif

