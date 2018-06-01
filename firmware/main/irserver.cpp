#include <esp_log.h>
#include <string.h>
#include <string>
#include <Task.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include "Mutex.h"
#include "irrc.h"
#include "irserver.h"
#include "irserverProtocol.h"
#include "boardconfig.h"

#include "sdkconfig.h"

static const char tag[] = "irserver";
static const int MULTIPLICITY = 4;
static const int COMM_TIMEOUT = 5;

static IRRC irrc;

//--------------------------------------------------------------------
// Class definitions
//--------------------------------------------------------------------
class CommunicationTask : public Task {
    friend class CommunicationTaskList;
    
private:
    enum {FREE, RUNNING} mode;
    CommunicationTask*   next;
    CommunicationTask*   prev;
    int                  sockfd;
    uint8_t              ibuf[IRS_REQMAXSIZE];
    int32_t              ibufUsed;
    uint8_t              obuf[IRS_RESMAXSIZE];
    int32_t              obufUsed;
    int32_t              obufHead;

public:
    CommunicationTask();
    void setSocket(int fd){
	sockfd = fd;
    };

private:
    void run(void *data) override;
    bool flashAndRecieveCmd(IRSHeader* hdr, void** data, int32_t* dataSize);
    bool makeResponse(const IRSHeader* req, IRSCmd respCode,
		      void* respData, int32_t respDataSize);
    bool cmdTxFormat(const IRSHeader* hdr, const void* data, int32_t dataSize);
};

//--------------------------------------------------------------------
class CommunicationTaskList {
private:
    CommunicationTask* first;
    CommunicationTask* last;

public:
    CommunicationTaskList();
    CommunicationTask* getFirst();
    void add(CommunicationTask* task);
    void remove(CommunicationTask* task);
};

//--------------------------------------------------------------------
class IRServerTask : public Task {
private:
    int sockfd;

    Mutex mutex;
    CommunicationTask commTasks[MULTIPLICITY];
    CommunicationTaskList freeCommTaskList;
    CommunicationTaskList runningCommTaskList;
    
public:
    IRServerTask();
    bool init(in_port_t port);
    void endCommTask(CommunicationTask* task);
    
private:
    void run(void *data) override;
};

static IRServerTask* server = NULL;

//--------------------------------------------------------------------
// CommunicationTask imprementation
//--------------------------------------------------------------------
CommunicationTask::CommunicationTask() :
    mode(FREE), next(NULL), prev(NULL), sockfd(-1),
    ibufUsed(0), obufUsed(0), obufHead(0)
{
}

void CommunicationTask::run(void *data)
{
    ESP_LOGI(tag, "begin communication task");

    mode = RUNNING;

    IRSHeader req;
    void* reqData;
    int32_t reqDataSize;

    while(flashAndRecieveCmd(&req, &reqData, &reqDataSize)){
	bool rc = false;
	if (req.cmd == IRServerCmdTxFormat){
	    rc = cmdTxFormat(&req, reqData, reqDataSize);
	}

	if (!rc){
	    break;
	}
    }
    
    close(sockfd);
    sockfd = -1;
    mode = FREE;
    server->endCommTask(this);

    ESP_LOGI(tag, "end communication task");
}

bool CommunicationTask::flashAndRecieveCmd(IRSHeader* hdr,
					   void** data, int32_t* dataSize)
{
    if (ibufUsed == IRS_REQMAXSIZE){
	ESP_LOGE(tag, "no remaining input buffer, it may be bug");
	return false;
    }

    bool cmdRecieved = false;
    fd_set read_fds;
    fd_set write_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    while (!cmdRecieved || obufUsed - obufHead > 0){
	if (ibufUsed < IRS_REQMAXSIZE){
	    FD_SET(sockfd, &read_fds);
	}else{
	    FD_CLR(sockfd, &read_fds);
	}
	if (obufUsed - obufHead > 0){
	    FD_SET(sockfd, &write_fds);
	}else{
	    FD_CLR(sockfd, &write_fds);
	}

	struct timeval timeout;
	timeout.tv_sec = COMM_TIMEOUT;
	timeout.tv_usec = 0;

	int rc = select(sockfd + 1, &read_fds, &write_fds, NULL, &timeout);
	if (rc == 0){
	    ESP_LOGI(tag, "client connection will be closed due to timeout");
	    return false;
	}else if (rc < 0){
	    ESP_LOGE(tag, "select failed");
	    return false;
	}

	if (FD_ISSET(sockfd, &write_fds)){
	    rc = write(sockfd, obuf + obufHead, obufUsed - obufHead);
	    if (rc < 0){
		ESP_LOGE(tag, "sending to client failed");
		return false;
	    }
	    obufHead += rc;
	}

	if (FD_ISSET(sockfd, &read_fds)){
	    rc = read(sockfd, ibuf + ibufUsed, sizeof(ibuf) - ibufUsed);
	    if (rc < 0){
		ESP_LOGE(tag, "reciing from client failed");
		return false;
	    }else if (rc == 0){
		ESP_LOGI(tag, "connection closed from client");
		return false;
	    }
	    ibufUsed += rc;
	    if (ibufUsed >= sizeof(IRSHeader)){
		IRSHeader* rawhdr = (IRSHeader*)ibuf;
		int16_t cmdsize = ntohs(rawhdr->size);
		if (rawhdr->id.intID != IRSIDINT ||
		    cmdsize > sizeof(ibuf)){
		    ESP_LOGI(tag, "protocol error");
		    return false;
		}
		if (ibufUsed >= cmdsize){
		    cmdRecieved = true;
		    hdr->id.intID = IRSIDINT;
		    hdr->cmd = ntohs(rawhdr->cmd);
		    hdr->size = cmdsize;
		    *data = ibuf + sizeof(IRSHeader);
		    *dataSize = cmdsize - sizeof(IRSHeader);
		}
	    }
	}
    }

    obufHead = 0;
    obufUsed = 0;
    return true;
}

bool CommunicationTask::makeResponse(const IRSHeader* req, IRSCmd respCode,
				     void* respData, int32_t respDataSize)
{
    if (ibufUsed > req->size){
	memcpy(ibuf, ibuf + req->size, ibufUsed - req->size);
    }
    ibufUsed -= req->size;

    obufHead = 0;
    obufUsed = sizeof(IRSHeader) + respDataSize;
    if (obufUsed > sizeof(obuf)){
	ESP_LOGE(tag, "reciing data size is too large");
	return false;
    }
    
    IRSHeader* resp = (IRSHeader*)obuf;
    resp->id.intID = IRSIDINT;
    resp->cmd = htons(respCode);
    resp->size = htons(obufUsed);
    if (respDataSize > 0){
	memcpy(obuf + sizeof(IRSHeader), respData, respDataSize);
    }

    return true;
}
    
bool CommunicationTask::cmdTxFormat(const IRSHeader* hdr,
				    const void* data, int32_t dataSize)
{
    {
	IRSTxFormatData* dhdr = (IRSTxFormatData*)data;
	IRSFormat format = (IRSFormat)ntohs(dhdr->format);
	int dataLen = ntohs(dhdr->dataLen);

	IRRC_PROTOCOL protocol;
	if (format == IRSFORMAT_NEC){
	    protocol = IRRC_NEC;
	}else if (format == IRSFORMAT_AEHA){
	    protocol = IRRC_AEHA;
	}else if (format == IRSFORMAT_SONY){
	    protocol = IRRC_SONY;
	}else{
	    ESP_LOGE(tag, "not supported format");
	    goto END;
	}

	IRRCChangeProtocol(&irrc, protocol);
	unsigned char* cbuf = (unsigned char*)(dhdr + 1);
	IRRCSend(&irrc, cbuf, dataLen);
    }

END:
    return makeResponse(hdr, IRServerCmdAck, NULL, 0); 
}

//--------------------------------------------------------------------
// CommunicationTaskList imprementation
//--------------------------------------------------------------------
CommunicationTaskList::CommunicationTaskList(): first(NULL), last(NULL)
{
}

CommunicationTask* CommunicationTaskList::getFirst()
{
    return first;
}

void CommunicationTaskList::add(CommunicationTask* task)
{
    if (task->next != NULL || task->prev != NULL){
	ESP_LOGE(tag, "task list corrupted");
	abort();
    }
    if (last){
	last->next = task;
	task->prev = last;
	last = task;
    }else{
	if (first){
	    ESP_LOGE(tag, "task list corrupted");
	    abort();
	}
	first = task;
	last = task;
    }
}
    
void CommunicationTaskList::remove(CommunicationTask* task)
{
    if (task->prev){
	task->prev->next = task->next;
    }else{
	first = task->next;
    }
    if (task->next){
	task->next->prev = task->prev;
    }else{
	last = task->prev;
    }
    task->next = NULL;
    task->prev = NULL;
}

//--------------------------------------------------------------------
// IRServerTask imprementation
//--------------------------------------------------------------------
IRServerTask::IRServerTask() :
    sockfd(-1)
{
    for (int i = 0; i < MULTIPLICITY; i++){
	freeCommTaskList.add(&commTasks[i]);
    }
}
    
bool IRServerTask::init(in_port_t port)
{
    {
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
	    ESP_LOGE(tag, "failed to careate socket");
	    goto ERR1;
	}
	
	struct sockaddr_in sock_addr;
	memset(&sock_addr, 0, sizeof(sock_addr));
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_addr.s_addr = 0;
	sock_addr.sin_port = htons(port);
	int rc = bind(sockfd,
		      (struct sockaddr*)&sock_addr,
		      sizeof(sock_addr));
	if (rc != 0) {
	    ESP_LOGE(tag, "bind failed");
	    goto ERR2;
	}
	
	rc = listen(sockfd, MULTIPLICITY);
	if (rc != 0) {
	    ESP_LOGE(tag, "listen failed");
	    goto ERR2;
	}
	
	return true;
    }
    
ERR2:
    close(sockfd);
    sockfd = -1;
ERR1:
    return false;
}

void IRServerTask::endCommTask(CommunicationTask* task)
{
    LockHolder holder = LockHolder(mutex);
    runningCommTaskList.remove(task);
    freeCommTaskList.add(task);
}

void IRServerTask::run(void *data)
{
    
    while (1){
	struct sockaddr_in sock_addr;
	socklen_t addr_len;
	int newfd = accept(sockfd, (struct sockaddr*)&sock_addr, &addr_len);
	if (newfd >= 0){
	    ESP_LOGI(tag, "detected client request to connect");
	    CommunicationTask* task = NULL;
	    {
		LockHolder holder = LockHolder(mutex);
		task = freeCommTaskList.getFirst();
		if (task){
		    freeCommTaskList.remove(task);
		    runningCommTaskList.add(task);
		}
	    }
	    if (task){
		task->setStackSize(8000);
		task->setSocket(newfd);
		task->start();
	    }else{
		ESP_LOGE(tag, "server is busy: there is no free task slot");
	    }
	}else{	    
	    ESP_LOGE(tag, "failed to accept  socket");
	}
    }
}

//--------------------------------------------------------------------
// launch server
//--------------------------------------------------------------------
void startIRServer()
{
    if (server == NULL){
	IRRCInit(&irrc, IRRC_TX, IRRC_NEC, GPIO_IRLED);
	
	server = new IRServerTask();
	server->init(IRSERVER_PORT);
	server->setStackSize(8000);
	server->start();
	ESP_LOGI(tag, "IR server has been started. port: %d", IRSERVER_PORT);
    }
}
