/* Scheduler related functions are implemented here */
#include <sched/sched.h>
#include <arch/machine/smp.h>
#include <common/kprint.h>
#include <machine.h>
#include <mm/kmalloc.h>
#include <common/list.h>
#include <common/util.h>
#include <object/thread.h>
#include <common/macro.h>
#include <common/errno.h>
#include <common/types.h>
#include <object/thread.h>
#include <irq/irq.h>
#include <sched/context.h>

/* in arch/sched/idle.S */
void idle_thread_routine(void);

/* Metadata for ready queue */
struct queue_meta {
        struct list_head queue_head;
        u32 queue_len;
        char pad[pad_to_cache_line(sizeof(u32) + sizeof(struct list_head))];
};

/*
 * rr_ready_queue
 * Per-CPU ready queue for ready tasks.
 */
struct queue_meta rr_ready_queue_meta[PLAT_CPU_NUM];

/*
 * RR policy also has idle threads.
 * When no active user threads in ready queue,
 * we will choose the idle thread to execute.
 * Idle thread will **NOT** be in the RQ.
 */
struct thread idle_threads[PLAT_CPU_NUM];

/*
 * Lab4
 * Sched_enqueue
 * Put `thread` at the end of ready queue of assigned `affinity`.
 * If affinity = NO_AFF, assign the core to the current cpu.
 * If the thread is IDLE thread, do nothing!
 * Do not forget to check if the affinity is valid!
 */
int rr_sched_enqueue(struct thread *thread)
{
        /* LAB 4 TODO BEGIN */
        if(!thread || !(thread->thread_ctx) || thread->thread_ctx->state == TS_READY){
                return -1;
        }
        if(thread->thread_ctx->type == TYPE_IDLE){
                return 0;
        }
        if(thread->thread_ctx->state == TS_WAITING){
                return 0;
        }
        u32 cpu_id = smp_get_cpu_id();
        if(thread->thread_ctx->affinity != NO_AFF){
                cpu_id = thread->thread_ctx->affinity;
        }
        if (cpu_id < 0 || cpu_id >= PLAT_CPU_NUM) {
                // kinfo("wrong cpu_id is %d", cpu_id);
                return -1;
        }
        list_add(&(thread->ready_queue_node), &(rr_ready_queue_meta[cpu_id].queue_head));
        rr_ready_queue_meta[cpu_id].queue_len++;
        thread->thread_ctx->cpuid = cpu_id;
        thread->thread_ctx->state = TS_READY;
        thread->thread_ctx->thread_exit_state = TE_RUNNING;
        thread->thread_ctx->sc->budget = DEFAULT_BUDGET;

        /* LAB 4 TODO END */
        return 0;
}

/*
 * Lab4
 * Sched_dequeue
 * remove `thread` from its current residual ready queue
 * Do not forget to add some basic parameter checking
 */
int rr_sched_dequeue(struct thread *thread)
{
        /* LAB 4 TODO BEGIN */
        if(!thread || !(thread->thread_ctx) || thread->thread_ctx->state != TS_READY){
                return -1;
        }
        if(thread->thread_ctx->type == TYPE_IDLE){
                return 0;
        }
        list_del(&thread->ready_queue_node);
        u32 cpu_id = thread->thread_ctx->cpuid;
        rr_ready_queue_meta[cpu_id].queue_len--;
        thread->thread_ctx->state = TS_INTER;

        /* LAB 4 TODO END */
        return 0;
}

/*
 * Lab4
 * Choose an appropriate thread and dequeue from ready queue
 *
 * Hints:
 * You can use `list_entry` to find the next ready thread in the ready queue
 */
struct thread *rr_sched_choose_thread(void)
{
        struct thread *thread = NULL;
        /* LAB 4 TODO BEGIN */
        u32 cpu_id = smp_get_cpu_id();
        if(!rr_ready_queue_meta[cpu_id].queue_len){
                thread = &idle_threads[cpu_id];
        }
        else{
                thread = list_entry(rr_ready_queue_meta[cpu_id].queue_head.prev, struct thread, ready_queue_node);
        }
        rr_sched_dequeue(thread);

        /* LAB 4 TODO END */
        return thread;
}

/*
 * Lab4
 * You should use this function in rr_sched
 */
static inline void rr_sched_refill_budget(struct thread *target, u32 budget)
{
        /* LAB 4 TODO BEGIN */
        target->thread_ctx->sc->budget = budget;

        /* LAB 4 TODO END */
}

/*
 * Lab4
 * Schedule a thread to execute.
 * This function will suspend current running thread, if any, and schedule
 * another thread from `rr_ready_queue[cpu_id]`.
 *
 * Hints:
 * Macro DEFAULT_BUDGET defines the value for resetting thread's budget.
 * After you get one thread from rr_sched_choose_thread, pass it to
 * switch_to_thread() to prepare for switch_context().
 * Then ChCore can call eret_to_thread() to return to user mode.
 * You should also check the state of the old thread. Old thread
 * could be exiting/waiting or running when calling this function.
 * You will also need to check the remaining budget of the old thread.
 */
int rr_sched(void)
{
        /* LAB 4 TODO BEGIN */
        if(current_thread != NULL && current_thread->thread_ctx != NULL){
                if((current_thread->thread_ctx->sc != NULL && current_thread->thread_ctx->sc->budget != 0 
                        && current_thread->thread_ctx->thread_exit_state != TE_EXITING)
                || (current_thread->thread_ctx->state != TS_RUNNING && current_thread->thread_ctx->state != TS_WAITING
                        && current_thread->thread_ctx->state != TS_EXIT && current_thread->thread_ctx->thread_exit_state != TE_EXITING)){
                        return 0;
                }
                if(current_thread->thread_ctx->thread_exit_state == TE_EXITING){
                        // kinfo("thread exit\n");
                        current_thread->thread_ctx->thread_exit_state = TE_EXITED;
                        current_thread->thread_ctx->state = TS_EXIT;
                }
                else{
                        rr_sched_enqueue(current_thread);
                }
        }
        struct thread *thread = rr_sched_choose_thread();
        rr_sched_refill_budget(thread, DEFAULT_BUDGET);
        thread->thread_ctx->affinity = smp_get_cpu_id();
        switch_to_thread(thread);

        /* LAB 4 TODO END */

        return 0;
}

int rr_sched_init(void)
{
        int i = 0;
        char idle_name[] = "KNL-IDLE-RR";
        int name_len = strlen(idle_name);

        struct cap_group *idle_cap_group;
        struct vmspace *idle_vmspace;

        /* Initialize global variables */
        for (i = 0; i < PLAT_CPU_NUM; i++) {
                current_threads[i] = NULL;
                init_list_head(&(rr_ready_queue_meta[i].queue_head));
                rr_ready_queue_meta[i].queue_len = 0;
        }

        /* Create a fake idle cap group to store the name */
        idle_cap_group = kzalloc(sizeof(*idle_cap_group));
        memset(idle_cap_group->cap_group_name, 0, MAX_GROUP_NAME_LEN);
        if (name_len > MAX_GROUP_NAME_LEN)
                name_len = MAX_GROUP_NAME_LEN;
        memcpy(idle_cap_group->cap_group_name, idle_name, name_len);
        init_list_head(&idle_cap_group->thread_list);

        extern struct vmspace *create_idle_vmspace(void);
        idle_vmspace = create_idle_vmspace();

        /* Initialize one idle thread for each core and insert into the RQ */
        for (i = 0; i < PLAT_CPU_NUM; i++) {
                /* Set the thread context of the idle threads */
                BUG_ON(!(idle_threads[i].thread_ctx =
                                 create_thread_ctx(TYPE_IDLE)));
                /* We will set the stack and func ptr in arch_idle_ctx_init */
                init_thread_ctx(&idle_threads[i], 0, 0, MIN_PRIO, TYPE_IDLE, i);
                /* Call arch-dependent function to fill the context of the idle
                 * threads */
                arch_idle_ctx_init(idle_threads[i].thread_ctx,
                                   idle_thread_routine);

                idle_threads[i].cap_group = idle_cap_group;
                idle_threads[i].vmspace = idle_vmspace;

                /* Add idle_threads to the threads list */
                list_add(&idle_threads[i].node, &idle_cap_group->thread_list);
        }
        kdebug("Scheduler initialized. Create %d idle threads.\n", i);

        return 0;
}

#define MAX_CAP_GROUP_BUF 256

void rr_top(void)
{
        u32 cpuid;
        struct thread *thread;
        /* A simple way to collect all cap groups */
        struct cap_group *cap_group_buf[MAX_CAP_GROUP_BUF] = {0};
        unsigned int cap_group_num = 0;
        int i = 0;

        printk("\n*****CPU RQ Info*****\n");
        for (cpuid = 0; cpuid < PLAT_CPU_NUM; cpuid++) {
                printk("== CPU %d RQ LEN %lu==\n",
                       cpuid,
                       rr_ready_queue_meta[cpuid].queue_len);
                thread = current_threads[cpuid];
                if (thread != NULL) {
                        for (i = 0; i < cap_group_num; i++)
                                if (thread->cap_group == cap_group_buf[i])
                                        break;
                        if (i == cap_group_num
                            && cap_group_num < MAX_CAP_GROUP_BUF) {
                                cap_group_buf[cap_group_num] =
                                        thread->cap_group;
                                cap_group_num++;
                        }
                        printk("Current ");
                        print_thread(thread);
                }
                if (!list_empty(&(rr_ready_queue_meta[cpuid].queue_head))) {
                        for_each_in_list (
                                thread,
                                struct thread,
                                ready_queue_node,
                                &(rr_ready_queue_meta[cpuid].queue_head)) {
                                for (i = 0; i < cap_group_num; i++)
                                        if (thread->cap_group
                                            == cap_group_buf[i])
                                                break;
                                if (i == cap_group_num
                                    && cap_group_num < MAX_CAP_GROUP_BUF) {
                                        cap_group_buf[cap_group_num] =
                                                thread->cap_group;
                                        cap_group_num++;
                                }
                                print_thread(thread);
                        }
                }
                printk("\n");
        }

        printk("\n*****CAP GROUP Info*****\n");
        for (i = 0; i < cap_group_num; i++) {
                printk("== CAP GROUP:%s ==\n",
                       cap_group_buf[i]->cap_group_name);
                for_each_in_list (thread,
                                  struct thread,
                                  node,
                                  &(cap_group_buf[i]->thread_list)) {
                        print_thread(thread);
                }
                printk("\n");
        }
}

struct sched_ops rr = {.sched_init = rr_sched_init,
                       .sched = rr_sched,
                       .sched_enqueue = rr_sched_enqueue,
                       .sched_dequeue = rr_sched_dequeue,
                       .sched_top = rr_top};
