#include "smart_fs.h"
#include "smart_core.h"
#include "smart_uart.h"
#include <string.h>

/* FAT12 BPB（BIOS Parameter Block）结构 */
typedef struct __attribute__((packed))
{
    uint8_t jmp[3];
    char oem[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t media_type;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
} fat12_bpb_t;

/* FAT12目录项结构 */
typedef struct __attribute__((packed))
{
    char name[8];           /* 文件名（8.3格式） */
    char ext[3];            /* 扩展名 */
    uint8_t attr;           /* 属性 */
    uint8_t reserved[10];
    uint16_t time;          /* 时间 */
    uint16_t date;          /* 日期 */
    uint16_t first_cluster; /* 起始簇号 */
    uint32_t file_size;     /* 文件大小 */
} fat12_dirent_t;

#define ATTR_READ_ONLY  0x01
#define ATTR_HIDDEN     0x02
#define ATTR_SYSTEM     0x04
#define ATTR_VOLUME_ID  0x08
#define ATTR_DIRECTORY  0x10
#define ATTR_ARCHIVE    0x20

/* 全局变量 */
static smart_block_device_t *fs_device = 0;
static uint32_t fat_start_sector = 0;
static uint32_t root_dir_start_sector = 0;
static uint32_t data_start_sector = 0;
static uint32_t sectors_per_cluster = 0;
static uint8_t fat_cache[512];  /* FAT表缓存（一个扇区） */

/* 读取FAT表项 */
static uint16_t read_fat_entry(uint16_t cluster)
{
    uint32_t fat_offset = cluster * 3 / 2;
    uint32_t fat_sector = fat_start_sector + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;
    
    /* 读取FAT扇区 */
    if (smart_block_read(fs_device, fat_sector, fat_cache, 1) != SMART_BLOCK_OK)
    {
        return 0xFFF;  /* 错误标记 */
    }
    
    /* 解析FAT12表项（12位） */
    if (cluster & 1)
    {
        /* 奇数簇：高12位 */
        return ((fat_cache[entry_offset + 1] << 8) | fat_cache[entry_offset]) >> 4;
    }
    else
    {
        /* 偶数簇：低12位 */
        return ((fat_cache[entry_offset + 1] & 0x0F) << 8) | fat_cache[entry_offset];
    }
}

/* 获取下一个簇号 */
static uint16_t get_next_cluster(uint16_t cluster)
{
    uint16_t next = read_fat_entry(cluster);
    if (next >= 0xFF8)
    {
        return 0xFFFF;  /* 文件结束 */
    }
    return next;
}

/* 文件名转换为8.3格式 */
static void filename_to_83(const char *filename, char *name83)
{
    int i;
    const char *dot = strchr(filename, '.');
    const char *name_part = filename;
    int name_len = dot ? (dot - filename) : strlen(filename);
    
    /* 文件名部分（8字符） */
    for (i = 0; i < 8; i++)
    {
        if (i < name_len && name_part[i] != ' ')
        {
            name83[i] = (name_part[i] >= 'a' && name_part[i] <= 'z') 
                        ? (name_part[i] - 'a' + 'A') : name_part[i];
        }
        else
        {
            name83[i] = ' ';
        }
    }
    
    /* 扩展名部分（3字符） */
    if (dot)
    {
        const char *ext = dot + 1;
        int ext_len = strlen(ext);
        for (i = 0; i < 3; i++)
        {
            if (i < ext_len && ext[i] != ' ')
            {
                name83[8 + i] = (ext[i] >= 'a' && ext[i] <= 'z')
                                ? (ext[i] - 'a' + 'A') : ext[i];
            }
            else
            {
                name83[8 + i] = ' ';
            }
        }
    }
    else
    {
        name83[8] = name83[9] = name83[10] = ' ';
    }
}

/* 在根目录查找文件 */
static smart_fs_status_t find_file_in_root(const char *filename, fat12_dirent_t *dirent)
{
    char name83[11];
    uint8_t sector_buffer[512];
    
    filename_to_83(filename, name83);
    
    /* 遍历根目录（通常占用多个扇区） */
    for (uint32_t i = 0; i < 14; i++)  /* 假设最多14个扇区 */
    {
        if (smart_block_read(fs_device, root_dir_start_sector + i, sector_buffer, 1) != SMART_BLOCK_OK)
        {
            return SMART_FS_ERROR;
        }
        
        /* 每个扇区16个目录项 */
        for (int j = 0; j < 16; j++)
        {
            fat12_dirent_t *entry = (fat12_dirent_t *)(sector_buffer + j * 32);
            
            /* 检查是否为空或已删除 */
            if (entry->name[0] == 0x00)
            {
                break;  /* 目录结束 */
            }
            if (entry->name[0] == 0xE5)
            {
                continue;  /* 已删除 */
            }
            
            /* 比较文件名 */
            if (memcmp(entry->name, name83, 11) == 0)
            {
                *dirent = *entry;
                return SMART_FS_OK;
            }
        }
    }
    
    return SMART_FS_NOT_FOUND;
}

/* 文件系统初始化 */
smart_fs_status_t smart_fs_init(smart_block_device_t *dev)
{
    if (!dev)
    {
        return SMART_FS_INVALID;
    }
    
    fs_device = dev;
    
    /* 读取引导扇区 - 必须使用完整的 512 字节缓冲区 */
    uint8_t sector_buffer[512];
    smart_block_status_t read_status = smart_block_read(dev, 0, sector_buffer, 1);
    
    if (read_status != SMART_BLOCK_OK)
    {
        return SMART_FS_ERROR;
    }
    
    /* 从扇区缓冲区中提取 BPB */
    fat12_bpb_t *bpb = (fat12_bpb_t *)sector_buffer;
    
    /* 验证FAT12签名（简化检查） */
    uint16_t bytes_per_sector = bpb->bytes_per_sector;
    if (bytes_per_sector != 512)
    {
        /* 如果扇区大小为0或无效，说明Flash未格式化 */
        if (bytes_per_sector == 0 || bytes_per_sector == 0xFFFF)
        {
            return SMART_FS_NOT_FOUND;
        }
        return SMART_FS_ERROR;
    }
    
    sectors_per_cluster = bpb->sectors_per_cluster;
    
    /* 计算各区域起始扇区 */
    fat_start_sector = bpb->reserved_sectors;
    root_dir_start_sector = fat_start_sector + (bpb->num_fats * bpb->sectors_per_fat);
    data_start_sector = root_dir_start_sector + ((bpb->root_entries * 32 + 511) / 512);
    
    smart_uart_print("[FS] Initialized: FAT sectors=");
    smart_uart_print_hex32(bpb->sectors_per_fat);
    smart_uart_print(", Root dir=");
    smart_uart_print_hex32(root_dir_start_sector);
    smart_uart_print("\n");
    
    return SMART_FS_OK;
}

/* 格式化文件系统（创建简单的FAT12结构） */
smart_fs_status_t smart_fs_format(smart_block_device_t *dev)
{
    smart_uart_print("[FS] Format function called\n");
    if (!dev)
    {
        smart_uart_print("[FS] Format: invalid device\n");
        return SMART_FS_INVALID;
    }
    
    smart_uart_print("[FS] Format: total sectors = ");
    smart_uart_print_hex32(dev->total_sectors);
    smart_uart_print("\n");
    
    uint8_t sector_buffer[512];
    uint32_t total_sectors = dev->total_sectors;
    
    /* 计算FAT12参数 */
    uint16_t root_entries = 224;  /* 标准FAT12根目录项数 */
    uint8_t sectors_per_cluster = 1;
    uint16_t reserved_sectors = 1;
    uint8_t num_fats = 2;
    
    /* 计算FAT表大小（简化：假设每个簇12位） */
    uint32_t data_sectors = total_sectors - reserved_sectors - (root_entries * 32 / 512);
    uint32_t clusters = data_sectors / sectors_per_cluster;
    uint16_t sectors_per_fat = (clusters * 3 / 2 + 511) / 512 + 1;
    
    /* 1. 创建引导扇区（BPB） */
    memset(sector_buffer, 0, 512);
    fat12_bpb_t *bpb = (fat12_bpb_t *)sector_buffer;
    
    bpb->jmp[0] = 0xEB;
    bpb->jmp[1] = 0x3C;
    bpb->jmp[2] = 0x90;
    memcpy(bpb->oem, "SMARTOS ", 8);
    bpb->bytes_per_sector = 512;
    bpb->sectors_per_cluster = sectors_per_cluster;
    bpb->reserved_sectors = reserved_sectors;
    bpb->num_fats = num_fats;
    bpb->root_entries = root_entries;
    bpb->total_sectors_16 = (total_sectors < 65536) ? total_sectors : 0;
    bpb->media_type = 0xF8;  /* 固定磁盘 */
    bpb->sectors_per_fat = sectors_per_fat;
    bpb->sectors_per_track = 1;
    bpb->num_heads = 1;
    bpb->hidden_sectors = 0;
    bpb->total_sectors_32 = (total_sectors >= 65536) ? total_sectors : 0;
    
    /* 引导扇区结束标记 */
    sector_buffer[510] = 0x55;
    sector_buffer[511] = 0xAA;
    
    smart_uart_print("[FS] Writing boot sector...\n");
    smart_block_status_t write_status = smart_block_write(dev, 0, sector_buffer, 1);
    if (write_status != SMART_BLOCK_OK)
    {
        smart_uart_print("[FS] Failed to write boot sector, status=");
        smart_uart_print_hex32(write_status);
        smart_uart_print("\n");
        return SMART_FS_ERROR;
    }
    smart_uart_print("[FS] Boot sector written\n");
    
    /* 2. 初始化FAT表 */
    smart_uart_print("[FS] Writing FAT tables...\n");
    memset(sector_buffer, 0, 512);
    sector_buffer[0] = 0xF8;  /* 媒体描述符 */
    sector_buffer[1] = 0xFF;
    sector_buffer[2] = 0xFF;  /* 簇0和簇1保留 */
    
    for (uint8_t fat = 0; fat < num_fats; fat++)
    {
        uint32_t fat_start = reserved_sectors + fat * sectors_per_fat;
        smart_uart_print("[FS] Writing FAT ");
        smart_uart_print_hex32(fat);
        smart_uart_print(" (");
        smart_uart_print_hex32(sectors_per_fat);
        smart_uart_print(" sectors)...\n");
        for (uint16_t i = 0; i < sectors_per_fat; i++)
        {
            if (smart_block_write(dev, fat_start + i, sector_buffer, 1) != SMART_BLOCK_OK)
            {
                smart_uart_print("[FS] Failed to write FAT sector ");
                smart_uart_print_hex32(i);
                smart_uart_print("\n");
                return SMART_FS_ERROR;
            }
            if (i == 0)
            {
                memset(sector_buffer, 0, 512);  /* 后续扇区清零 */
            }
        }
    }
    smart_uart_print("[FS] FAT tables written\n");
    
    /* 3. 初始化根目录 */
    smart_uart_print("[FS] Writing root directory...\n");
    memset(sector_buffer, 0, 512);
    uint32_t root_dir_sectors = (root_entries * 32 + 511) / 512;
    uint32_t root_dir_start = reserved_sectors + num_fats * sectors_per_fat;
    
    for (uint32_t i = 0; i < root_dir_sectors; i++)
    {
        if (smart_block_write(dev, root_dir_start + i, sector_buffer, 1) != SMART_BLOCK_OK)
        {
            smart_uart_print("[FS] Failed to write root dir sector ");
            smart_uart_print_hex32(i);
            smart_uart_print("\n");
            return SMART_FS_ERROR;
        }
    }
    smart_uart_print("[FS] Root directory written\n");
    
    smart_uart_print("[FS] Formatted: ");
    smart_uart_print_hex32(total_sectors);
    smart_uart_print(" sectors, FAT=");
    smart_uart_print_hex32(sectors_per_fat);
    smart_uart_print("\n");
    
    /* 验证写入：读回 boot sector 检查 */
    smart_uart_print("[FS] Verifying format...\n");
    uint8_t verify_buffer[512];
    if (smart_block_read(dev, 0, verify_buffer, 1) == SMART_BLOCK_OK)
    {
        fat12_bpb_t *verify_bpb = (fat12_bpb_t *)verify_buffer;
        smart_uart_print("[FS] Verify: bytes_per_sector=");
        smart_uart_print_hex32(verify_bpb->bytes_per_sector);
        smart_uart_print(", signature=0x");
        smart_uart_print_hex32((verify_buffer[510] << 8) | verify_buffer[511]);
        smart_uart_print("\n");
    }
    
    return SMART_FS_OK;
}

/* 打开文件 */
smart_fs_status_t smart_fs_open(const char *filename, smart_file_t *file)
{
    if (!filename || !file)
    {
        return SMART_FS_INVALID;
    }
    
    fat12_dirent_t dirent;
    if (find_file_in_root(filename, &dirent) != SMART_FS_OK)
    {
        return SMART_FS_NOT_FOUND;
    }
    
    /* 初始化文件句柄 */
    file->first_cluster = dirent.first_cluster;
    file->current_cluster = dirent.first_cluster;
    file->file_size = dirent.file_size;
    file->position = 0;
    file->is_dir = (dirent.attr & ATTR_DIRECTORY) != 0;
    
    /* 保存文件名用于close时更新 */
    strncpy(file->filename, filename, sizeof(file->filename) - 1);
    file->filename[sizeof(file->filename) - 1] = '\0';
    
    return SMART_FS_OK;
}

/* 读取文件 */
smart_fs_status_t smart_fs_read(smart_file_t *file, void *buffer, uint32_t size, uint32_t *bytes_read)
{
    if (!file || !buffer || !bytes_read)
    {
        return SMART_FS_INVALID;
    }
    
    if (file->position >= file->file_size)
    {
        *bytes_read = 0;
        return SMART_FS_OK;
    }
    
    uint32_t remaining = file->file_size - file->position;
    if (size > remaining)
    {
        size = remaining;
    }
    
    uint8_t *dst = (uint8_t *)buffer;
    uint32_t total_read = 0;
    uint8_t sector_buffer[512];
    
    /* 计算起始位置所在的簇和扇区 */
    uint32_t bytes_per_cluster = sectors_per_cluster * 512;
    uint32_t cluster_offset = file->position / bytes_per_cluster;
    uint32_t sector_offset = (file->position % bytes_per_cluster) / 512;
    
    /* 定位到起始簇 */
    uint16_t cluster = file->first_cluster;
    for (uint32_t i = 0; i < cluster_offset; i++)
    {
        cluster = get_next_cluster(cluster);
        if (cluster == 0xFFFF)
        {
            break;
        }
    }
    
    /* 读取数据 */
    while (total_read < size && cluster != 0xFFFF)
    {
        uint32_t sector = data_start_sector + (cluster - 2) * sectors_per_cluster + sector_offset;
        
        if (smart_block_read(fs_device, sector, sector_buffer, 1) != SMART_BLOCK_OK)
        {
            return SMART_FS_ERROR;
        }
        
        uint32_t copy_size = 512 - (file->position % 512);
        if (copy_size > (size - total_read))
        {
            copy_size = size - total_read;
        }
        
        memcpy(dst + total_read, sector_buffer + (file->position % 512), copy_size);
        total_read += copy_size;
        file->position += copy_size;
        
        sector_offset++;
        if (sector_offset >= sectors_per_cluster)
        {
            sector_offset = 0;
            cluster = get_next_cluster(cluster);
        }
    }
    
    *bytes_read = total_read;
    return SMART_FS_OK;
}

/* 写入FAT表项 */
static smart_fs_status_t write_fat_entry(uint16_t cluster, uint16_t value)
{
    uint32_t fat_offset = cluster * 3 / 2;
    uint32_t fat_sector = fat_start_sector + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;
    
    /* 读取FAT扇区 */
    if (smart_block_read(fs_device, fat_sector, fat_cache, 1) != SMART_BLOCK_OK)
    {
        return SMART_FS_ERROR;
    }
    
    /* 写入FAT12表项（12位） */
    if (cluster & 1)
    {
        /* 奇数簇：高12位 */
        fat_cache[entry_offset] = (fat_cache[entry_offset] & 0x0F) | ((value & 0x0F) << 4);
        fat_cache[entry_offset + 1] = (value >> 4) & 0xFF;
    }
    else
    {
        /* 偶数簇：低12位 */
        fat_cache[entry_offset] = value & 0xFF;
        fat_cache[entry_offset + 1] = (fat_cache[entry_offset + 1] & 0xF0) | ((value >> 8) & 0x0F);
    }
    
    /* 写回FAT扇区（两个FAT表都要更新） */
    if (smart_block_write(fs_device, fat_sector, fat_cache, 1) != SMART_BLOCK_OK)
    {
        return SMART_FS_ERROR;
    }
    
    /* 更新第二个FAT表 */
    uint32_t fat2_sector = fat_sector + (512 / 512);  /* 假设每个FAT占用相同扇区数 */
    if (smart_block_write(fs_device, fat2_sector, fat_cache, 1) != SMART_BLOCK_OK)
    {
        return SMART_FS_ERROR;
    }
    
    return SMART_FS_OK;
}

/* 分配一个空闲簇 */
static uint16_t allocate_cluster(void)
{
    /* 从簇2开始查找（0和1是保留的） */
    for (uint16_t cluster = 2; cluster < 0xFF0; cluster++)
    {
        uint16_t value = read_fat_entry(cluster);
        if (value == 0)
        {
            /* 找到空闲簇，标记为文件结束 */
            write_fat_entry(cluster, 0xFFF);
            return cluster;
        }
    }
    return 0;  /* 没有空闲簇 */
}

/* 写入文件（简化实现：仅支持追加） */
smart_fs_status_t smart_fs_write(smart_file_t *file, const void *buffer, uint32_t size, uint32_t *bytes_written)
{
    if (!file || !buffer || !bytes_written)
    {
        return SMART_FS_INVALID;
    }
    
    const uint8_t *src = (const uint8_t *)buffer;
    uint32_t total_written = 0;
    uint8_t sector_buffer[512];
    
    /* 如果文件为空，分配第一个簇 */
    if (file->first_cluster == 0)
    {
        file->first_cluster = allocate_cluster();
        if (file->first_cluster == 0)
        {
            return SMART_FS_FULL;
        }
        file->current_cluster = file->first_cluster;
    }
    
    /* 定位到文件末尾 */
    uint16_t cluster = file->first_cluster;
    uint32_t cluster_offset = file->file_size / (sectors_per_cluster * 512);
    
    for (uint32_t i = 0; i < cluster_offset; i++)
    {
        uint16_t next = get_next_cluster(cluster);
        if (next == 0xFFFF)
        {
            /* 需要分配新簇 */
            uint16_t new_cluster = allocate_cluster();
            if (new_cluster == 0)
            {
                return SMART_FS_FULL;
            }
            write_fat_entry(cluster, new_cluster);
            cluster = new_cluster;
        }
        else
        {
            cluster = next;
        }
    }
    
    /* 写入数据 */
    while (total_written < size)
    {
        uint32_t offset_in_cluster = file->file_size % (sectors_per_cluster * 512);
        uint32_t sector_in_cluster = offset_in_cluster / 512;
        uint32_t offset_in_sector = offset_in_cluster % 512;
        
        uint32_t sector = data_start_sector + (cluster - 2) * sectors_per_cluster + sector_in_cluster;
        
        /* 读取当前扇区（如果不是从头开始写） */
        if (offset_in_sector > 0 || (size - total_written) < 512)
        {
            if (smart_block_read(fs_device, sector, sector_buffer, 1) != SMART_BLOCK_OK)
            {
                return SMART_FS_ERROR;
            }
        }
        
        /* 复制数据到扇区缓冲区 */
        uint32_t copy_size = 512 - offset_in_sector;
        if (copy_size > (size - total_written))
        {
            copy_size = size - total_written;
        }
        
        memcpy(sector_buffer + offset_in_sector, src + total_written, copy_size);
        
        /* 写回扇区 */
        if (smart_block_write(fs_device, sector, sector_buffer, 1) != SMART_BLOCK_OK)
        {
            return SMART_FS_ERROR;
        }
        
        total_written += copy_size;
        file->file_size += copy_size;
        file->position += copy_size;
        
        /* 如果当前簇已满，分配新簇 */
        if ((file->file_size % (sectors_per_cluster * 512)) == 0 && total_written < size)
        {
            uint16_t new_cluster = allocate_cluster();
            if (new_cluster == 0)
            {
                *bytes_written = total_written;
                return SMART_FS_FULL;
            }
            write_fat_entry(cluster, new_cluster);
            cluster = new_cluster;
        }
    }
    
    *bytes_written = total_written;
    return SMART_FS_OK;
}

/* 更新目录项 */
smart_fs_status_t smart_fs_update_file_info(const char *filename, uint16_t first_cluster, uint32_t file_size)
{
    char name83[11];
    uint8_t sector_buffer[512];
    
    filename_to_83(filename, name83);
    
    /* 遍历根目录查找文件 */
    for (uint32_t i = 0; i < 14; i++)
    {
        if (smart_block_read(fs_device, root_dir_start_sector + i, sector_buffer, 1) != SMART_BLOCK_OK)
        {
            return SMART_FS_ERROR;
        }
        
        for (int j = 0; j < 16; j++)
        {
            fat12_dirent_t *entry = (fat12_dirent_t *)(sector_buffer + j * 32);
            
            if (entry->name[0] == 0x00)
            {
                return SMART_FS_NOT_FOUND;
            }
            if (entry->name[0] == 0xE5)
            {
                continue;
            }
            
            /* 比较文件名 */
            if (memcmp(entry->name, name83, 11) == 0)
            {
                /* 更新目录项 */
                entry->first_cluster = first_cluster;
                entry->file_size = file_size;
                
                /* 写回扇区 */
                if (smart_block_write(fs_device, root_dir_start_sector + i, sector_buffer, 1) != SMART_BLOCK_OK)
                {
                    return SMART_FS_ERROR;
                }
                
                return SMART_FS_OK;
            }
        }
    }
    
    return SMART_FS_NOT_FOUND;
}

/* 关闭文件 */
smart_fs_status_t smart_fs_close(smart_file_t *file)
{
    if (!file)
    {
        return SMART_FS_INVALID;
    }
    
    /* 更新目录项中的文件大小和起始簇 */
    return smart_fs_update_file_info(file->filename, file->first_cluster, file->file_size);
}

/* 列出目录 */
smart_fs_status_t smart_fs_list_dir(const char *dirname)
{
    (void)dirname;
    uint8_t sector_buffer[512];
    
    if (!fs_device || root_dir_start_sector == 0)
    {
        smart_uart_print("[FS] File system not initialized\n");
        return SMART_FS_ERROR;
    }
    
    smart_uart_print("[FS] Root directory:\n");
    
    for (uint32_t i = 0; i < 14; i++)
    {
        if (smart_block_read(fs_device, root_dir_start_sector + i, sector_buffer, 1) != SMART_BLOCK_OK)
        {
            smart_uart_print("[FS] Failed to read root dir sector ");
            smart_uart_print_hex32(i);
            smart_uart_print("\n");
            break;
        }
        
        for (int j = 0; j < 16; j++)
        {
            fat12_dirent_t *entry = (fat12_dirent_t *)(sector_buffer + j * 32);
            
            if (entry->name[0] == 0x00)
            {
                return SMART_FS_OK;  /* 目录结束 */
            }
            if (entry->name[0] == 0xE5)
            {
                continue;  /* 已删除 */
            }
            
            /* 打印文件名 */
            for (int k = 0; k < 8; k++)
            {
                if (entry->name[k] != ' ')
                {
                    smart_uart_putc(entry->name[k]);
                }
            }
            if (entry->ext[0] != ' ')
            {
                smart_uart_putc('.');
                for (int k = 0; k < 3; k++)
                {
                    if (entry->ext[k] != ' ')
                    {
                        smart_uart_putc(entry->ext[k]);
                    }
                }
            }
            smart_uart_print(" (");
            smart_uart_print_hex32(entry->file_size);
            smart_uart_print(" bytes)\n");
        }
    }
    
    return SMART_FS_OK;
}


/* 创建文件 */
smart_fs_status_t smart_fs_create(const char *filename)
{
    if (!fs_device || root_dir_start_sector == 0)
    {
        smart_uart_print("[FS] Create: device not initialized\n");
        return SMART_FS_ERROR;
    }
    
    /* 检查文件名长度 */
    if (!filename || strlen(filename) == 0 || strlen(filename) > 12)
    {
        smart_uart_print("[FS] Create: invalid filename\n");
        return SMART_FS_INVALID;
    }
    
    /* 分离文件名和扩展名 */
    char name[9] = {0};
    char ext[4] = {0};
    const char *dot = strchr(filename, '.');
    
    if (dot)
    {
        size_t name_len = dot - filename;
        if (name_len > 8) name_len = 8;
        memcpy(name, filename, name_len);
        
        size_t ext_len = strlen(dot + 1);
        if (ext_len > 3) ext_len = 3;
        memcpy(ext, dot + 1, ext_len);
    }
    else
    {
        size_t name_len = strlen(filename);
        if (name_len > 8) name_len = 8;
        memcpy(name, filename, name_len);
    }
    
    /* 转换为大写 */
    for (int i = 0; i < 8 && name[i]; i++)
    {
        if (name[i] >= 'a' && name[i] <= 'z')
            name[i] = name[i] - 'a' + 'A';
    }
    for (int i = 0; i < 3 && ext[i]; i++)
    {
        if (ext[i] >= 'a' && ext[i] <= 'z')
            ext[i] = ext[i] - 'a' + 'A';
    }
    
    uint8_t sector_buffer[512];
    
    /* 查找空闲目录项 */
    for (uint32_t i = 0; i < 14; i++)
    {
        smart_block_status_t status = smart_block_read(fs_device, root_dir_start_sector + i, sector_buffer, 1);
        if (status != SMART_BLOCK_OK)
        {
            smart_uart_print("[FS] Create: read dir sector failed, status=");
            smart_uart_print_hex32(status);
            smart_uart_print("\n");
            return SMART_FS_ERROR;
        }
        
        for (int j = 0; j < 16; j++)
        {
            fat12_dirent_t *entry = (fat12_dirent_t *)(sector_buffer + j * 32);
            
            /* 找到空闲项 */
            if (entry->name[0] == 0x00 || entry->name[0] == 0xE5)
            {
                /* 填充目录项 */
                memset(entry, 0, sizeof(fat12_dirent_t));
                memset(entry->name, ' ', 8);
                memset(entry->ext, ' ', 3);
                memcpy(entry->name, name, strlen(name));
                if (strlen(ext) > 0)
                {
                    memcpy(entry->ext, ext, strlen(ext));
                }
                entry->attr = 0x20;  /* Archive */
                entry->first_cluster = 0;  /* 空文件，无簇 */
                entry->file_size = 0;
                
                /* 写回扇区 */
                if (smart_block_write(fs_device, root_dir_start_sector + i, sector_buffer, 1) != SMART_BLOCK_OK)
                {
                    return SMART_FS_ERROR;
                }
                
                return SMART_FS_OK;
            }
        }
    }
    
    return SMART_FS_FULL;  /* 目录已满 */
}

/* 删除文件 */
smart_fs_status_t smart_fs_delete(const char *filename)
{
    if (!fs_device || root_dir_start_sector == 0)
    {
        return SMART_FS_ERROR;
    }
    
    /* 分离文件名和扩展名 */
    char name[9] = {0};
    char ext[4] = {0};
    const char *dot = strchr(filename, '.');
    
    if (dot)
    {
        size_t name_len = dot - filename;
        if (name_len > 8) name_len = 8;
        memcpy(name, filename, name_len);
        
        size_t ext_len = strlen(dot + 1);
        if (ext_len > 3) ext_len = 3;
        memcpy(ext, dot + 1, ext_len);
    }
    else
    {
        size_t name_len = strlen(filename);
        if (name_len > 8) name_len = 8;
        memcpy(name, filename, name_len);
    }
    
    /* 转换为大写 */
    for (int i = 0; i < 8 && name[i]; i++)
    {
        if (name[i] >= 'a' && name[i] <= 'z')
            name[i] = name[i] - 'a' + 'A';
    }
    for (int i = 0; i < 3 && ext[i]; i++)
    {
        if (ext[i] >= 'a' && ext[i] <= 'z')
            ext[i] = ext[i] - 'a' + 'A';
    }
    
    uint8_t sector_buffer[512];
    
    /* 查找文件 */
    for (uint32_t i = 0; i < 14; i++)
    {
        if (smart_block_read(fs_device, root_dir_start_sector + i, sector_buffer, 1) != SMART_BLOCK_OK)
        {
            return SMART_FS_ERROR;
        }
        
        for (int j = 0; j < 16; j++)
        {
            fat12_dirent_t *entry = (fat12_dirent_t *)(sector_buffer + j * 32);
            
            if (entry->name[0] == 0x00)
            {
                return SMART_FS_NOT_FOUND;
            }
            if (entry->name[0] == 0xE5)
            {
                continue;
            }
            
            /* 比较文件名 */
            int name_match = 1;
            for (int k = 0; k < 8; k++)
            {
                char c1 = (k < (int)strlen(name)) ? name[k] : ' ';
                if (entry->name[k] != c1)
                {
                    name_match = 0;
                    break;
                }
            }
            
            int ext_match = 1;
            for (int k = 0; k < 3; k++)
            {
                char c1 = (k < (int)strlen(ext)) ? ext[k] : ' ';
                if (entry->ext[k] != c1)
                {
                    ext_match = 0;
                    break;
                }
            }
            
            if (name_match && ext_match)
            {
                /* 标记为已删除 */
                entry->name[0] = 0xE5;
                
                /* TODO: 释放FAT链中的簇 */
                
                /* 写回扇区 */
                if (smart_block_write(fs_device, root_dir_start_sector + i, sector_buffer, 1) != SMART_BLOCK_OK)
                {
                    return SMART_FS_ERROR;
                }
                
                return SMART_FS_OK;
            }
        }
    }
    
    return SMART_FS_NOT_FOUND;
}
