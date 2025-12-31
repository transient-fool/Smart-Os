#include "smart_mempool.h"
#include "smart_core.h"
#include "smart_uart.h"

#ifndef SMART_MEMPOOL_MAX_POOLS
#define SMART_MEMPOOL_MAX_POOLS 4
#endif

static smart_mempool_t *pool_registry[SMART_MEMPOOL_MAX_POOLS];
static uint32_t pool_registry_count = 0;

static uint32_t align_up(uint32_t value, uint32_t align)
{
    return (value + (align - 1u)) & ~(align - 1u);
}

static int smart_mempool_register(smart_mempool_t *pool)
{
    if (pool_registry_count >= SMART_MEMPOOL_MAX_POOLS)
    {
        return -1;
    }
    pool_registry[pool_registry_count++] = pool;
    return 0;
}

static int smart_mempool_address_valid(smart_mempool_t *pool, void *block)
{
    if (!pool || !block) return 0;
    
    uint8_t *ptr = (uint8_t *)block;
    if (ptr < pool->buffer || ptr >= pool->buffer_end) return 0;
    if (((uintptr_t)(ptr - pool->buffer)) % pool->block_stride != 0) return 0;
    return 1;
}

void smart_mempool_init(smart_mempool_t *pool,
                        void *buffer,
                        uint32_t block_size,
                        uint32_t block_count,
                        uint16_t ops_per_tick)
{
    if (!pool || !buffer || block_size == 0 || block_count == 0)
    {
        return;
    }
    
    smart_enter_critical();
    
    pool->buffer = (uint8_t *)buffer;
    pool->block_stride = align_up(block_size, 4u);
    pool->block_size = block_size;
    pool->block_count = (uint16_t)block_count;
    pool->buffer_end = pool->buffer + pool->block_stride * block_count;
    pool->free_count = (uint16_t)block_count;
    pool->min_free_count = (uint16_t)block_count;
    pool->ops_per_tick = (ops_per_tick == 0) ? (uint16_t)block_count : ops_per_tick;
    pool->ops_left = pool->ops_per_tick;
    
    pool->free_list = pool->buffer;
    for (uint32_t i = 0; i < block_count; ++i)
    {
        uint8_t *current = pool->buffer + i * pool->block_stride;
        uint8_t *next = (i + 1u < block_count) ? (current + pool->block_stride) : NULL;
        *(void **)current = next;
    }
    
    smart_mempool_register(pool);
    
    smart_exit_critical();
}

smart_mempool_status_t smart_mempool_alloc_try(smart_mempool_t *pool, void **out_block)
{
    if (!pool || !out_block)
    {
        return SMART_MEMPOOL_INVALID;
    }
    
    smart_mempool_status_t status = SMART_MEMPOOL_OK;
    
    smart_enter_critical();
    
    if (pool->ops_left == 0)
    {
        status = SMART_MEMPOOL_BUSY;
    }
    else if (!pool->free_list)
    {
        status = SMART_MEMPOOL_EMPTY;
    }
    else
    {
        void *block = pool->free_list;
        pool->free_list = *(void **)pool->free_list;
        pool->free_count--;
        if (pool->free_count < pool->min_free_count)
        {
            pool->min_free_count = pool->free_count;
        }
        pool->ops_left--;
        *out_block = block;
        status = SMART_MEMPOOL_OK;
    }
    
    smart_exit_critical();
    return status;
}

smart_mempool_status_t smart_mempool_free_try(smart_mempool_t *pool, void *block)
{
    if (!pool || !block)
    {
        return SMART_MEMPOOL_INVALID;
    }
    
    smart_mempool_status_t status = SMART_MEMPOOL_OK;
    
    smart_enter_critical();
    
    if (!smart_mempool_address_valid(pool, block))
    {
        status = SMART_MEMPOOL_INVALID;
    }
    else if (pool->ops_left == 0)
    {
        status = SMART_MEMPOOL_BUSY;
    }
    else
    {
        *(void **)block = pool->free_list;
        pool->free_list = block;
        pool->free_count++;
        pool->ops_left--;
        status = SMART_MEMPOOL_OK;
    }
    
    smart_exit_critical();
    return status;
}

void smart_mempool_tick(void)
{
    for (uint32_t i = 0; i < pool_registry_count; ++i)
    {
        smart_mempool_t *pool = pool_registry[i];
        if (!pool) continue;
        pool->ops_left = pool->ops_per_tick;
    }
}

uint16_t smart_mempool_get_free(const smart_mempool_t *pool)
{
    return pool ? pool->free_count : 0u;
}

uint16_t smart_mempool_get_min_free(const smart_mempool_t *pool)
{
    return pool ? pool->min_free_count : 0u;
}


void smart_mempool_get_stats(const smart_mempool_t *pool, smart_mempool_stats_t *stats)
{
    if (!pool || !stats)
    {
        return;
    }
    
    smart_enter_critical();
    
    stats->block_size = pool->block_size;
    stats->block_count = pool->block_count;
    stats->free_count = pool->free_count;
    stats->min_free_count = pool->min_free_count;
    
    smart_exit_critical();
}
