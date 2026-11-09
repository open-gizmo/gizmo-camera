#include "serial.h"

// Init serial
void initSerial() {
	gizmo::initLog(); // Off in the released firmware, on in the debug build (pio run -e debug)
}