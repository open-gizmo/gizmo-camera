#ifndef SERIAL_H
#define SERIAL_H

#include <Arduino.h>
#include <gizmo/log.h>

// Init serial
void initSerial();

// Logging comes from gizmo-common
using gizmo::printLog;
using gizmo::printLogf;
using gizmo::printLogfln;
using gizmo::printLogln;

#endif