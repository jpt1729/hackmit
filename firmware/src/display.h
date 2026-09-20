#pragma once
#include <stdint.h>

void displayInit();
void displayTick();
void displayFlash(const char* text, uint32_t ms);
void displayFooter(const char* text);   // persistent bottom line: the device IP
void displaySplash(uint32_t ms);        // mascot + name, held over everything else
