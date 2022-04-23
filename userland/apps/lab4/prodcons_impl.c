#include "buf.h"
#include <chcore/internal/raw_syscall.h>
#include <chcore/thread.h>
#include <stdio.h>
#include "prodcons.h"

#define PRIO 255

volatile int global_exit = 0;
s32 empty_slot;
s32 filled_slot;

void *producer(void *arg)
{
        int new_msg;
        int i = 0;

        while (i < PROD_ITEM_CNT) {
                /* LAB 4 TODO BEGIN */
                __chcore_sys_wait_sem(empty_slot, 1);

                /* LAB 4 TODO END */
                new_msg = produce_new();
                buffer_add_safe(new_msg);
                /* LAB 4 TODO BEGIN */
                __chcore_sys_signal_sem(filled_slot);
                // printf("%d\n", i);
                // printf("i is %d  at producer new_msg is %d\n", i, new_msg);

                /* LAB 4 TODO END */
                i++;
        }
        __sync_fetch_and_add(&global_exit, 1);
        // printf("global is %d  at producer\n", global_exit);
        return 0;
}

void *consumer(void *arg)
{
        int cur_msg;
        int i = 0;

        while (i < COSM_ITEM_CNT) {
                /* LAB 4 TODO BEGIN */
                __chcore_sys_wait_sem(filled_slot, 1);

                /* LAB 4 TODO END */
                cur_msg = buffer_remove_safe();

                /* LAB 4 TODO BEGIN */
                __chcore_sys_signal_sem(empty_slot);

                /* LAB 4 TODO END */
                // printf("%d\n", i);
                // printf("i is %d at consuemr cur_msg is %d\n",i, cur_msg);
                consume_msg(cur_msg);
                i++;
        }
        __sync_fetch_and_add(&global_exit, 1);
        // printf("global is %d  at consumer\n", global_exit);
        return 0;
}
