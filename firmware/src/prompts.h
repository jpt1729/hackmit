#pragma once

void promptsInit();
void promptsTick();
void promptsDemoFire(int idx);
bool promptsAckPending();          // shake, or POST /ack from the dashboard
int  promptsFindById(const char* id);
void promptsScheduleChanged();     // the dashboard pushed a new routine
