#include <common/util.h>
#include <common/macro.h>
#include <common/kprint.h>
#include <mm/buddy.h>

/*
 * The layout of a phys_mem_pool:
 * | page_metadata are (an array of struct page) | alignment pad | usable memory
 * |
 *
 * The usable memory: [pool_start_addr, pool_start_addr + pool_mem_size).
 */
void init_buddy(struct phys_mem_pool *pool, struct page *start_page,
                vaddr_t start_addr, u64 page_num)
{
        int order;
        int page_idx;
        struct page *page;

        /* Init the physical memory pool. */
        pool->pool_start_addr = start_addr;
        pool->page_metadata = start_page;
        pool->pool_mem_size = page_num * BUDDY_PAGE_SIZE;
        /* This field is for unit test only. */
        pool->pool_phys_page_num = page_num;

        /* Init the free lists */
        for (order = 0; order < BUDDY_MAX_ORDER; ++order) {
                pool->free_lists[order].nr_free = 0;
                init_list_head(&(pool->free_lists[order].free_list));
        }

        /* Clear the page_metadata area. */
        memset((char *)start_page, 0, page_num * sizeof(struct page));

        /* Init the page_metadata area. */
        for (page_idx = 0; page_idx < page_num; ++page_idx) {
                page = start_page + page_idx;
                page->allocated = 1;
                page->order = 0;
                page->pool = pool;
        }

        /* Put each physical memory page into the free lists. */
        for (page_idx = 0; page_idx < page_num; ++page_idx) {
                page = start_page + page_idx;
                buddy_free_pages(pool, page);
        }
}

static struct page *get_buddy_chunk(struct phys_mem_pool *pool,
                                    struct page *chunk)
{
        u64 chunk_addr;
        u64 buddy_chunk_addr;
        int order;

        /* Get the address of the chunk. */
        chunk_addr = (u64)page_to_virt(chunk);
        order = chunk->order;
/*
 * Calculate the address of the buddy chunk according to the address
 * relationship between buddies.
 */
#define BUDDY_PAGE_SIZE_ORDER (12)
        buddy_chunk_addr = chunk_addr
                           ^ (1UL << (order + BUDDY_PAGE_SIZE_ORDER));

        /* Check whether the buddy_chunk_addr belongs to pool. */
        if ((buddy_chunk_addr < pool->pool_start_addr)
            || (buddy_chunk_addr
                >= (pool->pool_start_addr + pool->pool_mem_size))) {
                return NULL;
        }

        return virt_to_page((void *)buddy_chunk_addr);
}

static struct page *split_page(struct phys_mem_pool *pool, u64 order,
                               struct page *page)
{
        /* LAB 2 TODO 2 BEGIN */
        /*
         * Hint: Recursively put the buddy of current chunk into
         * a suitable free list.
         */
        if(page->order == order){
                return page;
        }
        list_del(&(page->node));
        pool->free_lists[page->order].nr_free--;
        page->order--;
        pool->free_lists[page->order].nr_free += 2;
        struct page *buddy_page = get_buddy_chunk(pool, page);
        buddy_page->order = page->order;
        buddy_page->allocated = 0;
        buddy_page->pool = pool;
        list_add(&buddy_page->node, &(pool->free_lists[page->order].free_list));
        list_add(&page->node, &(pool->free_lists[page->order].free_list));

        return split_page(pool, order, page);

        /* LAB 2 TODO 2 END */
}

struct page *buddy_get_pages(struct phys_mem_pool *pool, u64 order)
{
        /* LAB 2 TODO 2 BEGIN */
        /*
         * Hint: Find a chunk that satisfies the order requirement
         * in the free lists, then split it if necessary.
         */
        if(order > BUDDY_MAX_ORDER - 1){
                return NULL;
        }
        u64 current_order = order;
        while(list_empty(&(pool->free_lists[current_order].free_list))){
                current_order++;
                if(current_order > BUDDY_MAX_ORDER - 1){
                        return NULL;
                }
        }
        struct page *page = list_entry(pool->free_lists[current_order].free_list.next, struct page, node);
        if(current_order > order){
                page = split_page(pool, order, page);
        }

        pool->free_lists[order].nr_free--;
        list_del(&(page->node));
        page->allocated = 1;

        return page;

        /* LAB 2 TODO 2 END */
}

static struct page *merge_page(struct phys_mem_pool *pool, struct page *page)
{
        /* LAB 2 TODO 2 BEGIN */
        /*
         * Hint: Recursively merge current chunk with its buddy
         * if possible.
         */
        if(page->order >= BUDDY_MAX_ORDER - 1){
                return page;
        }
        struct page *buddy_page = get_buddy_chunk(pool, page);
        if(buddy_page == NULL || buddy_page->allocated || page->order != buddy_page->order){
                return page;
        }
        struct page *merged_page  = NULL;
        /* Compare the address of the chunk to determine address of merged_page */
        if((u64)page_to_virt(page) < (u64)page_to_virt(buddy_page)){
                merged_page = page;
        }
        else{
                merged_page = buddy_page;
        }
        pool->free_lists[page->order].nr_free -= 2;
        list_del(&(page->node));
        list_del(&(buddy_page->node));
        merged_page->order++;
        pool->free_lists[merged_page->order].nr_free++;
        list_add(&(merged_page->node), &(pool->free_lists[merged_page->order].free_list));

        return merge_page(pool, merged_page);

        /* LAB 2 TODO 2 END */
}

void buddy_free_pages(struct phys_mem_pool *pool, struct page *page)
{
        /* LAB 2 TODO 2 BEGIN */
        /*
         * Hint: Merge the chunk with its buddy and put it into
         * a suitable free list.
         */
        page->allocated = 0;
        struct free_list *list = &pool->free_lists[page->order];
        list->nr_free++;
        list_add(&(page->node), &(list->free_list));
        merge_page(pool, page);

        /* LAB 2 TODO 2 END */
}

void *page_to_virt(struct page *page)
{
        u64 addr;
        struct phys_mem_pool *pool = page->pool;

        BUG_ON(pool == NULL);
        /* page_idx * BUDDY_PAGE_SIZE + start_addr */
        addr = (page - pool->page_metadata) * BUDDY_PAGE_SIZE
               + pool->pool_start_addr;
        return (void *)addr;
}

struct page *virt_to_page(void *ptr)
{
        struct page *page;
        struct phys_mem_pool *pool = NULL;
        u64 addr = (u64)ptr;
        int i;

        /* Find the corresponding physical memory pool. */
        for (i = 0; i < physmem_map_num; ++i) {
                if (addr >= global_mem[i].pool_start_addr
                    && addr < global_mem[i].pool_start_addr
                                       + global_mem[i].pool_mem_size) {
                        pool = &global_mem[i];
                        break;
                }
        }

        BUG_ON(pool == NULL);
        page = pool->page_metadata
               + (((u64)addr - pool->pool_start_addr) / BUDDY_PAGE_SIZE);
        return page;
}

u64 get_free_mem_size_from_buddy(struct phys_mem_pool *pool)
{
        int order;
        struct free_list *list;
        u64 current_order_size;
        u64 total_size = 0;

        for (order = 0; order < BUDDY_MAX_ORDER; order++) {
                /* 2^order * 4K */
                current_order_size = BUDDY_PAGE_SIZE * (1 << order);
                list = pool->free_lists + order;
                total_size += list->nr_free * current_order_size;

                /* debug : print info about current order */
                kdebug("buddy memory chunk order: %d, size: 0x%lx, num: %d\n",
                       order,
                       current_order_size,
                       list->nr_free);
        }
        return total_size;
}

#ifdef CHCORE_KERNEL_TEST
#include <mm/mm.h>
#include <lab.h>
void lab2_test_buddy(void)
{
        struct phys_mem_pool *pool = &global_mem[0];
        {
                u64 free_mem_size = get_free_mem_size_from_buddy(pool);
                lab_check(pool->pool_phys_page_num == free_mem_size / PAGE_SIZE,
                          "Init buddy");
        }
        {
                lab_check(buddy_get_pages(pool, BUDDY_MAX_ORDER + 1) == NULL
                                  && buddy_get_pages(pool, (u64)-1) == NULL,
                          "Check invalid order");
        }
        {
                bool ok = true;
                u64 expect_free_mem = pool->pool_phys_page_num * PAGE_SIZE;
                struct page *page = buddy_get_pages(pool, 0);
                BUG_ON(page == NULL);
                lab_assert(page->order == 0 && page->allocated);
                expect_free_mem -= PAGE_SIZE;
                lab_assert(get_free_mem_size_from_buddy(pool)
                           == expect_free_mem);
                buddy_free_pages(pool, page);
                expect_free_mem += PAGE_SIZE;
                lab_assert(get_free_mem_size_from_buddy(pool)
                           == expect_free_mem);
                lab_check(ok, "Allocate & free order 0");
        }
        {
                bool ok = true;
                u64 expect_free_mem = pool->pool_phys_page_num * PAGE_SIZE;
                struct page *page;
                for (int i = 0; i < BUDDY_MAX_ORDER; i++) {
                        page = buddy_get_pages(pool, i);
                        BUG_ON(page == NULL);
                        lab_assert(page->order == i && page->allocated);
                        expect_free_mem -= (1 << i) * PAGE_SIZE;
                        lab_assert(get_free_mem_size_from_buddy(pool)
                                   == expect_free_mem);
                        buddy_free_pages(pool, page);
                        expect_free_mem += (1 << i) * PAGE_SIZE;
                        lab_assert(get_free_mem_size_from_buddy(pool)
                                   == expect_free_mem);
                }
                for (int i = BUDDY_MAX_ORDER - 1; i >= 0; i--) {
                        page = buddy_get_pages(pool, i);
                        BUG_ON(page == NULL);
                        lab_assert(page->order == i && page->allocated);
                        expect_free_mem -= (1 << i) * PAGE_SIZE;
                        lab_assert(get_free_mem_size_from_buddy(pool)
                                   == expect_free_mem);
                        buddy_free_pages(pool, page);
                        expect_free_mem += (1 << i) * PAGE_SIZE;
                        lab_assert(get_free_mem_size_from_buddy(pool)
                                   == expect_free_mem);
                }
                lab_check(ok, "Allocate & free each order");
        }
        {
                bool ok = true;
                u64 expect_free_mem = pool->pool_phys_page_num * PAGE_SIZE;
                struct page *pages[BUDDY_MAX_ORDER];
                for (int i = 0; i < BUDDY_MAX_ORDER; i++) {
                        pages[i] = buddy_get_pages(pool, i);
                        BUG_ON(pages[i] == NULL);
                        lab_assert(pages[i]->order == i);
                        expect_free_mem -= (1 << i) * PAGE_SIZE;
                        lab_assert(get_free_mem_size_from_buddy(pool)
                                   == expect_free_mem);
                }
                for (int i = 0; i < BUDDY_MAX_ORDER; i++) {
                        buddy_free_pages(pool, pages[i]);
                        expect_free_mem += (1 << i) * PAGE_SIZE;
                        lab_assert(get_free_mem_size_from_buddy(pool)
                                   == expect_free_mem);
                }
                lab_check(ok, "Allocate & free all orders");
        }
        {
                bool ok = true;
                u64 expect_free_mem = pool->pool_phys_page_num * PAGE_SIZE;
                struct page *page;
                for (int i = 0; i < pool->pool_phys_page_num; i++) {
                        page = buddy_get_pages(pool, 0);
                        BUG_ON(page == NULL);
                        lab_assert(page->order == 0);
                        expect_free_mem -= PAGE_SIZE;
                        lab_assert(get_free_mem_size_from_buddy(pool)
                                   == expect_free_mem);
                }
                lab_assert(get_free_mem_size_from_buddy(pool) == 0);
                lab_assert(buddy_get_pages(pool, 0) == NULL);
                for (int i = 0; i < pool->pool_phys_page_num; i++) {
                        page = pool->page_metadata + i;
                        lab_assert(page->allocated);
                        buddy_free_pages(pool, page);
                        expect_free_mem += PAGE_SIZE;
                        lab_assert(get_free_mem_size_from_buddy(pool)
                                   == expect_free_mem);
                }
                lab_assert(pool->pool_phys_page_num * PAGE_SIZE
                           == expect_free_mem);
                lab_check(ok, "Allocate & free all memory");
        }
        printk("[TEST] Buddy tests finished\n");
}
#endif /* CHCORE_KERNEL_TEST */
