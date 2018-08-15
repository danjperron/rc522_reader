/*
 * main.h
 *
 *  Created on: 26.09.2013
 *      Author: alexs
 */

#ifndef MAIN_H_
#define MAIN_H_

extern uint8_t debug;
extern void my_spi_transfer(unsigned char *txbuff, unsigned char *rxbuff, int len);


#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */


#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

int  mem_fd;
void *gpio_map;

extern volatile unsigned long *GPIO;


// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(GPIO+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(GPIO+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(GPIO+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(GPIO+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(GPIO+10) // clears bits which are 1 ignores bits which are 0

#define GPIO_READ(g)  (*(GPIO + 13) &= (1<<(g)))




#endif /* MAIN_H_ */
