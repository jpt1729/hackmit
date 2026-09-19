#pragma once
#include <Arduino.h>

void     eventsInit();
uint32_t addEvent(const char* type, const char* detail);
String   eventsJsonSince(uint32_t sinceId);
