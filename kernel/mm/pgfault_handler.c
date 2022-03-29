#include <common/macro.h>
#include <common/util.h>
#include <common/list.h>
#include <common/errno.h>
#include <common/kprint.h>
#include <common/types.h>
#include <lib/printk.h>
#include <mm/vmspace.h>
#include <mm/kmalloc.h>
#include <mm/mm.h>
#include <mm/vmspace.h>
#include <arch/mmu.h>
#include <object/thread.h>
#include <object/cap_group.h>
#include <sched/context.h>

int handle_trans_fault(struct vmspace *vmspace, vaddr_t fault_addr)
{
        struct vmregion *vmr;
        struct pmobject *pmo;
        paddr_t pa;
        u64 offset;
        u64 index;
        int ret = 0;

        vmr = find_vmr_for_va(vmspace, fault_addr);
        if (vmr == NULL) {
                printk("handle_trans_fault: no vmr found for va 0x%lx!\n",
                       fault_addr);
                kinfo("process: %p\n", current_cap_group);
                print_thread(current_thread);
                kinfo("faulting IP: 0x%lx, SP: 0x%lx\n",
                      arch_get_thread_next_ip(current_thread),
                      arch_get_thread_stack(current_thread));

                kprint_vmr(vmspace);
                kwarn("TODO: kill such faulting process.\n");
                return -ENOMAPPING;
        }

        pmo = vmr->pmo;
        switch (pmo->type) {
        case PMO_ANONYM:
        case PMO_SHM: {
                vmr_prop_t perm;

                perm = vmr->perm;

                /* Get the offset in the pmo for faulting addr */
                offset = ROUND_DOWN(fault_addr, PAGE_SIZE) - vmr->start;
                BUG_ON(offset >= pmo->size);

                /* Get the index in the pmo radix for faulting addr */
                index = offset / PAGE_SIZE;

                fault_addr = ROUND_DOWN(fault_addr, PAGE_SIZE);
                /* LAB 3 TODO BEGIN */
                pa = get_page_from_pmo(pmo, index);

                /* LAB 3 TODO END */
                if (pa == 0) {
                        /* Not committed before. Then, allocate the physical
                         * page. */
                        /* LAB 3 TODO BEGIN */
                        void *pgtbl = get_pages(0);
                        pa = virt_to_phys(pgtbl);
                        memset(pgtbl, 0, PAGE_SIZE);
                        commit_page_to_pmo(pmo, index, pa);
                        map_range_in_pgtbl(vmspace->pgtbl, fault_addr, pa, PAGE_SIZE, vmr->perm);

                        /* LAB 3 TODO END */
#ifdef CHCORE_LAB3_TEST
                        printk("Test: Test: Successfully map\n");
#endif
                } else {
                        /*
                         * pa != 0: the faulting address has be committed a
                         * physical page.
                         *
                         * For concurrent page faults:
                         *
                         * When type is PMO_ANONYM, the later faulting threads
                         * of the process do not need to modify the page
                         * table because a previous faulting thread will do
                         * that. (This is always true for the same process)
                         * However, if one process map an anonymous pmo for
                         * another process (e.g., main stack pmo), the faulting
                         * thread (e.g, in the new process) needs to update its
                         * page table.
                         * So, for simplicity, we just update the page table.
                         * Note that adding the same mapping is harmless.
                         *
                         * When type is PMO_SHM, the later faulting threads
                         * needs to add the mapping in the page table.
                         * Repeated mapping operations are harmless.
                         */
                        /* LAB 3 TODO BEGIN */
                        // if(pmo->type == PMO_ANONYM){

                        // }
                        // else{
                                
                        // }
                        unmap_range_in_pgtbl(vmspace->pgtbl, fault_addr, PAGE_SIZE);
                        map_range_in_pgtbl(vmspace->pgtbl, fault_addr, pa, PAGE_SIZE, vmr->perm);

                        /* LAB 3 TODO END */
#ifdef CHCORE_LAB3_TEST
                        printk("Test: Test: Successfully map for pa not 0\n");
#endif
                }

                /* Cortex A53 has VIPT I-cache which is inconsistent with
                 * dcache. */
#ifdef CHCORE_ARCH_AARCH64
                if (vmr->perm & VMR_EXEC) {
                        extern void arch_flush_cache(u64, s64, int);
                        /*
                         * Currently, we assume the fauling thread is running on
                         * the CPU. Thus, we flush cache by using user VA.
                         */
                        BUG_ON(current_thread->vmspace != vmspace);
                        /* 4 means flush idcache. */
                        arch_flush_cache(fault_addr, PAGE_SIZE, 4);
                }
#endif

                break;
        }
        case PMO_FORBID: {
                kinfo("Forbidden memory access (pmo->type is PMO_FORBID).\n");
                BUG_ON(1);
                break;
        }
        default: {
                kinfo("handle_trans_fault: faulting vmr->pmo->type"
                      "(pmo type %d at 0x%lx)\n",
                      vmr->pmo->type,
                      fault_addr);
                kinfo("Currently, this pmo type should not trigger pgfaults\n");
                kprint_vmr(vmspace);
                BUG_ON(1);
                return -ENOMAPPING;
        }
        }

        return ret;
}
