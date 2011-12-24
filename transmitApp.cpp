/*
MicroGPIO application, transmitApp.cpp
By Harry Johnson
This file is part of microGPIO.
 
microGPIO is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
 
microGPIO is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser Public License for more details.
 
You should have received a copy of the GNU Lesser Public License
along with microGPIO.  If not, see <http://www.gnu.org/licenses/>.
 
To get this working, put it in the demo folder of the Microtouch source tree. Note that 
if you have the default programs, this WILL exceed the Microtouch's memory, giving you a verify error. 
To fix this, remove some of the larger default programs. (Doomed, etc)
*/

#include "Platform.h"
#include "mmc.h"
#include "Board.h"

#define BUTTONH 35
#define BUTTONW 35
#define NUMBUTTONS 6

//on-screen button code
class Button //We are simulating latching button. Press to press, press again to release.
{
	int w,h,color; //button width, height, and color
public:
	int x, y, pressed; //X location, Y location, and pressed or not. (1/0)
	Button(int x_tmp, int y_tmp, int w_tmp, int h_tmp, int color_tmp); //constructor
	Button(); //simple constructor
	void setParams(int x_tmp, int y_tmp, int w_tmp, int h_tmp, int color_tmp); //set all parameters, redraw
	void setPressed(int x); //set whether the button is pressed or not. 
	int contains(int x_tmp, int y_tmp); //check if a button contains a given point.
	void draw(); //draw the button
};

void Button::draw() { 
	// Rectangle. Black if released, color (defined above) if pressed.
	Graphics.Rectangle(x, y, w, h, color - (color*(1-pressed)));

}

void Button::setPressed(int x) { //1 for pressed, 0 for not pressed.
	pressed = x;	
}

int Button::contains(int x_tmp, int y_tmp) { //check if point is contained in button. 
	if((x_tmp>x) && (x_tmp< x+w) && (y_tmp>y) && (y_tmp< y+w)) return 1; //rectangle, this makes it easy!
	return 0;
}

Button::Button(int x_tmp, int y_tmp, int w_tmp, int h_tmp, int color_tmp) { //constructor just sets params.
	setParams(x_tmp, y_tmp, w_tmp, h_tmp, color_tmp);
	draw(); //redraw
}

Button::Button() { //this makes array of buttons declaration easy.
	
}

void Button::setParams(int x_tmp, int y_tmp, int w_tmp, int h_tmp, int color_tmp) { //mass assignment.
	x = x_tmp;
	y = y_tmp;
	w = w_tmp;
	h = h_tmp;
	color = color_tmp;
	pressed = 0;
	draw();
}
//end on-screen button code

class transmitState //main state.
{
public:
	u8 DTransmitValue; //value of port D.
	Button Dbuttons[NUMBUTTONS]; //array of buttons
	
	void transmitLED() { //transmit the command via SPI. 
		SPI_Enable(); //enable SPI
		SPCR = 0x53;  //slowest clock speed. (doesn't really matter, still plenty fast.)
		MMC_SS_LOW(); //wake up the target board.
		delay(5); //give it time to process and set up SPI.
		SPI_ReceiveByte('D'); //changing value of port D.
		SPI_ReceiveByte(DTransmitValue); //to this number.
		MMC_SS_HIGH(); //stop target from listening
		SPI_Disable(); //disable SPI.
	}
	
	int OnEvent(Event* e)
	{
		switch (e->Type)
		{
			case Event::OpenApp: { //when first opened
				Graphics.DrawString("Pins D0-D5", 45, 10, 0); //let people know what buttons do.
				
				for(u8 ii = 0; ii<NUMBUTTONS; ii++) { //set up all buttons with width, location, color.
					Dbuttons[ii].setParams(45, 20 + (ii*(BUTTONH+8)), BUTTONW, BUTTONH, Graphics.ToColor(255, 0, 0)); //autoplace the buttons in a vertical line. 	
				}
				
				DTransmitValue = 0; //initialize the port value.
				SPI_Init(); //Init SPI.
				SPI_Disable(); //disable, to let the LCD work again.
				transmitLED(); //transmit the 0 value, inits target.
			}
			break;
				
			case Event::TouchDown: { //LCD screen pressed.
				TouchData* t = (TouchData*)e->Data; //store the data about the touch.
			
				for(u8 jj = 0; jj<NUMBUTTONS; jj++) { //poll each button, to see if it is touched.
					if(Dbuttons[jj].contains((t->x),( t->y)) ==1) {
						Dbuttons[jj].setPressed(1-Dbuttons[jj].pressed); //if the button is touched, toggle the button's state (on/off).
						DTransmitValue |= (Dbuttons[jj].pressed<<jj); //line handles if the button is on. OR the value with this appropriate pin mask. (turns on bit)
						DTransmitValue &= ~((1-Dbuttons[jj].pressed)<<jj); //line handles if the button is off. AND the value with the inverted pin mask. (turns off bit) 
					}
					Dbuttons[jj].draw(); //redraw the button.
				}
				transmitLED(); //update the target.
				
				if (t->y > 320) { //touched the bottom black bar.
					return -1;		// Quit
				} 	
			}
			break;
				
			default:
				;
		}
		return 0;
	}
};

INSTALL_APP(microGPIO,transmitState);