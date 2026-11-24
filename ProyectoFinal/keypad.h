#ifndef KEYPAD_H_
#define KEYPAD_H_
void KEYPAD_Init(void);
void KEYPAD_Task(void);
char KEYPAD_GetKey(void); // consume un evento
#endif
