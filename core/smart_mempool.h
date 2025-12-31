#ifndef __SMART_MEMPOOL_H__
#define __SMART_MEMPOOL_H__

#include <stdint.h>
#include <stddef.h>

typedef enum
{
    SMART_MEMPOOL_OK = 0,
    SMART_MEMPOOL_EMPTY,
    SMART_MEMPOOL_BUSY,
    SMART_MEMPOOL_INVALID
} smart_mempool_status_t;

typedef struct smart_mempool
{
    uint8_t *buffer;
    uint8_t *buffer_end;
    uint32_t block_size;
    uint32_t block_stride;
    uint16_t block_count;
    uint16_t free_count;
    uint16_t ops_per_tick;
    uint16_t ops_left;
    void *free_list;
    uint16_t min_free_count; /* 运行过程中记录的最小剩余块数 */
} smart_mempool_t;

void smart_mempool_init(smart_mempool_t *pool,
                        void *buffer,
                        uint32_t block_size,
                        uint32_t block_count,
                        uint16_t ops_per_tick);

smart_mempool_status_t smart_mempool_alloc_try(smart_mempool_t *pool, void **out_block);
smart_mempool_status_t smart_mempool_free_try(smart_mempool_t *pool, void *block);

void smart_mempool_tick(void);

uint16_t smart_mempool_get_free(const smart_mempool_t *pool);
uint16_t smart_mempool_get_min_free(const smart_mempool_t *pool);

/* 获取全局内存池统计信息（用于Shell命令） */
typedef struct {
    uint32_t block_size;
    uint16_t block_count;
    uint16_t free_count;
    uint16_t min_free_count;
} smart_mempool_stats_t;

void smart_mempool_get_stats(const smart_mempool_t *pool, smart_mempool_stats_t *stats);

/* 获取全局内存池（由应用层提供） */
smart_mempool_t *smart_get_mempool(void);

#endif

