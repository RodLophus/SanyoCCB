/*

This library implements the Sanyo CCB (Computer Control Bus).

CCB is a chip-to-chip communication protocol developed by Sanyo.
It is similar to Philips´ I2C in its purpose, but  much simpler.

               ------------------------------------

Copyright (c) 2013 Rodolfo Broco Manin.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/


#include <inttypes.h>
#include "SanyoCCB.h"

// Base delay (us).  Also used to time the CL (clock) line.
// 100us should be enough even for slow CCB devices.
#define CCB_DELAY 100


/******************************\
 *        Constructor         *
 *  just set class variables  *
\******************************/
SanyoCCB::SanyoCCB(uint8_t do_pin, uint8_t cl_pin, uint8_t di_pin, uint8_t ce_pin) {
	_do_pin = do_pin;
	_cl_pin = cl_pin;
	_di_pin = di_pin;
	_ce_pin = ce_pin;
}


/******************************************\
 *                 init()                 *
 *  Set pin functions and initial states  *
\******************************************/ 
void SanyoCCB::init() {
	pinMode(_do_pin, OUTPUT);
	pinMode(_cl_pin, OUTPUT);
	pinMode(_ce_pin, OUTPUT);
  
	pinMode(_di_pin, INPUT);
	digitalWrite(_di_pin, HIGH);
  
	digitalWrite(_do_pin, LOW);
	digitalWrite(_cl_pin, LOW);  // Clock-rest-low mode

	// Paranoia: cycle CE to "flush" de bus
	digitalWrite(_ce_pin, HIGH); delayMicroseconds(CCB_DELAY);
	digitalWrite(_ce_pin, LOW); delayMicroseconds(CCB_DELAY);	
}


/************************************\
 *           writeByte()            *
 *  Send a single byte via CCB bus  *
\************************************/ 
void SanyoCCB::writeByte(byte data) {
	// Send one byte out bia CCB bus (LSB first)
	for(int8_t i = 0; i <= 7; i++) {
		digitalWrite(_do_pin, bitRead(data, i));
		digitalWrite(_cl_pin, HIGH); delayMicroseconds(CCB_DELAY);
		digitalWrite(_cl_pin, LOW); delayMicroseconds(CCB_DELAY);
	}
}


/***************************************\
 *             readByte()              *
 *  Receive a single byte via CCB bus  *
\***************************************/ 
byte SanyoCCB::readByte() {
	byte data = 0;
	// Receive one byte from the CCB bus (MSB first)
	for(int8_t i = 7; i >= 0; i--) {
		digitalWrite(_cl_pin, HIGH); delayMicroseconds(CCB_DELAY);
		bitWrite(data, i, digitalRead(_di_pin));
		digitalWrite(_cl_pin, LOW); delayMicroseconds(CCB_DELAY);
	}
	return data;
}


/*****************************************\
 *                 ccb()                 *
 *  The universal send/receive function  *
\*****************************************/
void SanyoCCB::ccb(byte address, byte *data, int8_t dataLength, uint8_t mode) {
	int8_t i; // i may reach negative values in the counters
	          // dataLength is typed "int8_t" for compatibility with this counter
	
	// Send the address, with the nibbles swapped (required by the CCB protocol to support 4-bit addresses)
	writeByte((address >> 4) | (address << 4));
  
	// Enter the data transfer mode
	digitalWrite(_cl_pin, LOW);
	digitalWrite(_ce_pin, HIGH); delayMicroseconds(CCB_DELAY);
 
	switch(mode) {
		case _CCB_SEND:
		// Send data
		// Note: as CCB devices usually reads registers data from MSB to LSB, the buffer is read from left to right
		for(i = dataLength - 1; i >= 0; i--)
				writeByte(data[i]);
		digitalWrite(_do_pin, LOW);
		break;
      
	case _CCB_RECEIVE:
		// Receive data
		for(i = 0; i < dataLength; i++)
			data[i] = readByte();
		break;
	}

	digitalWrite(_ce_pin, LOW); delayMicroseconds(CCB_DELAY);
}


/*********************************************************\
 *                      diPinState()                     *
 *  Return the state of the DI pin                       *
 * Some CCB devices uses the DO pin for other functions  *
 * when the data bus is idle.  This method makes reading *
 * it easier                                             *
\*********************************************************/
uint8_t SanyoCCB::diPinState() {
	return digitalRead(_di_pin);
}


/********************************************************\
 *                     write()                          *
 *  Send dataLength (up to 127) bytes via CCB bus       *
 * Note: the contents of the input buffer is send       *
 * backwards (from the rightmost to the leftmost byte), *
 * so the order of the data bytes must be the opposite  *
 * as the one shown on the device's datasheets          *
\********************************************************/ 
void SanyoCCB::write(byte address, byte *data, int8_t dataLength) {
	ccb(address, data, dataLength, _CCB_SEND);
}


/******************************************************\
 *                      read()                        *
 *  receive dataLength (up to 127) bytes via CCB bus  *
\******************************************************/ 
void SanyoCCB::read(byte address, byte *data, int8_t dataLength) {
	ccb(address, data, dataLength, _CCB_RECEIVE);
}