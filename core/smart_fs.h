#ifndef __SMART_FS_H__
#define __SMART_FS_H__

#include <stdint.h>
#include <stddef.h>
#include "smart_block.h"

/* 文件系统状态 */
typedef enum
{
    SMART_FS_OK = 0,
    SMART_FS_ERROR,
    SMART_FS_NOT_FOUND,
    SMART_FS_INVALID,
    SMART_FS_FULL
} smart_fs_status_t;

/* 文件句柄 */
typedef struct
{
    uint32_t first_cluster;    /* 起始簇号 */
    uint32_t current_cluster;   /* 当前簇号 */
    uint32_t file_size;          /* 文件大小 */
    uint32_t position;           /* 当前读写位置 */
    uint8_t is_dir;              /* 是否为目录 */
    char filename[12];           /* 文件名（8.3格式，用于close时更新） */
} smart_file_t;

/* 文件系统初始化 */
smart_fs_status_t smart_fs_init(smart_block_device_t *dev);

/* 格式化文件系统（创建FAT12结构） */
smart_fs_status_t smart_fs_format(smart_block_device_t *dev);

/* 文件操作 */
smart_fs_status_t smart_fs_open(const char *filename, smart_file_t *file);
smart_fs_status_t smart_fs_read(smart_file_t *file, void *buffer, uint32_t size, uint32_t *bytes_read);
smart_fs_status_t smart_fs_write(smart_file_t *file, const void *buffer, uint32_t size, uint32_t *bytes_written);
smart_fs_status_t smart_fs_close(smart_file_t *file);

/* 目录操作 */
smart_fs_status_t smart_fs_list_dir(const char *dirname);

/* 文件创建和删除 */
smart_fs_status_t smart_fs_create(const char *filename);
smart_fs_status_t smart_fs_delete(const char *filename);

/* 更新文件信息 */
smart_fs_status_t smart_fs_update_file_info(const char *filename, uint16_t first_cluster, uint32_t file_size);

#endif

