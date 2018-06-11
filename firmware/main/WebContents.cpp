#include <esp_log.h>
#include <esp_system.h>
#include <string.h>
#include <string>
#include <map>
#include <GeneralUtils.h>
#include "WebContents.h"
#include "Config.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "WebContents";

struct BinContent {
    const char* data;
    size_t length;

    BinContent(const char* d, size_t l) : data(d), length(l){};
    BinContent() : data(NULL), length(0){};
    BinContent(const BinContent& s) : data(s.data), length(s.length){};
};

typedef std::map<WebString, BinContent> ContentMap;
#define BINDEF(n) \
    extern const char n ## _start[] asm(" _binary_" #n  "_start");\
    extern const char n ## _end[] asm("_binary_" #n "_end");
#define EMBEDDED(n) {n##_start, (size_t)(n##_end - n##_start - 1)}

BINDEF(index_html);
BINDEF(jquery_3_3_1_min_js);
BINDEF(elflet_css);

static const ContentMap dict = {
    {"/", EMBEDDED(index_html)},
    {"/index.html", EMBEDDED(index_html)},
    {"/jquery-3.3.1.min.js", EMBEDDED(jquery_3_3_1_min_js)},
    {"/elflet.css", EMBEDDED(elflet_css)},
};

class WebContents : public ContentProvider {
protected:
public:
    WebContents();
    virtual ~WebContents();
    
    WebString getContent(const WebString& path) const override;
};

WebContents::WebContents(){
}

WebContents::~WebContents(){
}

WebString WebContents::getContent(const WebString& path) const {
    auto c = dict.find(path);
    if (c != dict.end()){
	return {c->second.data, c->second.length};
    }else{
	return {};
    }
}


ContentProvider* createContentProvider(){
    return new WebContents;
}
