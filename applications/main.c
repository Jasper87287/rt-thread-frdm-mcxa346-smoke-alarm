#include <rtthread.h>

int sample_thread_init(void);
int control_thread_init(void);

int main(void)
{
    rt_kprintf("=== 机房监测终端 (SIM MODE) ===\n");
    sample_thread_init();
    control_thread_init();
    return 0;
}
