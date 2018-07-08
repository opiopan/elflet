#include <esp_log.h>
#include <esp_system.h>
#include "version.h"
#include "version.string"

#include "boardconfig.h"
#include "sdkconfig.h"

//----------------------------------------------------------------------
// class Version implementation
//----------------------------------------------------------------------
Version::Version(const char* str){
    enum STATUS {MAJOR = 0, MINOR, BUILD} stat = MAJOR;
    major = 0;
    minor = 0;
    build = 0;
    uncommited = false;
    
    for (const char* p = str; *p; p++){
	if (*p >= '0' && *p <= '9'){
	    int* dest = stat == MAJOR ? &major :
		        stat == MINOR ? &minor :
		                        &build;
	    *dest *= 10;
	    *dest += *p - '0';
	}else if (*p == '.'){
	    if (stat == BUILD){
		break;
	    }
	    stat = (STATUS)((int)stat + 1);
	}else if (*p == '+'){
	    uncommited = true;
	    break;
	}
    }
}

int Version::compare(const Version& other) const{
    if (major != other.major){
	return major - other.major;
    }
    if (minor != other.minor){
	return minor - other.minor;
    }
    if (build != other.build){
	return build - other.build;
    }
    if (uncommited != other.uncommited){
	return uncommited ? 1 : -1;
    }
    
    return 0;
}

//----------------------------------------------------------------------
// export functions
//----------------------------------------------------------------------
static Version* currentVersion;

const char* getVersionString(){
    return verstr;
}

const Version* getVersion(){
    if (!currentVersion){
	currentVersion = new Version(verstr);
    }
    return currentVersion;
}
