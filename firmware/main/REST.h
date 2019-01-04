#pragma once
#include "webserver.h"

void registerConfigRESTHandler(WebServer* server);
void registerStatusRESTHandler(WebServer* server);
void registerIrRESTHandler(WebServer* server);
void registerSensorRESTHandler(WebServer* server);
void registerShadowRESTHandler(WebServer* server);
void registerBleHidRESTHandler(WebServer* server);
