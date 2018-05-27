#include <esp_log.h>
#include <string.h>
#include <string>
#include <stdlib.h>
#include <Task.h>
#include <mbedtls/error.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <mongoose.h>
#include "Mutex.h"
#include "bdlist.h"
#include "session.h"

#include "boardconfig.h"
#include "sdkconfig.h"

#define MAX_SESSION 4

const char tag[] = "session";

//--------------------------------------------------------------------
// Session Imprementation Class definition
//--------------------------------------------------------------------
class SessionImp : public Session{
public:
    typedef BDList<SessionImp> SessionList;
    
protected:
    friend SessionList;

    static int32_t seed;

    SessionImp* next;
    SessionImp* prev;
    
    bool    isActive;
    char user[SESSION_USER_NAME_MAX + 1];
    char id[SESSION_ID_LENGTH + 1];

public:
    static void initEnv(int32_t s);
    
    SessionImp();
    virtual ~SessionImp();

    const char* getId() override {return id;};
    const char* getUser() override {return user;};

    bool activate(const char* user);
    void expire() override;
};

static Mutex* listMutex;
static SessionImp::SessionList* activeList;
static SessionImp::SessionList* freeList;

void SessionImp::initEnv(int32_t s){
    seed = s;
}

//--------------------------------------------------------------------
// session create / destroy
//--------------------------------------------------------------------
SessionImp::SessionImp() :
    next(NULL), prev(NULL), isActive(false), user(""), id(""){
    freeList->addAtLast(this);    
}

SessionImp::~SessionImp(){
    if (isActive){
	activeList->remove(this);
    }else{
	freeList->remove(this);
    }
}

//--------------------------------------------------------------------
// session activate / deactivate
//--------------------------------------------------------------------
bool SessionImp::activate(const char* u){
    if (strlen(user) > SESSION_USER_NAME_MAX){
	ESP_LOGE(tag, "user name too long %s", u);
	return false;
    }

    /* calcurate session id */
    int32_t src[2] = {seed, rand()};
    char buf[sizeof(src) * 2];
    cs_md5_ctx* ctx = new cs_md5_ctx;
    cs_md5_init(ctx);
    cs_md5_update(ctx, (uint8_t*)buf, sizeof(buf));
    uint8_t hash[16];
    cs_md5_final(hash, ctx);
    delete ctx;
    for (int i = 0; i < sizeof(hash) * 2; i++){
	int data = (hash[i/2] >> (i & 1 ? 0 : 4)) & 0xf;
	static const uint8_t dic[] = "0123456789abcdef";
	id[i] = dic[data];
    }
    id[sizeof(hash) * 2] = 0;

    /* transition state */
    freeList->remove(this);
    isActive = false;
    activeList->addAtLast(this);

    return true;
}

void SessionImp::expire(){
    if (isActive){
	LockHolder holder = LockHolder(*listMutex);
	activeList->remove(this);
	isActive = false;
	freeList->addAtLast(this);
    }
}

//--------------------------------------------------------------------
// session pool operation
//--------------------------------------------------------------------
static SessionImp* sessionPool;

void initSessionPool(int32_t seed){
    if (sessionPool == NULL){
	activeList = new SessionImp::SessionList("sessionActiveList");
	freeList = new SessionImp::SessionList("sessionFreeList");
	listMutex = new Mutex();
	SessionImp::initEnv(seed);
	sessionPool = new SessionImp[MAX_SESSION];
    }
}

void deinitSessionPool(){
    if (sessionPool){
	delete[] sessionPool;
	sessionPool = NULL;
	delete activeList;
	activeList = NULL;
	delete freeList;
	freeList = NULL;
	delete listMutex;
	listMutex = NULL;
    }
}

Session* createSession(const uint8_t* user){
    return NULL;
}

Session* findSession(const uint8_t* id){
    return NULL;
}
