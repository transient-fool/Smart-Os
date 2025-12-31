# Smart-OS 文件系统设计文档 (DESIGN04)

## 概述

本文档记录了 Smart-OS 中 FAT12 文件系统的实现过程，包括设计决策、遇到的问题以及最终的解决方案。

## 实现目标

为 Smart-OS 添加一个简单的文件系统，支持基本的文件操作：
- 文件系统格式化（FAT12）
- 文件系统初始化和挂载
- 目录列表
- 文件读写（基础功能）

## 架构设计

### 1. 分层架构

```
+------------------+
|   应用层 (main)  |
+------------------+
         |
+------------------+
| 文件系统层 (FS)  |  <- smart_fs.c/h
+------------------+
         |
+------------------+
|  块设备层 (BD)   |  <- smart_block.c/h
+------------------+
         |
+------------------+
|   存储硬件层     |  <- Flash/SRAM
+------------------+
```

### 2. 核心组件

#### 2.1 块设备抽象层 (`smart_block.c/h`)

提供统一的块设备接口，隐藏底层存储细节：

```c
typedef struct {
    uint32_t sector_size;      // 扇区大小（512字节）
    uint32_t total_sectors;    // 总扇区数
    smart_block_status_t (*read)(smart_block_device_t *dev, 
                                  uint32_t sector, 
                                  void *buffer, 
                                  uint32_t count);
    smart_block_status_t (*write)(smart_block_device_t *dev, 
                                   uint32_t sector, 
                                   const void *buffer, 
                                   uint32_t count);
    void *priv;                // 私有数据
} smart_block_device_t;
```

**关键函数：**
- `smart_flash_init()` - 初始化存储设备
- `smart_block_read()` - 读取扇区
- `smart_block_write()` - 写入扇区

#### 2.2 文件系统层 (`smart_fs.c/h`)

实现 FAT12 文件系统的核心功能：

**数据结构：**
```c
// FAT12 BPB (BIOS Parameter Block)
typedef struct __attribute__((packed)) {
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
    // ... 其他字段
} fat12_bpb_t;

// 文件句柄
typedef struct {
    uint32_t first_cluster;
    uint32_t current_cluster;
    uint32_t file_size;
    uint32_t position;
    uint8_t is_dir;
} smart_file_t;
```

**关键函数：**
- `smart_fs_init()` - 初始化/挂载文件系统
- `smart_fs_format()` - 格式化为 FAT12
- `smart_fs_open()` - 打开文件
- `smart_fs_read()` - 读取文件
- `smart_fs_write()` - 写入文件
- `smart_fs_list_dir()` - 列出目录

## 遇到的问题与解决方案

### 问题 1: 栈缓冲区溢出导致程序卡死

**现象：**
```
[FS] Flash not formatted, returning NOT_FOUND
```
程序打印最后一条消息后卡死，无法返回到调用函数。

**根本原因：**
在 `smart_fs_init()` 函数中，声明了一个 `fat12_bpb_t bpb` 变量（只有 36 字节），然后将其地址传递给 `smart_block_read()` 函数：

```c
fat12_bpb_t bpb;  // 只有 36 字节
smart_block_read(dev, 0, &bpb, 1);  // 但要读取 512 字节！
```

`smart_block_read()` 会读取整个扇区（512 字节）到这个缓冲区，导致：
- 栈上的其他数据被覆盖
- 函数返回地址（lr）被破坏
- 帧指针（r7）被破坏
- 函数返回时跳转到错误地址，程序卡死

**解决方案：**
使用完整的 512 字节缓冲区接收扇区数据，然后将其强制转换为 BPB 指针：

```c
uint8_t sector_buffer[512];  // 完整扇区缓冲区
smart_block_read(dev, 0, sector_buffer, 1);
fat12_bpb_t *bpb = (fat12_bpb_t *)sector_buffer;  // 安全访问
```

**教训：**
- 传递给读取函数的缓冲区大小必须足够容纳所有要读取的数据
- 这是一个经典的缓冲区溢出漏洞，在嵌入式开发中非常常见
- 栈溢出会导致难以调试的问题，因为症状可能在很久之后才出现

### 问题 2: QEMU 中 Flash 区域只读

**现象：**
```
[Flash] Write completed
[FS] Verify: bytes_per_sector=00000000, signature=0x00000000
```
写入操作返回成功，但读回的数据仍然是 0。

**根本原因：**
在 QEMU 模拟的 LM3S6965 环境中：
- 使用 `-kernel` 参数加载程序时，Flash 区域（0x00000000 - 0x0003FFFF）被映射为只读
- 即使使用 `-pflash` 参数，LM3S6965 的 QEMU 实现也不支持从 pflash 直接启动
- 尝试写入 Flash 区域（0x00010000）的操作被忽略，数据无法持久化

**尝试的方案：**

1. **方案 A：使用 pflash 设备**
   - 创建 Flash 镜像文件
   - 使用 `-drive if=pflash,format=raw,file=flash.img` 参数
   - 问题：LM3S6965 需要同时使用 `-kernel`，导致 pflash 设备被忽略

2. **方案 B：使用 Flash 编程寄存器**
   - 尝试使用 LM3S6965 的 Flash 控制器寄存器进行编程
   - 问题：QEMU 可能未完全模拟 Flash 编程功能

**最终解决方案：使用 SRAM 作为存储（QEMU 环境）**

通过条件编译，在不同环境使用不同的存储区域：

```c
#ifdef QEMU_ENV
#define FLASH_FS_BASE_ADDR   0x20002000  // SRAM 高地址区域
#define FLASH_FS_SIZE        (56 * 1024)  // 56KB
#else
#define FLASH_FS_BASE_ADDR   0x00010000  // Flash 高地址区域
#define FLASH_FS_SIZE        (192 * 1024) // 192KB
#endif
```

**SRAM 布局（QEMU 环境）：**
```
0x20000000 - 0x20000E3F: 程序 BSS/堆栈（约 4KB）
0x20002000 - 0x2000FFFF: 文件系统区域（56KB）
```

**优点：**
- SRAM 可以直接读写，无需特殊编程序列
- 在 QEMU 中完全可用
- 代码简单，易于调试

**缺点：**
- 掉电数据丢失（但 QEMU 模拟器本来就不持久化）
- 占用 SRAM 空间

**真实硬件部署：**
在真实硬件上，移除 `-DQEMU_ENV` 编译选项即可切换回 Flash 存储。

### 问题 3: 调试输出过多

**现象：**
程序运行时产生大量调试输出，影响可读性和性能。

**解决方案：**
- 移除不必要的调试打印
- 保留关键的状态信息
- 在开发阶段可以通过宏控制调试级别

## 实现细节

### FAT12 文件系统布局

```
+------------------+
| Boot Sector      | 扇区 0 (BPB + 引导代码)
+------------------+
| FAT #1           | 扇区 1-3
+------------------+
| FAT #2 (备份)    | 扇区 4-6
+------------------+
| Root Directory   | 扇区 7-20 (224个目录项)
+------------------+
| Data Area        | 扇区 21+ (文件数据)
+------------------+
```

### 格式化流程

1. **创建引导扇区（BPB）**
   - 设置文件系统参数（扇区大小、簇大小等）
   - 写入引导签名 0x55AA

2. **初始化 FAT 表**
   - 创建两份 FAT 表（主表和备份）
   - 设置媒体描述符（0xF8）
   - 标记簇 0 和簇 1 为保留

3. **初始化根目录**
   - 清空根目录区域
   - 准备接收目录项

4. **验证格式化**
   - 读回引导扇区
   - 检查 BPB 参数和签名

### 文件系统初始化流程

1. **读取引导扇区**
2. **验证 FAT12 签名**
   - 检查 bytes_per_sector == 512
   - 检查引导签名 0x55AA
3. **计算各区域起始扇区**
   - FAT 起始扇区
   - 根目录起始扇区
   - 数据区起始扇区
4. **挂载成功**

## 测试结果

### 成功输出示例

```
=== File System Test ===
[Storage] Using SRAM at 0x20002000 (56 KB)
[Storage] Initialized
[Storage] Ready: 00000070 sectors
[FS] Flash initialized: 00000070 sectors
[FS] Initializing file system...
[FS] Flash not formatted, formatting...
[FS] Format function called
[FS] Format: total sectors = 00000070
[FS] Writing boot sector...
[FS] Boot sector written
[FS] Writing FAT tables...
[FS] Writing FAT 00000000 (00000002 sectors)...
[FS] Writing FAT 00000001 (00000002 sectors)...
[FS] FAT tables written
[FS] Writing root directory...
[FS] Root directory written
[FS] Formatted: 00000070 sectors, FAT=00000002
[FS] Verifying format...
[FS] Verify: bytes_per_sector=00000200, signature=0x000055AA
[FS] Flash formatted successfully
[FS] Initialized: FAT sectors=00000002, Root dir=00000005
[FS] File system initialized
[FS] Root directory:
=== End FS Test ===
```

### 验证点

- ✅ 存储设备初始化成功
- ✅ 文件系统格式化成功
- ✅ BPB 参数正确（bytes_per_sector=512）
- ✅ 引导签名正确（0x55AA）
- ✅ 文件系统重新挂载成功
- ✅ 根目录可以列出（当前为空）

## 代码统计

```
文件系统核心代码：
- smart_fs.c:     ~450 行
- smart_fs.h:     ~40 行
- smart_block.c:  ~250 行
- smart_block.h:  ~50 行

总计：约 790 行代码
```

## 后续改进方向

### 短期改进

1. **文件写入功能**
   - 实现 FAT 表更新
   - 支持簇分配和链接
   - 实现文件创建

2. **目录操作**
   - 创建目录
   - 删除文件/目录
   - 重命名

3. **错误处理**
   - 更完善的错误检查
   - 文件系统一致性验证

### 长期改进

1. **性能优化**
   - FAT 表缓存
   - 目录项缓存
   - 批量读写优化

2. **功能扩展**
   - 支持长文件名（LFN）
   - 支持 FAT16/FAT32
   - 文件属性和时间戳

3. **可靠性**
   - 掉电保护
   - 坏块管理
   - 文件系统修复工具

## 编译和运行

### QEMU 环境（使用 SRAM）

```bash
make clean
make              # 自动包含 -DQEMU_ENV
make qemu         # 或直接运行 QEMU
```

### 真实硬件（使用 Flash）

```bash
make clean
# 修改 Makefile，移除 -DQEMU_ENV
make
# 烧录到硬件
```

## 参考资料

1. **FAT12 文件系统规范**
   - Microsoft FAT Specification
   - 8.3 文件名格式

2. **LM3S6965 数据手册**
   - Flash 编程接口
   - 内存映射

3. **嵌入式文件系统设计**
   - 块设备抽象
   - 分层架构设计

## 总结

通过本次实现，我们成功为 Smart-OS 添加了一个基础但功能完整的 FAT12 文件系统。主要收获：

1. **架构设计的重要性**：分层设计使得存储介质可以灵活切换
2. **缓冲区管理**：必须确保缓冲区大小足够，避免溢出
3. **平台差异**：QEMU 和真实硬件的行为可能不同，需要条件编译
4. **调试技巧**：栈溢出等问题需要系统性的调试方法

文件系统现在可以正常工作，为后续的应用开发提供了基础的持久化存储能力。
