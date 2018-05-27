#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>

#include "irslib.h"

typedef struct {
    int  sockfd;
    char obuf[IRS_REQMAXSIZE];
    char ibuf[IRS_RESMAXSIZE];
    bool resp;
    int  ibufUsed;
}IRSCTX;


IRSHANDLE irsOpen(const char* host)
{
    struct hostent* he = gethostbyname(host);
    if (*he->h_addr_list == NULL){
	errno = ENOENT;
	goto ERR1;
    }

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0){
	goto ERR1;
    }
    
    struct sockaddr_in addr;
    addr.sin_family = PF_INET;
    addr.sin_addr.s_addr = *((in_addr_t *) *he->h_addr_list);
    addr.sin_port = htons(IRSERVER_PORT);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0){
	goto ERR2;
    }

    IRSCTX* ctx = (IRSCTX*)malloc(sizeof(IRSCTX));
    if (ctx == NULL){
	goto ERR2;
    }
    ctx->sockfd = sockfd;
    ctx->ibufUsed = 0;
    ctx->resp = false;

    return ctx;
    
ERR2:
    close(sockfd);
    
ERR1:
    return NULL;
}

void irsClose(IRSHANDLE handle)
{
    IRSCTX* ctx = (IRSCTX*)handle;
    close(ctx->sockfd);
    free(ctx);
}

int irsSendRequest(IRSHANDLE handle, IRSCmd cmd, void* data, int dataSize)
{
    IRSCTX* ctx = (IRSCTX*)handle;

    IRSHeader* hdr = (IRSHeader*)ctx->obuf;
    if (sizeof(*hdr) + dataSize > sizeof(ctx->obuf)){
	errno = E2BIG;
	return -1;
    }
    hdr->id.intID = IRSIDINT;
    hdr->cmd = htons(cmd);
    hdr->size = htons(sizeof(*hdr) + dataSize);
    if (dataSize > 0){
	memcpy(hdr + 1, data, dataSize);
    }
    
    int toSend = sizeof(*hdr) + dataSize;
    int sent = 0;
    while (sent < toSend){
	int rc = write(ctx->sockfd, ctx->obuf + sent, toSend - sent);
	if (rc < 0){
	    return -1;
	}
	sent += rc;
    }

    ctx->resp = false;
    ctx->ibufUsed = 0;
    while (!ctx->resp){
	int rc = read(ctx->sockfd, ctx->ibuf + ctx->ibufUsed,
		      sizeof(ctx->ibuf) - ctx->ibufUsed);
	if (rc < 0){
	    return -1;
	}
	ctx->ibufUsed += rc;
	if (ctx->ibufUsed >= sizeof(IRSHeader)){
	    IRSHeader* hdr = (IRSHeader*)ctx->ibuf;
	    int size = ntohs(hdr->size);
	    if (hdr->id.intID != IRSIDINT || size > sizeof(ctx->ibuf)){
		errno = EPROTO;
		return -1;
	    }
	    if (ctx->ibufUsed == size){
		ctx->resp = true;
	    }
	}
    }
    
    return 0;
}

int irsGetRespons(IRSHANDLE handle, void** data, int* dataSize)
{
    IRSCTX* ctx = (IRSCTX*)handle;

    if (!ctx->resp){
	errno = EBUSY;
	return -1;
    }

    
    IRSHeader* hdr = (IRSHeader*)ctx->ibuf;
    int rcode = ntohs(hdr->cmd);
    *dataSize = ntohs(hdr->size - sizeof(*hdr));
    *data = hdr + 1;

    return 0;
}

int irsInvokeTxFormat(IRSHANDLE handle,
		      IRSFormat format, void* data, int dataSize)
{
    IRSCTX* ctx = (IRSCTX*)handle;

    char buf[sizeof(ctx->obuf) - sizeof(IRSHeader)];
    IRSTxFormatData* hdr = (IRSTxFormatData*)buf;
    if (dataSize + sizeof(*hdr) > sizeof(buf)){
	errno = E2BIG;
	return -1;
    }
    hdr->format = htons(format);
    hdr->dataLen = htons(dataSize);
    if (dataSize > 0){
	memcpy(hdr + 1, data, dataSize);
    }
    return irsSendRequest(handle, IRServerCmdTxFormat,
			  buf, dataSize + sizeof(*hdr));
}
