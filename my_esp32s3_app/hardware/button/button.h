#ifndef __BUTTON_H__
#define __BUTTON_H__

#include "esp_err.h"

#define BUTTON_PIN 21

esp_err_t button_init(void);

void button_task(void *pvParam);
void button_task_stop(void);
void button_task_start(void);

#endif // __BUTTON_H__
