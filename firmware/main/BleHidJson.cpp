#include <esp_log.h>
#include <esp_system.h>
#include <vector>
#include "Config.h"
#include "BleHidJson.h"

static const char JSON_KEYCODES[] = "KeyCodes";
static const char JSON_SPECHIAL_MASK[] = "SpecialKeyMask";
static const char JSON_CONSUMER[] = "ConsumerCode";

BleHidCodeData bleHidJsonToData(const json11::Json &json){
    auto rval = BLEHID_DEFAULT_CODE;
    if (json.is_object()){
        auto keyCodes = json[JSON_KEYCODES];
        auto special = json[JSON_SPECHIAL_MASK];
        auto consumer = json[JSON_CONSUMER];
        auto duration = json[JSON_DURATION];

        if (keyCodes.is_array()){
            rval.type = BLEHID_KEYCODE;
            int i = 0;
            for (auto &code : keyCodes.array_items()){
                if (i >= BLEHID_MAX_KEYCODE_NUM || !code.is_number()){
                    break;
                }
                rval.data.keycode.keys[i] = code.int_value() & 0xff;
                i++;
            }
            rval.data.keycode.keynum = i;
            if (special.is_number()){
                rval.data.keycode.mask = special.int_value() & 0xff;
            }
        }else if (consumer.is_number()){
            rval.type = BLEHID_CONSUMERCODE;
            rval.data.consumercode = consumer.int_value() & 0xff;
        }

        if (duration.is_number()){
            rval.duration = duration.int_value() & 0xff;
        }
    }

    return rval;
}

json11::Json::object bleHidDataToJson(const BleHidCodeData &data){
    if (data.type == BLEHID_KEYCODE){
        std::vector<int32_t> keys;
        for (int i = 0; i < data.data.keycode.keynum; i++){
            keys.insert(keys.end(), data.data.keycode.keys[i]);
        }
        return json11::Json::object({
            {JSON_KEYCODES, keys},
            {JSON_SPECHIAL_MASK, (int32_t)data.data.keycode.mask},
            {JSON_DURATION, (int32_t)data.duration},
        });
    }else{
        return json11::Json::object({
            {JSON_CONSUMER, (int32_t)data.data.consumercode},
            {JSON_DURATION, (int32_t)data.duration},
        });
    }
}
