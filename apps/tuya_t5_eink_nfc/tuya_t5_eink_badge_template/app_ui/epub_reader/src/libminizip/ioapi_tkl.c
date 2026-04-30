/* ioapi_tkl.c -- IO base function implementation for minizip using TuyaOS tkl_fs
 * 
 * This file provides file I/O operations for minizip library using
 * TuyaOS's tkl_fs interface instead of standard C file functions.
 * 
 * Copyright (C) 2024 Tuya Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zlib.h"
#include "ioapi.h"
#include "tkl_fs.h"

/* TuyaOS file system interface */
voidpf ZCALLBACK tkl_fopen_file_func(opaque, filename, mode)
voidpf           opaque;
const char      *filename;
int              mode;
{
    (void)opaque;
    TUYA_FILE file = NULL;
    const char *mode_fopen = NULL;
    
    /* Convert mode flags to TuyaOS file mode strings */
    if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER) == ZLIB_FILEFUNC_MODE_READ) {
        mode_fopen = "rb";
    } else if (mode & ZLIB_FILEFUNC_MODE_EXISTING) {
        mode_fopen = "r+b";
    } else if (mode & ZLIB_FILEFUNC_MODE_CREATE) {
        mode_fopen = "wb";
    }

    if ((filename != NULL) && (mode_fopen != NULL)) {
        file = tkl_fopen(filename, mode_fopen);
    }
    
    return file;
}

uLong ZCALLBACK tkl_fread_file_func(opaque, stream, buf, size)
voidpf          opaque;
voidpf          stream;
void           *buf;
uLong           size;
{
    (void)opaque;
    uLong ret;
    
    ret = (uLong)tkl_fread(buf, (INT_T)size, (TUYA_FILE)stream);
    
    return ret;
}

uLong ZCALLBACK tkl_fwrite_file_func(opaque, stream, buf, size)
voidpf          opaque;
voidpf          stream;
const void     *buf;
uLong           size;
{
    (void)opaque;
    uLong ret;
    
    ret = (uLong)tkl_fwrite((VOID_T *)buf, (INT_T)size, (TUYA_FILE)stream);
    
    return ret;
}

long ZCALLBACK tkl_ftell_file_func(opaque, stream)
voidpf         opaque;
voidpf         stream;
{
    (void)opaque;
    long ret;
    
    INT64_T pos = tkl_ftell((TUYA_FILE)stream);
    ret = (long)pos;
    
    return ret;
}

long ZCALLBACK tkl_fseek_file_func(opaque, stream, offset, origin)
voidpf         opaque;
voidpf         stream;
uLong          offset;
int            origin;
{
    (void)opaque;
    int  fseek_origin = 0;
    long ret;
    
    /* Convert origin flags */
    switch (origin) {
    case ZLIB_FILEFUNC_SEEK_CUR:
        fseek_origin = SEEK_CUR;
        break;
    case ZLIB_FILEFUNC_SEEK_END:
        fseek_origin = SEEK_END;
        break;
    case ZLIB_FILEFUNC_SEEK_SET:
        fseek_origin = SEEK_SET;
        break;
    default:
        return -1;
    }
    
    ret = tkl_fseek((TUYA_FILE)stream, (INT64_T)offset, fseek_origin);
    
    return ret;
}

int ZCALLBACK tkl_fclose_file_func(opaque, stream)
voidpf        opaque;
voidpf        stream;
{
    (void)opaque;
    int ret;
    
    ret = tkl_fclose((TUYA_FILE)stream);
    
    return ret;
}

int ZCALLBACK tkl_ferror_file_func(opaque, stream)
voidpf        opaque;
voidpf        stream;
{
    (void)opaque;
    int ret;
    
    /* tkl_feof returns non-zero if at end of file or error */
    ret = tkl_feof((TUYA_FILE)stream);
    
    return ret;
}

/* Fill the filefunc structure with TuyaOS functions */
void fill_tkl_filefunc(pzlib_filefunc_def) zlib_filefunc_def *pzlib_filefunc_def;
{
    pzlib_filefunc_def->zopen_file  = tkl_fopen_file_func;
    pzlib_filefunc_def->zread_file  = tkl_fread_file_func;
    pzlib_filefunc_def->zwrite_file = tkl_fwrite_file_func;
    pzlib_filefunc_def->ztell_file  = tkl_ftell_file_func;
    pzlib_filefunc_def->zseek_file  = tkl_fseek_file_func;
    pzlib_filefunc_def->zclose_file = tkl_fclose_file_func;
    pzlib_filefunc_def->zerror_file = tkl_ferror_file_func;
    pzlib_filefunc_def->opaque      = NULL;
}

/* 
 * fill_fopen_filefunc - Standard minizip function that unzip.c and zip.c call
 * 
 * This function is called by unzip.c and zip.c when pzlib_filefunc_def is NULL.
 * We redirect it to use TuyaOS tkl_fs functions instead of standard C fopen/fread/etc.
 */
void fill_fopen_filefunc(pzlib_filefunc_def) zlib_filefunc_def *pzlib_filefunc_def;
{
    /* Use TuyaOS file system functions */
    fill_tkl_filefunc(pzlib_filefunc_def);
}

