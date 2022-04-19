/*******************************************************
 * Authors: Parker Ottaway, Derek Allen, Auston Hein
 * 
 * Description: This is a kernel module that takes in a 
 *              task and outputs the number of pages in
 *              use (the Working Set Size, aka WSS)
 *              every 5 seconds. 
 * 
 * Purpose: To understand the concept of the Working Set
 *          of pages that a task uses.
 * 
 ******************************************************/








#include <linux/init.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/sched/signal.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <asm/pgtable.h>
#include <asm/mmu_context.h>

unsigned long timer_interval_ns = 5e9; /* Call function every five seconds. */
static struct hrtimer hr_timer;
struct task_struct * task;
pgd_t * pgd;
p4d_t * p4d;
pud_t * pud;
pmd_t * pmd;
pte_t * ptep;
/* Working set size. */
unsigned long wss = 0;
int ii;

/* Stops the kernel from being reported as "tainted." */
MODULE_LICENSE("GPL");
/* Name the module's authors. */
MODULE_AUTHOR("Parker Ottaway, Derek Allen, and Auston Hein");

/* Variable to hold the PID of the process we want to get the
   information of. */
int PID;

/* Accept the process ID of a process running on the machine. */
module_param(PID, int, S_IRUSR);

/* Function to either count the WSS or reset all the access bits to zero. */
void walkPageTable( bool mode ) {
	struct vm_area_struct * vas;

	/* Point to the first virtual memory area belonging to
           the task. */
    vas = task->mm->mmap;

	/* Iterate through all the task's virtual address spaces. */
    while(vas) {
        /* Go through all the contiguous pages belonging to
           the task's virtual memory area. */
        for(ii = vas->vm_start; ii <= (vas->vm_end-PAGE_SIZE); ii+= PAGE_SIZE) {
            /* Get global directory. */
            pgd = pgd_offset(task->mm,ii);
            /* Get 4th directory. */
            p4d = p4d_offset(pgd,ii);
            /* Get upper directory. */
            pud = pud_offset(p4d,ii);
            /* Get middle directory. */
            pmd = pmd_offset(pud,ii);
            /* Get the page table entry. */
            ptep = pte_offset_map(pmd,ii);
            /* Lock the page to read it. */
            down_read(&task->mm->mmap_sem);

			/* DO STUFF HERE. */
			if( mode == true ) { // We are clearing the accessed bit.
				pte_mkold(*ptep);
			} else { // We are counting the WSS of the given process.
				if( pte_young(*ptep) ) {
					wss = wss + 1;
				}
			}

			/* Unlock the page from read lock. */
            up_read(&task->mm->mmap_sem);
		}
		/* Get the next virtual address space belonging to the
           task. */
		vas = vas->vm_next;
	}
}

/* Function called when 5 seconds are up. */
enum hrtimer_restart timer_callback(struct hrtimer *timer_for_restart) {
    ktime_t currtime , interval;
    currtime = ktime_get();
    interval = ktime_set(0, timer_interval_ns);
    hrtimer_forward(timer_for_restart, currtime, interval);

	for_each_process(task) {
        /* Look for the task with our given PID. */
        if(task->pid == PID) {
            /* We found the task, now we want to escape the for
               loop and begin traversing the 5-level page table. */
            goto found;
        }
    }
	found:

	walkPageTable(false);
	printk("[%d]:[%lu]",PID,wss);
	wss = 0;
	walkPageTable(true);

    return HRTIMER_RESTART;
}

/* Function to be run initially when the module is added (Ran only once). */
int thrashMod_init(void) {
    ktime_t ktime;
	ktime = ktime_set(0, timer_interval_ns); 
    hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    hr_timer.function = &timer_callback;
    hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL);

    return 0;

}

/* Function to be run when the module is removed. */
void thrashMod_exit(void) {
    int ret;
    ret = hrtimer_cancel(&hr_timer);
    if(ret) {
        printk("Timer was still in use!\n");
    }
    printk("HR Timer removed\n");
}



module_init(thrashMod_init);
module_exit(thrashMod_exit);
