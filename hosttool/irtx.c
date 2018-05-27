#include <stdio.h>
#include <string.h>
#include "irslib.h"

static void printusage()
{
    fprintf(stderr, "usage: irtx HOST nec|aeha|sony BYTE-STREAM\n");
}

static int ctox(int c){
    if (c >= '0' && c <= '9'){
	return c - '0';
    }else if (c >= 'a' && c <= 'f'){
	return c - 'a' + 0xa;
    }else if (c >= 'A' && c <= 'F'){
	return c - 'A' + 0xa;
    }else{
	return -1;
    }
}

int main(int argc, char** argv)
{
    if (argc < 4){
	printusage();
	return 1;
    }

    IRSFormat format = -1;
    if (strcmp(argv[2], "nec") == 0){
	format = IRSFORMAT_NEC;
    }else if (strcmp(argv[2], "aeha") == 0){
	format = IRSFORMAT_AEHA;
    }else if (strcmp(argv[2], "sony") == 0){
	format = IRSFORMAT_SONY;
    }else{
	printusage();
	return 1;
    }

    unsigned char buf[32];
    char* stream = argv[3];
    int streamLen = strlen(argv[3]);
    if ((streamLen & 1) || streamLen / 2 > sizeof(buf)){
	printusage();
	return 1;
    }
    for (int i = 0; stream[i*2] != 0; i++){
	int h = ctox(stream[i*2]);
	int l = ctox(stream[i*2 + 1]);
	if (h < 0 || l < 0){
	    printusage();
	    return 1;
	}
	buf[i] = (h << 4 | l);
    }

    IRSHANDLE irs = irsOpen(argv[1]);
    if (irs == NULL){
	perror("irsOpen");
	return 1;
    }

    irsInvokeTxFormat(irs, format, buf, streamLen / 2);

    return 0;
}
