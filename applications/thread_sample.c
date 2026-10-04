#include "hal.h"
#include "app_config.h"

static struct rt_messagequeue sample_mq;
static rt_uint8_t mq_buf[128];



/* 限幅宏（宏定义放文件开头） */


static void sample_thread_entry(void *param) {
    env_data_t data;
    /* 滤波后的数据缓存 */
    static env_data_t filtered_data = {25.0f, 50.0f, 5.0f, 300.0f};
    #define FILTER_ALPHA  0.3f

    hal_sensor_init();

    while (1) {
        hal_sensor_read(&data);

        /* 一阶低通滤波 */
        filtered_data.temperature = filtered_data.temperature * (1 - FILTER_ALPHA) + data.temperature * FILTER_ALPHA;
        filtered_data.humidity    = filtered_data.humidity    * (1 - FILTER_ALPHA) + data.humidity    * FILTER_ALPHA;
        filtered_data.smoke       = filtered_data.smoke       * (1 - FILTER_ALPHA) + data.smoke       * FILTER_ALPHA;
        filtered_data.light       = filtered_data.light       * (1 - FILTER_ALPHA) + data.light       * FILTER_ALPHA;

        /* 边界钳位保护 —— 必须放在函数内部 */
     

        /* 发送滤波后的数据 */
        rt_mq_send(&sample_mq, &filtered_data, sizeof(env_data_t));
        rt_thread_mdelay(SAMPLE_PERIOD);
    }
}

rt_mq_t get_sample_mq(void) { return &sample_mq; }

int sample_thread_init(void) {
    rt_mq_init(&sample_mq, "sample_mq", mq_buf,
               sizeof(env_data_t), SAMPLE_MQ_BUF_SIZE, RT_IPC_FLAG_FIFO);

    rt_thread_t tid = rt_thread_create("sample", sample_thread_entry,
                                       RT_NULL, SAMPLE_THREAD_STACK, 10, 10);
    if (tid) rt_thread_startup(tid);
    return 0;
}
