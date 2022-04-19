/**********************************************************************
 * Authors: Parker Ottaway, Derek Allen, Auston Hein
 * 
 * Description: This is a kernel module that takes in a
 * 				task, walks the page table and outputs
 * 				the starting and ending addresses of
 * 				100 contiguous pages of different kinds
 * 				of memory, including invalid pages, 
 * 				pages swapped to the disk, and pages
 * 				present in memory.
 * 
 * Purpose: To understand how data is stored and 
 * 			accessed in a computer.
 * 
 **********************************************************************/





#include <linux/init.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/mm_types.h>
#include <linux/types.h>
#include <linux/sched/signal.h>
#include <asm/pgtable.h>
#include <asm/mmu_context.h>

/* Define the number of similar pages in a row. */
#define CONTIG_PAGES 100

/* Stops the kernel from being reported as "tainted." */
MODULE_LICENSE("GPL");
/* Name the module's authors. */
MODULE_AUTHOR("Parker Ottaway, Derek Allen, and Auston Hein");

/* Variable to hold the PID of the process we want to get the
   information of. */
int PID;

/* Accept the process ID of a process running on the machine. */
module_param(PID, int, S_IRUSR);

/* Function to be initially run when the module is added to the
   kernel. */
int memMod_init(void) {
	struct task_struct * task;
	struct vm_area_struct * vas;
	int ii;
	int	physicalMemCount = 0;
	int swapCount = 0;

	int contiguousPhysical = 0;
	int contiguousSwap = 0;
	int contiguousInvalid = 0;
	unsigned long beginAddressPhysical = 0;
	unsigned long endAddressPhysical = 0;
	unsigned long beginAddressSwap = 0;
	unsigned long endAddressSwap = 0;
	unsigned long beginAddressInvalid = 0;
	unsigned long endAddressInvalid = 0;

	pgd_t * pgd;
	p4d_t * p4d;
	pud_t * pud;
	pmd_t * pmd;
	pte_t * ptep;
	
	/* Find the process with matching PID. */
	for_each_process(task) {
		/* Look for the task with our given PID. */
		if(task->pid == PID) {
			/* We found the task, now we want to escape the for
			   loop and begin traversing the 5-level page table. */
			goto found;
		}
	}
	
	/* Return error since the task could not be found. */
	return 1;
	
	/* We found the task, now we traverse the five layers. */
	found:
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
				
				/* Check for valid/invalid. */
				if(!pte_none(*ptep)) {
					
					/* Check if in memory or on disk. */
					if(pte_present(*ptep)) { /* Page is in memory. */
						physicalMemCount++;
						/* If the physical memory chunk has not been found yet. */
						if(contiguousPhysical != CONTIG_PAGES + 1) {
							contiguousPhysical++;
						}
						
						/* Track first address. */
						if(contiguousPhysical == 1) {
							beginAddressPhysical = ii;
						}
						
						/* If others are non-zero, set them to zero. */
						if(contiguousSwap != CONTIG_PAGES + 1) {
							contiguousSwap = 0;
							beginAddressSwap = 0;
						}
						if(contiguousInvalid != CONTIG_PAGES + 1) {
							contiguousInvalid = 0;
							beginAddressInvalid = 0;
						}
						
						/* Save ending address. */
						if(contiguousPhysical == CONTIG_PAGES) {
							endAddressPhysical = ii;
							printk("Starting Physical Address: %lu\tEnding Physical Address: %lu", beginAddressPhysical, endAddressPhysical);
							/* Set contiguous counter to 101 so that it does not keep printing. */
							contiguousPhysical++;
						}
					} else { /* Page is swapped. */
						/* Increment counters. */
						swapCount++;
						/* If the swap memory chunk has not been found yet. */
						if(contiguousSwap != CONTIG_PAGES + 1) {
							contiguousSwap++;
						}

						/* Track first address. */
						if(contiguousSwap == 1) {
							beginAddressSwap = ii;
						}

						/* If others are non-zero, set them to zero. */
						if(contiguousPhysical != CONTIG_PAGES + 1) {
							contiguousPhysical = 0;
							beginAddressPhysical = 0;
						}
						if(contiguousInvalid != CONTIG_PAGES + 1) {
							contiguousInvalid = 0;
							beginAddressInvalid = 0;
						}

						/* Save ending address. */
						if(contiguousSwap == CONTIG_PAGES) {
							endAddressSwap = ii;
							printk("Starting Swap Address: %lu\tEnding Swap Address: %lu", beginAddressSwap, endAddressSwap);
							/* Set contiguous counter to 101 so that it does not keep printing. */
							contiguousSwap++;
						}
							
					}
				} else { /* Page is invalid. */
						/* If the invalid memory chunk has not been found yet. */
						if(contiguousInvalid != CONTIG_PAGES + 1) {
							contiguousInvalid++;
						}

						/* Track first page. */
						if(contiguousInvalid == 1) {
							beginAddressInvalid = ii;
						}

						/* If others are non-zero, set them to zero. */
						if(contiguousPhysical != CONTIG_PAGES + 1) {
							contiguousPhysical = 0;
							beginAddressPhysical = 0;
						}
						if(contiguousSwap != CONTIG_PAGES + 1) {
							contiguousSwap = 0;
							beginAddressSwap = 0;
						}

						/* Save ending address. */
						if(contiguousInvalid == CONTIG_PAGES) {
							endAddressInvalid = ii;
							printk("Starting Invalid Address: %lu\tEnding Invalid Address: %lu", beginAddressInvalid, endAddressInvalid);
							/* Set contiguous counter to 101 so that it does not keep printing. */
							contiguousInvalid++;
						}
				}

				/* Unlock the page from read lock. */
				up_read(&task->mm->mmap_sem);
			}
			/* Get the next virtual address space belonging to the
			   task. */
			vas = vas->vm_next;
		}
	
	/* Print the PID, number of pages in memory, and number of pages swapped. */
	printk("PID: %d\tPRESENT: %d\tSWAPPED: %d",task->pid,physicalMemCount,swapCount);
	printk("GOT HERE.");
	return 0;
}

/* Module function that is run when the module is removed from the
   kernel. This will mainly do cleanup. */
void memMod_exit(void) {
	
}

/* Define the init and exit functions for this kernel module. */
module_init(memMod_init);
module_exit(memMod_exit);
