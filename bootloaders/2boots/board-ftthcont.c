/**********************************************************/
/* board-arduino.c                                        */
/* Copyright (c) 2010 by thomas seiler                    */
/* This file is based on the original Arduino Bootloader  */
/* -------------------------------------------------------*/
/*                                                        */
/* This program is free software; you can redistribute it */
/* and/or modify it under the terms of the GNU General    */
/* Public License as published by the Free Software       */
/* Foundation; either version 2 of the License, or        */
/* (at your option) any later version.                    */
/*                                                        */
/* This program is distributed in the hope that it will   */
/* be useful, but WITHOUT ANY WARRANTY; without even the  */
/* implied warranty of MERCHANTABILITY or FITNESS FOR A   */
/* PARTICULAR PURPOSE.  See the GNU General Public        */
/* License for more details.                              */
/*                                                        */
/* You should have received a copy of the GNU General     */
/* Public License along with this program; if not, write  */
/* to the Free Software Foundation, Inc.,                 */
/* 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA */
/*                                                        */
/* Licence can be viewed at                               */
/* http://www.fsf.org/licenses/gpl.txt                    */
/**********************************************************/

/* some includes */
#include <inttypes.h>
#include <avr/io.h>
#include "stk500v1.h"
#include "mmc_fat.h"

/* function prototype */
void main (void) __attribute__ ((naked,noreturn,section (".init9")));

/* some variables */
const void (*app_start)(void) = 0x0000;
uint8_t reset_reason = 0;

/* main program starts here */
void main(void)
{	
	/* here we learn how we were reset */
	reset_reason = MCUSR;
	MCUSR = 0;

	/* stop watchdog */
	WDTCSR |= _BV(WDCE) | _BV(WDE);
	WDTCSR = 0;

	/* start app right ahead if this was not an external reset */
	/* this means that all the code below this line is only executed on external reset */
	if (!(reset_reason & _BV(EXTRF))) app_start();

	/* power up sd card via PC4 */
        //PORTC |= _BV(PINC4); /* pullup : activate power supplies for SD card */

	/* configure PD7 (UPDATE) as input */
        //PORTD |= _BV(PIND7); /* pullup : update button */

	/* if UPDATE is not pressed, start app */
	//if (bit_is_set(PIND, PIND7)) app_start();

	/* --- this line is only passed if we had an external reset and UPDATE was pressed --- */

	/* this is needed because of the __attribute__ naked, section .init 9 */
	/* from now, we can call functions :-) */
	asm volatile ( "clr __zero_reg__" );
	SP=RAMEND;

#ifndef DISABLE_SERIAL
	/* try first serial, to not let the programmer timeout in case there is an MMC included */
	stk500v1();
#endif

#ifndef DISABLE_MMC
	/* then try the mmc, less time critical... */
	mmc_updater();
#endif
		
	/* we reset via watchdog in order to reset all the registers to their default values */
	WDTCSR = _BV(WDE);
	while (1); // 16 ms
}


/* end of file board-ftthcont.c */
