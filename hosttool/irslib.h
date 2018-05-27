#pragma once

#include <irserverProtocol.h>

typedef void* IRSHANDLE;

IRSHANDLE irsOpen(const char* host);
void irsClose(IRSHANDLE handle);
int irsSendRequest(IRSHANDLE handle, IRSCmd cmd, void* data, int dataSize);
int irsGetRespons(IRSHANDLE handle, void** data, int* dataSize);

int irsInvokeTxFormat(IRSHANDLE handle,
		      IRSFormat format, void* data, int dataSize);
