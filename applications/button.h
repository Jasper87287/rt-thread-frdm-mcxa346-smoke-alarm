#ifndef __BUTTON_H__
#define __BUTTON_H__

#include <rtthread.h>

typedef void (*key_event_cb_t)(uint8_t key_id);

void button_init(key_event_cb_t cb);

#endif
