#pragma once

#include "ShadowDevice.h"

bool startPubSubService();
void enablePubSub();
void publishSensorData();
void publishIrrcData();
void publishShadowStatus(ShadowDevice* shadow);
