#pragma once

#ifdef __cplusplus
#include "webserver.h"
extern "C" {
#endif

bool startWebService();

#ifdef __cplusplus
}
const WebServer::Stat* getWebStat();
#endif
