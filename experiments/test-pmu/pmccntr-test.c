#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("averageTalking");
MODULE_DESCRIPTION("Counter Test Kernel Modul");



// PMCCNTR_EL0 Test Modul
static int __init kernel_module_init(void) {
    unsigned long writeValue;
    unsigned long t1_pmu, t2_pmu;
    unsigned long t1_cntvct, t2_cntvct;
    printk(KERN_INFO "Counter Test Kernel Module loaded\n");


    writeValue = 0x80000000;
    asm volatile("MSR PMCNTENSET_EL0, %0":: "r" (writeValue));
    writeValue = 0x00000007;
    asm volatile("MSR PMCR_EL0, %0":: "r" (writeValue));
    asm volatile("MRS %0, PMCCNTR_EL0":"=r"(t1_pmu));
    asm volatile("mrs %0, cntvct_el0" : "=r"(t1_cntvct));

    int i;
    for (i = 0; i < 1000000; i++) {
        asm volatile("nop");
    }

    asm volatile("MRS %0, PMCCNTR_EL0":"=r"(t2_pmu));
    asm volatile("mrs %0, cntvct_el0" : "=r"(t2_cntvct));

    printk(KERN_INFO "PMCCNTR_EL0 before: %lu, after: %lu, delta: %lu\n", t1_pmu, t2_pmu, t2_pmu - t1_pmu);
    printk(KERN_INFO "CNTVCT_EL0 before: %lu, after: %lu, delta: %lu\n", t1_cntvct, t2_cntvct, t2_cntvct - t1_cntvct);
    return 0;
}

static void __exit kernel_module_exit(void)
{
    printk(KERN_INFO "Counter Test Kernel Module unloaded\n");
}

module_init(kernel_module_init);
module_exit(kernel_module_exit);
