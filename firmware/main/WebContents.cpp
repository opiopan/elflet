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
    bool isCompressed;

    BinContent(const char* d, size_t l, bool c) :
	data(d), length(l), isCompressed(c){};
    BinContent() : data(NULL), length(0), isCompressed(false){};
    BinContent(const BinContent& s) :
	data(s.data), length(s.length), isCompressed(s.isCompressed){};
};

typedef std::map<WebString, BinContent> ContentMap;
#define BINDEF(n) \
    extern const char n ## _start[] asm(" _binary_" #n  "_start");\
    extern const char n ## _end[] asm("_binary_" #n "_end");
#define EMBEDDED(n, c) {n##_start, (size_t)(n##_end - n##_start - 1), c}

BINDEF(index_htmlz);
BINDEF(jquery_3_3_1_min_jsz);
BINDEF(elflet_cssz);
BINDEF(wizard_htmlz);
BINDEF(wizard_cssz);
BINDEF(wizard_jsz);

static const ContentMap normalDict = {
    {"/", EMBEDDED(index_htmlz, true)},
    {"/index.html", EMBEDDED(index_htmlz, true)},
    {"/jquery-3.3.1.min.js", EMBEDDED(jquery_3_3_1_min_jsz, true)},
    {"/elflet.css", EMBEDDED(elflet_cssz, true)},
    {"/wizard.html", EMBEDDED(wizard_htmlz, true)},
    {"/wizard.css", EMBEDDED(wizard_cssz, true)},
    {"/wizard.js", EMBEDDED(wizard_jsz, true)},
};

class WebContents : public ContentProvider {
protected:
    const ContentMap* dict;
public:
    WebContents();
    virtual ~WebContents();
    
    Content getContent(const WebString& path) const override;
};

WebContents::WebContents(){
    if (elfletConfig->getBootMode() == Config::FactoryReset ||
	elfletConfig->getBootMode() == Config::Configuration){
	dict = new ContentMap({
	    {"/jquery-3.3.1.min.js", EMBEDDED(jquery_3_3_1_min_jsz, true)},
	    {"/", EMBEDDED(wizard_htmlz, true)},
	    {"/wizard.html", EMBEDDED(wizard_htmlz, true)},
	    {"/wizard.css", EMBEDDED(wizard_cssz, true)},
	    {"/wizard.js", EMBEDDED(wizard_jsz, true)},
	});
    }else{
	dict = &normalDict;
    }
}

WebContents::~WebContents(){
}

ContentProvider::Content WebContents::getContent(const WebString& path) const {
    auto c = dict->find(path);
    if (c != dict->end()){
	return {{c->second.data, c->second.length}, c->second.isCompressed};
    }else{
	return {{}, false};
    }
}


ContentProvider* createContentProvider(){
    return new WebContents;
}
