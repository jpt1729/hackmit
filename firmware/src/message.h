#pragma once
#include <stdint.h>

// A short note from the caregiver, shown on the OLED until the wearer shakes
// it away. The dashboard already records voice messages with a transcript;
// this is where that transcript lands on the wrist.
void        messageInit();
void        messageSet(const char* text);   // POST /message
void        messageTick();
void        messageDismiss();               // shake, or POST /message/dismiss
bool        messagePending();
const char* messageText();
