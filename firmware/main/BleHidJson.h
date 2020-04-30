#pragma once

#include "BleHidService.h"
#include <json11.hpp>

BleHidCodeData bleHidJsonToData(const json11::Json &json);
json11::Json::object bleHidDataToJson(const BleHidCodeData &data);
