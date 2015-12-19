// usbUtils.h
#pragma once

#include "../inc/CyAPI/CyAPI.h"

void initUSB();
void closeUSB();

CCyBulkEndPoint* getBulkEndPoint();

void startCounter();
double getCounter();

void writeReg (WORD addr, WORD data);
WORD readReg (WORD addr);

void startStreaming();
void sendFocus (int value);

void jpeg (int width, int height);
