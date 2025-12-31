# Smart-OS 设计文档（新增功能）

本文档描述 Smart-OS 相比初始版本新增的功能模块。

## 1. 时间可预测锁步内存池（Time-Predictable Lockstep Memory Pool）

### 1.1 设计理念

传统动态内存分配（如 `malloc/free`）存在以下问题：
- **时间不可预测**：分配/释放时间取决于碎片化程度，最坏情况可能很长
- **内存碎片**：频繁分配释放导致内存碎片，降低利用率
- **实时性差**：无法保证在 deadline 内完成内存操作

Smart-OS 引入**时间可预测锁步内存池**，核心思想：
- **定长块分配**：所有内存块大小相同，消除碎片
- **每 Tick 操作预算**：每个系统时钟周期（1ms）只允许有限次内存操作
- **常数时间操作**：分配/释放均为 O(1) 时间复杂度
- **与调度器同步**：内存操作预算在每个 SysTick 中断中统一刷新

### 1.2 数据结构

```c
typedef struct smart_mempool
{
    uint8_t *buffer;          // 内存池缓冲区起始地址
    uint8_t *buffer_end;      // 缓冲区结束地址（用于地址验证）
    uint32_t block_size;      // 用户请求的块大小
    uint32_t block_stride;    // 实际块步长（对齐后）
    uint16_t block_count;     // 总块数
    uint16_t free_count;      // 当前空闲块数
    uint16_t ops_per_tick;    // 每个 tick 允许的操作次数
    uint16_t ops_left;        // 当前 tick 剩余操作次数
    void *free_list;          // 空闲块链表头
} smart_mempool_t;
```

**关键设计点**：
- `block_stride`：块大小按 4 字节对齐，确保地址对齐和指针操作安全
- `ops_per_tick`：可配置的操作预算，控制内存操作的实时性
- `free_list`：使用链表管理空闲块，分配/释放均为 O(1)

### 1.3 操作状态

```c
typedef enum
{
    SMART_MEMPOOL_OK = 0,      // 操作成功
    SMART_MEMPOOL_EMPTY,       // 内存池已满（无空闲块）
    SMART_MEMPOOL_BUSY,        // 当前 tick 操作预算已用完
    SMART_MEMPOOL_INVALID      // 参数无效或地址错误
} smart_mempool_status_t;
```

### 1.4 核心机制

#### 1.4.1 初始化

```c
void smart_mempool_init(smart_mempool_t *pool,
                        void *buffer,
                        uint32_t block_size,
                        uint32_t block_count,
                        uint16_t ops_per_tick);
```

**初始化流程**：
1. 计算对齐后的块步长（4 字节对齐）
2. 初始化空闲块链表（每个块的前 4 字节存储下一个块的地址）
3. 设置操作预算参数
4. 注册到内核的全局池表

**关键特性**：
- 内存池缓冲区由用户提供（静态分配），不依赖动态分配
- 支持多个独立的内存池实例
- 每个池可配置不同的块大小和操作预算

#### 1.4.2 分配操作

```c
smart_mempool_status_t smart_mempool_alloc_try(smart_mempool_t *pool, void **out_block);
```

**分配流程**：
1. 检查操作预算（`ops_left > 0`）
2. 检查空闲块可用性（`free_list != NULL`）
3. 从链表头取出一个块
4. 更新空闲计数和操作预算
5. 返回块地址

**时间特性**：
- **最坏情况**：O(1) 常数时间
- **可预测性**：操作时间与内存池状态无关
- **实时保证**：如果预算用完，立即返回 `SMART_MEMPOOL_BUSY`，不阻塞

#### 1.4.3 释放操作

```c
smart_mempool_status_t smart_mempool_free_try(smart_mempool_t *pool, void *block);
```

**释放流程**：
1. 验证块地址有效性（在池范围内且对齐正确）
2. 检查操作预算
3. 将块插入空闲链表头部
4. 更新空闲计数和操作预算

**安全特性**：
- 地址验证防止释放无效指针
- 双重释放检测（通过地址验证间接实现）
- 临界区保护确保线程安全

#### 1.4.4 锁步刷新

```c
void smart_mempool_tick(void);
```

**刷新机制**：
- 在每个 SysTick 中断中调用
- 遍历所有已注册的内存池
- 将每个池的 `ops_left` 重置为 `ops_per_tick`

**与调度器集成**：
```c
void SysTick_Handler(void)
{
    os_tick++;
    smart_mempool_tick();  // 刷新所有内存池的操作预算
    // ... 其他处理
}
```

### 1.5 使用示例

```c
#define MEMPOOL_BLOCK_SIZE   64u
#define MEMPOOL_BLOCK_COUNT  8u
#define MEMPOOL_OPS_PER_TICK 2u

static uint8_t telemetry_pool_buf[MEMPOOL_BLOCK_SIZE * MEMPOOL_BLOCK_COUNT];
static smart_mempool_t telemetry_pool;

void task_entry(void *param)
{
    void *block = NULL;
    
    // 尝试分配
    smart_mempool_status_t st = smart_mempool_alloc_try(&telemetry_pool, &block);
    if (st == SMART_MEMPOOL_OK)
    {
        // 使用内存块
        // ...
        
        // 释放
        smart_mempool_free_try(&telemetry_pool, block);
    }
    else if (st == SMART_MEMPOOL_BUSY)
    {
        // 当前 tick 预算已用完，等待下一个 tick
    }
    else if (st == SMART_MEMPOOL_EMPTY)
    {
        // 内存池已满，需要等待其他任务释放
    }
}
```

### 1.6 实时性分析

**时间可预测性**：
- 分配操作：固定时间（链表操作 + 临界区开销）
- 释放操作：固定时间（地址验证 + 链表操作 + 临界区开销）
- 最坏情况延迟：可精确计算，适合硬实时系统

**操作预算控制**：
- 每个 tick 最多执行 `ops_per_tick` 次内存操作
- 防止某个任务占用过多 CPU 时间进行内存操作
- 保证其他任务也能获得内存操作机会

**可调度性影响**：
- 内存操作时间可纳入任务 WCET（最坏情况执行时间）分析
- 不会因为内存碎片导致时间不可预测
- 支持形式化验证和可调度性证明

## 2. 代码结构更新

### 2.1 新增文件

```
Smart-OS/
├── core/
│   ├── smart_mempool.h      # 内存池 API 定义
│   └── smart_mempool.c      # 内存池实现
```

### 2.2 修改文件

**`core/smart_core.c`**：
- `SysTick_Handler()` 中调用 `smart_mempool_tick()` 刷新操作预算

**`user/main.c`**：
- 添加内存池测试示例
- TaskA 和 TaskB 交替进行内存分配/释放操作

**`Makefile`**：
- 添加 `smart_mempool.c` 到编译列表

## 3. 创新点总结

### 3.1 时间可预测性

- **常数时间操作**：分配/释放均为 O(1)，与内存池状态无关
- **操作预算机制**：每个 tick 限制操作次数，防止时间抖动
- **与调度器同步**：内存操作预算与系统时钟同步刷新

### 3.2 实时系统友好

- **无内存碎片**：定长块设计，消除碎片问题
- **可分析性**：最坏情况执行时间可精确计算
- **非阻塞设计**：预算用完立即返回，不阻塞任务

### 3.3 模块化设计

- **独立模块**：内存池与调度器解耦，可独立使用
- **多实例支持**：支持创建多个不同配置的内存池
- **静态分配**：不依赖动态内存，适合资源受限系统

## 4. 适用场景

### 4.1 硬实时系统

- 需要保证内存操作时间的可预测性
- 支持 WCET 分析和可调度性验证
- 适合安全关键应用

### 4.2 周期性任务通信

- 任务间传递固定大小的数据块
- 遥测数据缓冲区
- 事件队列

### 4.3 资源受限系统

- 内存资源有限，需要精确控制
- 避免动态分配的不确定性
- 静态配置，编译时确定内存使用

## 5. 未来扩展方向

### 5.1 多级内存池

- 支持不同块大小的内存池
- 根据任务需求选择合适的内存池

### 5.2 内存使用统计

- 记录分配/释放次数
- 监控内存池利用率
- 支持运行时诊断

### 5.3 内存保护

- 结合 MPU（内存保护单元）实现内存隔离
- 防止任务访问其他任务的内存块
- 增强系统安全性

