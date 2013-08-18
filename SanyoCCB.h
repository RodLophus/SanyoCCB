#ifndef SanyoCCB_h
#define SanyoCCB_h

#include <inttypes.h>
#include "Arduino.h"

#define _CCB_SEND    0
#define _CCB_RECEIVE 1

class SanyoCCB {
	public:
		SanyoCCB(uint8_t, uint8_t, uint8_t, uint8_t);
		void init();
		uint8_t diPinState();
		void read(byte, byte*, int8_t);
		void write(byte, byte*, int8_t);
	private:
		void writeByte(byte);
		byte readByte();
		void ccb(byte, byte*, int8_t, uint8_t);
		int _do_pin;
		int _cl_pin;
		int _di_pin;
		int _ce_pin;
};

#endif

