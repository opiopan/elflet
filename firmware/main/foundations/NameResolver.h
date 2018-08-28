#pragma once

#include <string>

enum RNAME_RESULT {
    RNAME_SUCCEED, RNAME_NO_NEED_TO_RESOLVE, RNAME_FAIL_TO_RESOLVE
};

RNAME_RESULT resolveHostname(std::string& name);
