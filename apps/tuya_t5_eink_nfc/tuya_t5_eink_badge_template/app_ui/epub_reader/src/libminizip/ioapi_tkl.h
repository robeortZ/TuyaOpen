/* ioapi_tkl.h -- IO base function header for minizip using TuyaOS tkl_fs
 * 
 * This file provides file I/O operations for minizip library using
 * TuyaOS's tkl_fs interface instead of standard C file functions.
 * 
 * Copyright (C) 2024 Tuya Inc.
 */

#ifndef _IOAPI_TKL_H
#define _IOAPI_TKL_H

#include "ioapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Fill the filefunc structure with TuyaOS tkl_fs functions
 * 
 * This function initializes a zlib_filefunc_def structure with TuyaOS
 * file system operations (tkl_fopen, tkl_fread, etc.) for use with minizip.
 * 
 * @param pzlib_filefunc_def Pointer to the structure to be filled
 * 
 * @note After calling this function, the structure can be passed to
 *       unzOpen2() or zipOpen2() to use TuyaOS file system.
 * 
 * Example usage:
 *   zlib_filefunc_def filefunc;
 *   fill_tkl_filefunc(&filefunc);
 *   unzFile uf = unzOpen2("file.zip", &filefunc);
 */
void fill_tkl_filefunc OF((zlib_filefunc_def * pzlib_filefunc_def));

/**
 * @brief Standard minizip function for default file operations
 * 
 * This is the function that unzip.c and zip.c call when no custom
 * file functions are provided. We implement it to use TuyaOS tkl_fs.
 * 
 * @param pzlib_filefunc_def Pointer to the structure to be filled
 */
void fill_fopen_filefunc OF((zlib_filefunc_def * pzlib_filefunc_def));

#ifdef __cplusplus
}
#endif

#endif /* _IOAPI_TKL_H */

