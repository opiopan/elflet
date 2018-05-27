#include <esp_log.h>
#include <esp_vfs.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include "htdigestfs.h"

static const char* tag = "htdigestfs";

#define HTDIGESTFS_FDMAX 1

typedef struct {
    bool isOpened;
    off_t pos;
}FDCONTEXT;

static FDCONTEXT fdcontexts[HTDIGESTFS_FDMAX];

static struct {
    uint8_t* data;
    size_t length;
}digestData;

static int fs_open(void* ctx, const char * path, int flags, int mode)
{
    if (flags & O_WRONLY || flags & O_RDWR){
	errno = EACCES;
	return -1;
    }

    for (int fd = 0; fd < HTDIGESTFS_FDMAX; fd++){
	if (!fdcontexts[fd].isOpened){
	    fdcontexts[fd].isOpened = true;
	    fdcontexts[fd].pos = 0;
	    return fd;
	}
    }
    errno = EMFILE;
    return -1;
}

static int fs_close(void* ctx, int fd)
{
    FDCONTEXT* fdctx = fdcontexts + (fd & 0xffff);
    fdctx->isOpened = false;
    return 0;
}

static ssize_t fs_write(void* ctx, int fd, const void * data, size_t size)
{
    errno = EIO;
    return -1;
}

static ssize_t fs_read(void* ctx, int fd, void * dst, size_t size)
{
    FDCONTEXT* fdctx = fdcontexts + (fd & 0xffff);
    if (!fdctx->isOpened){
	errno = EBADF;
	return -1;
    }
    if (fdctx->pos >= digestData.length){
	return 0;
    }

    size_t rc = digestData.length - fdctx->pos;
    if (rc > size){
	rc = size;
    }
    memcpy(dst, digestData.data + fdctx->pos, rc);
    fdctx->pos += rc;

    return rc;
}

static off_t fs_lseek(void* ctx, int fd, off_t offset, int mode)
{
    FDCONTEXT* fdctx = fdcontexts + (fd & 0xffff);
    if (!fdctx->isOpened){
	errno = EBADF;
	return -1;
    }

    off_t newpos = fdctx->pos;
    if (mode == SEEK_SET){
	newpos = 0;
    }else if (mode == SEEK_END){
	newpos = digestData.length;
    }
    newpos += offset;

    if (offset < 0 || offset > digestData.length){
	errno = EINVAL;
	return -1;
    }
    if (offset < 0 || offset > digestData.length){
	errno = EOVERFLOW;
	return -1;
    }

    fdctx->pos = newpos;
    return newpos;
}

static int fs_fstat(void* ctx, int fd, struct stat * st)
{
    FDCONTEXT* fdctx = fdcontexts + (fd & 0xffff);
    if (!fdctx->isOpened){
	errno = EBADF;
	return -1;
    }

    *st = (struct stat){
	.st_mode = S_IFREG | 0666,
	.st_size = digestData.length,
	.st_blocks = 1,
	.st_blksize = digestData.length
    };

    return 0;
}

static FILE* represent_fp = NULL;

void htdigestfs_init(const char* mp)
{
    esp_vfs_t fs = {
        .flags = ESP_VFS_FLAG_CONTEXT_PTR,
	.write_p = &fs_write,
	.open_p = &fs_open,
	.fstat_p = &fs_fstat,
	.close_p = &fs_close,
	.read_p = &fs_read,
	.lseek_p = &fs_lseek
    };
    ESP_ERROR_CHECK(esp_vfs_register(mp, &fs, NULL));

    char path[strlen(mp) + 2];
    strncpy(path, mp, sizeof(path));
    strncat(path, "/a", sizeof(path));
    ESP_LOGI(tag, "path: %s", path);
    represent_fp = fopen(path, "r");
}

void htdigestfs_register(const char* user, const char* domain, void* digest)
{
    size_t ulen = strlen(user);
    size_t dlen = strlen(domain);
    size_t size = ulen + dlen + HTDIGESTLENGTH * 2 + 2;
    if (digestData.data){
	free(digestData.data);
    }
    digestData.data = malloc(size);
    digestData.length = size;
    sprintf((char*)digestData.data, "%s:%s:", user, domain);
    uint8_t* buf = digestData.data + ulen + dlen + 2;
    uint8_t* bytes = (uint8_t*)digest;
    for (int i = 0; i < HTDIGESTLENGTH * 2; i++){
	int data = (bytes[i/2] >> (i & 1 ? 0 : 4)) & 0xf;
	static const uint8_t dic[] = "0123456789abcdef";
	buf[i] = dic[data];
    }
    
}

FILE* htdigestfs_fp()
{
    return represent_fp;
}
