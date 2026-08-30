#ifndef __RELAY_H__
#define __RELAY_H__

#include <stdbool.h>

void relay_init(void);
void relay_turn_on(void);
void relay_turn_off(void);
void relay_toggle(void);
bool relay_is_on(void);

#endif //__RELAY_H__