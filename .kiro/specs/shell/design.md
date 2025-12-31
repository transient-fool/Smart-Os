# Shell 命令行界面设计文档

## 概述

本文档描述 Smart-OS Shell 的详细设计，包括架构、数据结构、接口和实现策略。

## 架构设计

### 系统架构

```
┌─────────────────────────────────────┐
│         用户 (UART 终端)             │
└──────────────┬──────────────────────┘
               │ 字符输入/输出
┌──────────────▼──────────────────────┐
│          Shell 任务                  │
│  ┌────────────────────────────────┐ │
│  │    输入处理模块                 │ │
│  │  - 字符接收                     │ │
│  │  - 回显                         │ │
│  │  - 行编辑 (退格)                │ │
│  └────────────┬───────────────────┘ │
│               │                      │
│  ┌────────────▼───────────────────┐ │
│  │    命令解析模块                 │ │
│  │  - 分词                         │ │
│  │  - 参数提取                     │ │
│  └────────────┬───────────────────┘ │
│               │                      │
│  ┌────────────▼───────────────────┐ │
│  │    命令分发模块                 │ │
│  │  - 命令查找                     │ │
│  │  - 参数验证                     │ │
│  │  - 命令执行                     │ │
│  └────────────┬───────────────────┘ │
└───────────────┼─────────────────────┘
                │
    ┌───────────┼───────────┐
    │           │           │
┌───▼───┐  ┌───▼───┐  ┌───▼────┐
│ 系统  │  │ 内存  │  │ 文件   │
│ 信息  │  │ 管理  │  │ 系统   │
│ 命令  │  │ 命令  │  │ 命令   │
└───────┘  └───────┘  └────────┘
```

### 组件设计

#### 1. Shell 核心 (`smart_shell.c/h`)

**职责：**
- Shell 任务管理
- 输入缓冲区管理
- 命令提示符显示
- 主循环控制

**关键数据结构：**
```c
typedef struct {
    char input_buffer[SHELL_INPUT_BUFFER_SIZE];  // 输入缓冲区
    uint32_t input_pos;                           // 当前输入位置
    uint8_t running;                              // Shell 运行状态
} shell_context_t;
```

#### 2. 命令解析器 (`smart_shell.c`)

**职责：**
- 将输入行分割为命令和参数
- 处理空格和特殊字符
- 参数计数

**关键函数：**
```c
// 解析命令行，返回参数数量
int shell_parse_command(const char *line, char *argv[], int max_args);
```

#### 3. 命令注册表 (`smart_shell.c`)

**职责：**
- 维护命令列表
- 命令查找
- 命令执行

**关键数据结构：**
```c
typedef int (*shell_cmd_func_t)(int argc, char *argv[]);

typedef struct {
    const char *name;           // 命令名
    const char *description;    // 命令描述
    const char *usage;          // 使用方法
    shell_cmd_func_t handler;   // 处理函数
} shell_command_t;
```

#### 4. UART 输入扩展 (`drivers/smart_uart.c`)

**新增功能：**
- 非阻塞字符读取
- 输入可用性检查

**新增函数：**
```c
// 非阻塞读取一个字符，返回 1 表示成功，0 表示无数据
int smart_uart_getc_nonblock(char *c);

// 检查是否有输入可用
int smart_uart_input_available(void);
```

## 数据结构

### 命令表

```c
static const shell_command_t shell_commands[] = {
    {"help",    "显示所有命令",           "help",                  cmd_help},
    {"version", "显示系统版本",           "version",               cmd_version},
    {"uptime",  "显示系统运行时间",       "uptime",                cmd_uptime},
    {"ps",      "显示任务列表",           "ps",                    cmd_ps},
    {"clear",   "清空屏幕",               "clear",                 cmd_clear},
    {"meminfo", "显示内存信息",           "meminfo",               cmd_meminfo},
    {"free",    "显示可用内存",           "free",                  cmd_free},
    {"ls",      "列出文件",               "ls",                    cmd_ls},
    {"cat",     "显示文件内容",           "cat <filename>",        cmd_cat},
    {"format",  "格式化文件系统",         "format",                cmd_format},
    {"fsinfo",  "显示文件系统信息",       "fsinfo",                cmd_fsinfo},
    {NULL,      NULL,                     NULL,                    NULL}  // 结束标记
};
```

### 输入缓冲区

```c
#define SHELL_INPUT_BUFFER_SIZE  128
#define SHELL_MAX_ARGS           16

static shell_context_t shell_ctx = {
    .input_buffer = {0},
    .input_pos = 0,
    .running = 0
};
```

## 接口设计

### 公共 API

```c
// 初始化并启动 Shell
void smart_shell_init(void);

// Shell 任务入口（内部使用）
void shell_task_entry(void *param);
```

### 命令处理函数接口

```c
// 命令处理函数原型
// 返回值：0 表示成功，非 0 表示错误
typedef int (*shell_cmd_func_t)(int argc, char *argv[]);

// 示例：help 命令
int cmd_help(int argc, char *argv[]);

// 示例：ls 命令
int cmd_ls(int argc, char *argv[]);
```

## 实现细节

### 输入处理流程

```
1. 等待 UART 输入
   ↓
2. 读取字符
   ↓
3. 判断字符类型
   ├─ 回车 → 执行命令
   ├─ 退格 → 删除字符
   ├─ 可打印字符 → 添加到缓冲区并回显
   └─ 其他 → 忽略
   ↓
4. 返回步骤 1
```

### 命令执行流程

```
1. 解析命令行
   ↓
2. 提取命令名和参数
   ↓
3. 在命令表中查找命令
   ├─ 找到 → 调用处理函数
   └─ 未找到 → 显示错误
   ↓
4. 显示新提示符
```

### 特殊字符处理

| 字符 | ASCII | 处理方式 |
|------|-------|----------|
| 回车 | 0x0D  | 执行命令 |
| 换行 | 0x0A  | 忽略（或与回车一起处理） |
| 退格 | 0x08  | 删除最后一个字符 |
| DEL  | 0x7F  | 删除最后一个字符 |
| Ctrl+C | 0x03 | 清空当前行（可选） |
| ESC  | 0x1B  | 用于箭头键序列（可选） |

### 内置命令实现

#### help 命令

```c
int cmd_help(int argc, char *argv[])
{
    smart_uart_print("\nAvailable commands:\n");
    smart_uart_print("-------------------\n");
    
    for (int i = 0; shell_commands[i].name != NULL; i++) {
        smart_uart_print("  ");
        smart_uart_print(shell_commands[i].name);
        smart_uart_print(" - ");
        smart_uart_print(shell_commands[i].description);
        smart_uart_print("\n");
    }
    
    return 0;
}
```

#### ps 命令

```c
int cmd_ps(int argc, char *argv[])
{
    smart_uart_print("\nTask List:\n");
    smart_uart_print("PID  Entry     Deadline  Period    State\n");
    smart_uart_print("---  --------  --------  --------  -----\n");
    
    // 遍历任务列表并显示信息
    // 需要在 smart_core.c 中添加任务遍历接口
    
    return 0;
}
```

#### ls 命令

```c
int cmd_ls(int argc, char *argv[])
{
    smart_fs_status_t status = smart_fs_list_dir("/");
    
    if (status != SMART_FS_OK) {
        smart_uart_print("Error: Failed to list directory\n");
        return -1;
    }
    
    return 0;
}
```

#### cat 命令

```c
int cmd_cat(int argc, char *argv[])
{
    if (argc < 2) {
        smart_uart_print("Usage: cat <filename>\n");
        return -1;
    }
    
    smart_file_t file;
    smart_fs_status_t status = smart_fs_open(argv[1], &file);
    
    if (status != SMART_FS_OK) {
        smart_uart_print("Error: File not found\n");
        return -1;
    }
    
    char buffer[128];
    uint32_t bytes_read;
    
    while (1) {
        status = smart_fs_read(&file, buffer, sizeof(buffer) - 1, &bytes_read);
        if (status != SMART_FS_OK || bytes_read == 0) {
            break;
        }
        
        buffer[bytes_read] = '\0';
        smart_uart_print(buffer);
    }
    
    smart_fs_close(&file);
    smart_uart_print("\n");
    
    return 0;
}
```

## 正确性属性

*属性是一个特征或行为，应该在系统的所有有效执行中保持为真——本质上是关于系统应该做什么的正式陈述。属性作为人类可读规范和机器可验证正确性保证之间的桥梁。*

### 属性 1: 输入缓冲区不溢出

*对于任何*输入序列，当缓冲区已满时，Shell 应拒绝新输入，缓冲区位置不应超过缓冲区大小。

**验证：需求 1.4**

### 属性 2: 命令解析一致性

*对于任何*有效命令行，解析后的参数数量应等于实际参数个数，且每个参数应正确提取。

**验证：需求 2.1, 2.2**

### 属性 3: 命令查找完整性

*对于任何*已注册命令，Shell 应能在命令表中找到并执行；对于未注册命令，Shell 应返回"未知命令"错误。

**验证：需求 2.4**

### 属性 4: 错误恢复

*对于任何*命令执行错误，Shell 应显示错误信息并返回到就绪状态，不应崩溃或挂起。

**验证：需求 7.1, 7.5**

### 属性 5: 任务独立性

*对于任何*Shell 操作（等待输入、执行命令），其他任务应继续正常运行，不受 Shell 阻塞。

**验证：需求 8.4**

## 错误处理

### 错误类型

1. **输入错误**
   - 缓冲区溢出 → 提示并拒绝输入
   - 无效字符 → 忽略

2. **解析错误**
   - 参数过多 → 截断并警告
   - 空命令 → 忽略

3. **执行错误**
   - 命令不存在 → 显示"未知命令"
   - 参数错误 → 显示用法
   - 文件系统错误 → 显示具体错误

4. **系统错误**
   - 内存不足 → 提示并继续
   - 任务创建失败 → 报告错误

## 测试策略

### 单元测试

1. **命令解析测试**
   - 测试各种输入格式
   - 测试边界条件（空输入、最大参数）

2. **命令查找测试**
   - 测试已知命令
   - 测试未知命令
   - 测试大小写敏感性

### 集成测试

1. **输入处理测试**
   - 测试正常输入
   - 测试退格
   - 测试缓冲区溢出

2. **命令执行测试**
   - 测试每个内置命令
   - 测试错误情况
   - 测试命令组合

### 系统测试

1. **并发测试**
   - Shell 运行时其他任务正常工作
   - 长时间运行稳定性

2. **压力测试**
   - 快速连续输入
   - 大量命令执行

## 性能考虑

### 内存使用

- Shell 任务栈：1024 字节
- 输入缓冲区：128 字节
- 命令表：静态分配，约 500 字节
- 总计：约 1.7 KB

### CPU 使用

- 空闲时：Shell 任务让出 CPU，几乎无开销
- 输入时：每个字符处理约 10 微秒
- 命令执行：取决于具体命令

### 优先级

- Shell 任务优先级：低（不影响实时任务）
- 建议优先级：最低或次低

## 扩展性

### 添加新命令

1. 实现命令处理函数
2. 在命令表中注册
3. 无需修改 Shell 核心代码

示例：
```c
int cmd_mycommand(int argc, char *argv[])
{
    // 命令实现
    return 0;
}

// 在命令表中添加
{"mycommand", "我的命令", "mycommand [args]", cmd_mycommand},
```

### 未来增强

1. **命令历史**
   - 使用循环缓冲区存储历史
   - 上下箭头键导航

2. **Tab 补全**
   - 命令名补全
   - 文件名补全

3. **管道和重定向**
   - 命令管道 `cmd1 | cmd2`
   - 输出重定向 `cmd > file`

4. **脚本支持**
   - 从文件读取命令
   - 批处理执行

## 依赖关系

### 必需组件

- UART 驱动（已有）
- 任务调度器（已有）
- 文件系统（已有）
- 内存池（已有）

### 新增依赖

- UART 非阻塞读取功能（需添加）
- 任务列表遍历接口（需添加到 smart_core.c）

## 实现计划

### 阶段 1：基础框架（核心功能）

1. Shell 任务创建
2. 输入处理（回显、退格）
3. 命令解析
4. 命令分发

### 阶段 2：基本命令

1. help
2. version
3. clear
4. uptime

### 阶段 3：系统命令

1. ps
2. meminfo
3. free

### 阶段 4：文件系统命令

1. ls
2. cat
3. format
4. fsinfo

### 阶段 5：优化和增强

1. 错误处理完善
2. 性能优化
3. 用户体验改进

## 总结

Shell 设计采用模块化架构，核心功能独立，命令可扩展。通过 UART 与用户交互，作为独立任务运行，不影响系统其他功能。实现简单但功能完整，为后续系统调用和高级功能奠定基础。
