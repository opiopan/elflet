#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "irslib.h"

static void printusage()
{
    fprintf(stderr,
	    "usage: irtx [-b bit-length] HOST "
	    "nec|aeha|sony HEX-BYTE-STREAM\n");
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
    int bits = -1;
    int opt;
    while ((opt = getopt(argc, argv, "b:")) != -1){
	switch (opt){
	case 'b':
	    bits = atoi(optarg);
	    if (bits < 1){
		fprintf(stderr, "bit length whilch specified with "
			"option '-b' must be grater than 0\n");
		return 1;
	    }
	    break;
	default:
	    fprintf(stderr, "synax error\n");
	    printusage();
	    return 1;
	}
    }
    
    if (argc - optind < 3){
	printusage();
	return 1;
    }

    IRSFormat format = -1;
    if (strcmp(argv[1 + optind], "nec") == 0){
	format = IRSFORMAT_NEC;
    }else if (strcmp(argv[1 + optind], "aeha") == 0){
	format = IRSFORMAT_AEHA;
    }else if (strcmp(argv[1 + optind], "sony") == 0){
	format = IRSFORMAT_SONY;
    }else{
	printusage();
	return 1;
    }

    unsigned char buf[32];
    char* stream = argv[2 + optind];
    int streamLen = strlen(argv[2 + optind]);
    if ((streamLen & 1) || streamLen / 2 > sizeof(buf)){
	fprintf(stderr, "byte stream is too long\n");
	return 1;
    }
    bits = bits < 1 ? streamLen * 4 : bits;
    for (int i = 0; stream[i*2] != 0; i++){
	int h = ctox(stream[i*2]);
	int l = ctox(stream[i*2 + 1]);
	if (h < 0 || l < 0){
	    printusage();
	    return 1;
	}
	buf[i] = (h << 4 | l);
    }

    IRSHANDLE irs = irsOpen(argv[optind]);
    if (irs == NULL){
	perror("irsOpen");
	return 1;
    }

    irsInvokeTxFormat(irs, format, buf, bits);

    return 0;
}
