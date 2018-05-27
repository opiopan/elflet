#include <esp_log.h>
#include "webserver.h"

#include "sdkconfig.h"

//static const char* tag = "ContentTypeDict";

static struct DictEnt{
    const char* extension;
    const char* typeString;
}dict[] = {
    {"html", "text/html"},
    {"css", "text/css"},
    {"xml", "text/xml"},
    {"js", "application/javascript"},
    {"json", "application/json"},
    {"jpg", "image/jpeg"},
    {"gif", "image/gif"},
    {"png", "image/png"},
    {NULL, "application/octet-stream"}
};

const char* findMimeType(const WebString& path){
    int pos;
    for (pos = path.length() - 1; pos >= 0; pos--){
	if (path.data()[pos] == '.'){
	    break;
	}
    }
    pos++;
    auto ext = WebString(path.data() + pos, path.length() - pos);
    
    const DictEnt* entry;
    for (entry = dict; entry->extension; entry++){
	if (ext == entry->extension){
	    break;
	}
    }
    
    return entry->typeString;
}
