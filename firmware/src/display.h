#pragma once
#include <stdint.h>

void displayInit();
void displayTick();
void displayFlash(const char* text, uint32_t ms);
