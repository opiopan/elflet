#include <esp_log.h>
#include <esp_ota_ops.h>
#include <mdns.h>
#include <string.h>
#include <stdio.h>
#include <iostream>
#include "NameResolver.h"

static const char tag[] = "NameResolver";


RNAME_RESULT resolveHostname(std::string& hostname){
    enum Status {INDOUBT, COLON, HEAD, HOSTNAME, TAIL};
    Status status = INDOUBT;
    int headlen = 0;
    int name = 0;
    int namelen = 0;
    int tail = 0;
    int taillen =0;

    // parse element of hostname (uri)
    for (int i = 0; i < hostname.length(); i++){
	int val = hostname.data()[i];
	switch (status){
	case INDOUBT:
	    headlen++;
	    if (val == ':'){
		status = COLON;
	    }
	    break;
	case COLON:
	    if (val == '/'){
		status = HEAD;
		headlen++;
	    }else if (val >= '0' && val <= '9'){
		status = TAIL;
		namelen = headlen - 1;
		headlen = 0;
		tail = namelen;
		taillen = 2;
	    }else{
		//syntax error
		return RNAME_NO_NEED_TO_RESOLVE;
	    }
	    break;
	case HEAD:
	    if (val == '/'){
		headlen++;
	    }else if (val == ':'){
		// syntax error
		return RNAME_NO_NEED_TO_RESOLVE;
	    }else{
		status = HOSTNAME;
		name = headlen;
		namelen = 1;
	    }
	    break;
	case HOSTNAME:
	    if (val == ':'){
		status = TAIL;
		tail = i;
		taillen = 1;;
	    }else{
		namelen++;
	    }
	    break;
	case TAIL:
	    taillen++;
	    break;
	}
    }

    if (status == INDOUBT){
	status = HOSTNAME;
	namelen = headlen;
	headlen = 0;
    }
    if (status != HOSTNAME && status != TAIL){
	return RNAME_NO_NEED_TO_RESOLVE;
    }

    // Is hostname mDNS name?
    static const char domain[6] = {'.', 'l', 'o', 'c', 'a', 'l'};
    if (namelen <= sizeof(domain) ||
	memcmp(hostname.data() + name + namelen - sizeof(domain),
	       domain, sizeof(domain)) != 0){
	// no need to resolve
	return RNAME_NO_NEED_TO_RESOLVE;
    }

    // resolve mDNS name
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    std::string in(hostname.data() + name, namelen - sizeof(domain));
    struct ip4_addr addr;
    addr.addr = 0;
    esp_err_t err = -1;
    for (int i = 0; i < 6 && err != ESP_OK; i++){
	err = mdns_query_a(in.c_str(), 1000,  &addr);
    }
    if (err != ESP_OK){
	//fail to resolve
	return RNAME_FAIL_TO_RESOLVE;
    }

    // make result
    char out[16];
    snprintf(out, sizeof(out), "%d.%d.%d.%d",
	     addr.addr & 0xff, (addr.addr >> 8) & 0xff,
	     (addr.addr >> 16) & 0xff, (addr.addr >> 24) & 0xff);
    std::string result(hostname.data(), headlen);
    result += out;
    if (taillen > 0){
	result += hostname.c_str() + tail;
    }
    hostname = result;
    
    return RNAME_SUCCEED;
}
