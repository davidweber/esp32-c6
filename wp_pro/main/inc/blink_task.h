#ifndef __BLINK_TASK_H__
#define __BLINK_TASK_H__

#define LED_GPIO         5
#define BLINK_TASK_TAG   "BLINK_TASK_TAG"

void blink_task(void *param);
void blink_task_create(void);

#endif
