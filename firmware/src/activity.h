#pragma once

void  activityInit();
void  activityTick();
bool  activityAckConsume();
bool  activityImuOk();      // false = MPU-6050 never answered on I2C
float activityMotion();     // smoothed ||a| - 1g| in g: 0 = dead still
