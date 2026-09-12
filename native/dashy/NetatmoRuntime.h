#pragma once
#include "Readings.h"
#include <cstdint>
namespace dashy {
bool startNetatmo();
Readings netatmoReadings();
bool pauseNetatmo();
void resumeNetatmo(uint32_t suspendedSeconds);
}
