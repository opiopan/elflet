#pragma once

class Version {
public:
    int major;
    int minor;
    int build;
    bool uncommited;

public:
    Version(const char* str);

    int compare(const Version& other) const;
    bool operator == (const Version& right) const {
        return compare(right) == 0;
    };
    bool operator < (const Version& right) const {
        return compare(right) < 0;
    };
    bool operator <= (const Version& right) const {
        return compare(right) <= 0;
    };
    bool operator > (const Version& right) const {
        return compare(right) > 0;
    };
    bool operator >= (const Version& right) const {
        return compare(right) >= 0;
    };
};

const char* getVersionString();
const Version* getVersion();
