#pragma once
#include "webserver.h"

void registerConfigRESTHandler(WebServer* server);
void registerIrRESTHandler(WebServer* server);
void registerSensorRESTHandler(WebServer* server);
