/*
Filename: main.c for GPIO expander for the Adafruit/Rossum Microtouch.
Copyright (C) 2011  Harry Johnson

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser Public License for more details.

You should have received a copy of the GNU Lesser Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

Note: This was intended for use with the Microtouch, see www.adafruit.com, but was NOT designed or
reviewed by the original creators of the Microtouch.
*/

#include <avr/io.h> //ATTiny2313 Pin Definitions
#include <avr/interrupt.h> //interrupts.


#define USI_PORT PORTB
#define USI_PORT_IN PINB
#define USI_PORT_DIR DDRB
#define CS_PIN PB4
#define DI_PIN PB5
#define DO_PIN PB6
#define SCK_PIN PB7

//Pin Definitions
#define SPILED PB3
//End of pin definitions

volatile unsigned char heldUSIDR = 0; //the current data value.
unsigned char cmd[2] = {0}; //2 byte command/argument.

int expectByte = 0; //for command arguments.
int setConflict = 0; //is there a conflict on the SPI bus?
volatile int new =0; //Is the byte new?
volatile int SPI_enabled =0; //SPI enabled flag.

//SPI slave mode only.
void SPI_init(); //initialize SPI
void SPI_enable(); //enable SPI, reset counter, reset data register
void SPI_disable(); //disable SPI
unsigned char SPI_get(); //get the latest byte.
int SPI_isBusy(); //is the port in the process of shifting?
int SPI_put(unsigned char val); //queue up a byte.
int evalCommand(unsigned char command[2]);

int main(void)
{
	DDRD = 0xFF; //all output on port D.
	DDRB |= 0x0F; //outputs on the lower 4 bits.
	SPI_init(); //initialize the USI for slave SPI, set up pins.
    
	PORTD = 0; //turn off all inputs.
	
	for(;;){
		if(new==1) { //there is a new byte.
			if(expectByte == 0) { //this is the start of a new command.
				cmd[0] = SPI_get(); //store command.
				evalCommand(cmd); //evaluate the command, which here means say we anticipate a new byte.
			} else {
				cmd[1] = SPI_get(); //store argument.
				evalCommand(cmd); //evaluate command + argument.
				expectByte = 0; //back to waiting for a new command.
			}
		}
	} 
    return 0; //shouldn't reach here.
}

int evalCommand(unsigned char command[2]) {
	switch(command[0]) { //D, B turn on respective ports. More commands coming later.
		case 'D': if(expectByte==0) {expectByte=1;} else {PORTD = command[1]; } return 0; break; //if expectByte = 0, then this is the first byte of the command. 
		case 'B': if(expectByte==0) {expectByte=1;} else {PORTB = (command[1] & 0x07); } return 0; break;
		default: return 1; break; //error
	}	
}


/* SPI Code Begins Here */

void SPI_init() { //slave only
	PCMSK = (1<<PCINT4); //only the pin change 4 interrupt (Slave Select pin) is used.
	GIMSK |= (1<<PCIE); //enable the pin change interrupts in general.
	USI_PORT_DIR &= ~((1<<CS_PIN) | (1<<DI_PIN) | (1<<SCK_PIN) | (1<<DO_PIN)); // Everything input
	USI_PORT_DIR |= (1<<SPILED); //SPI enabled LED.
	USI_PORT &= ~((1<<DI_PIN) | (1<<SCK_PIN) | (1<<DO_PIN) | (1<<SPILED)); //remove pullups for all comm pins, turn off the indicator.
	USI_PORT |= (1<<CS_PIN); //enable pullup on CS pin
	USICR = (1<<USIWM0) | (1<<USICS1) | (0<<USICS0); //set wire mode and clock source (external via SCK pin)
	sei(); //enable interrupts.
}

void SPI_enable() {
	USISR &= 0xF0; //zero out the clock, so that inputs will be fresh again.
	USI_PORT_DIR |= (1<<DO_PIN); //DO pin needs to be set as output.
	USICR |= (1<<USIOIE); //enable the interrupt. 
	USIDR = 0; //zero out the data register.
	new = 0; //not a new byte at first.
	SPI_enabled = 1; //store that the SPI is enabled.
	USI_PORT |= (1<<SPILED); //turn on the SPI indicator LED. 
}

void SPI_disable() {
	USI_PORT_DIR &= ~(1<<DO_PIN); // inputs 
	USI_PORT &= ~((1<<DI_PIN) | (1<<SCK_PIN) | (1<<DO_PIN)); //remove pullups for all other pins.
	USICR &= ~(1<<USIOIE); //disable the counter interrupt.
	SPI_enabled = 0; //flag that SPI is off.
	USI_PORT &= ~(1<<SPILED); //turn off the SPI indicator LED.
}

int SPI_isBusy() {
	if((USISR & 0x0F) != 0 ) { // USI clock timer = 0 if USI is between bytes.
		setConflict = 1; //the port is still being updated, boo.
		return 1; //busy
	}
	setConflict = 0; //no conflict, yay!
	return 0; //not busy
}

int SPI_put(unsigned char val) { //Slave doesn't get to initiate send, but it can queue up data.
	if(SPI_isBusy() == 1) { //if there is still a byte being transferred, don't put our own data there.
		return -1; //Try again later!
	}
	USIDR = val; //if the line is quiet, queue up our own value to be sent out.
	return 0;
}

unsigned char SPI_get() { //gets the most recent value.
	return heldUSIDR;	
	new = 0; //the stored byte has already been read.
}

ISR(PCINT_vect) { //Slave select pin has changed, which only occurs at the beginning and the end of SPI usage.
	if((USI_PORT_IN & (1<<CS_PIN))==0) { //active low Slave Select pin.
		SPI_enable(); //if the pin is pulled low, then enable/reset the SPI.
	} else { 
		SPI_disable(); //if the pin is high, then disable the SPI.
	}

}

ISR(USI_OVERFLOW_vect) { //USI clock timer overflowed from 15 to 0, byte transfer complete.
	heldUSIDR = USIDR; //store the data
	new = 1; //there is a new byte.
	USISR |= (1<<USIOIF); //re-enable the USI timer interrupt.
}
/* SPI code ends here. */
