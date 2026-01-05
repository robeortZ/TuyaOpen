/**
 * @file ty_gui_fs.c
 * @brief the default weak implements of tuya os file system, this implement only used when OS=linux
 * @version 0.1
 * @date 2019-08-15
 *
 * @copyright Copyright 2020-2021 Tuya Inc. All Rights Reserved.
 *
 */
#include <errno.h>
// #include "tuya_app_gui_core_config.h"
#include "tuya_cloud_types.h"
#include "ty_gui_fs.h"
#include "tal_log.h"

#ifdef TUYA_APP_USE_TAL_IPC         //使用tal层文件系统接口
#include "tuya_uf_db.h"
#else
#include "tkl_fs.h"
#endif

/**
 * @brief Make directory
 *
 * @param[in] path: path of directory
 *
 * @note This API is used for making a directory
 *
 * @return 0 on success. Others on failed
 */
INT_T ty_gui_fs_mkdir(CONST CHAR_T *path)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return ufmkdir(path, 0);
#else
    return tkl_fs_mkdir(path);
#endif
}

/**
 * @brief Remove directory
 *
 * @param[in] path: path of directory
 *
 * @note This API is used for removing a directory
 *
 * @return 0 on success. Others on failed
 */
INT_T ty_gui_fs_remove(CONST CHAR_T *path)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return ufrmdir(path, 0);
#else
    return tkl_fs_remove(path);
#endif
}

/**
 * @brief Get file mode
 *
 * @param[in] path: path of directory
 * @param[out] mode: bit attibute of directory
 *
 * @note This API is used for getting file mode.
 *
 * @return 0 on success. Others on failed
 */
INT_T ty_gui_fs_mode(CONST CHAR_T *path, UINT_T *mode)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return -1;
#else
    return tkl_fs_mode(path, mode);
#endif
}

/**
 * @brief Check whether the file or directory exists
 *
 * @param[in] path: path of directory
 * @param[out] is_exist: the file or directory exists or not
 *
 * @note This API is used to check whether the file or directory exists.
 *
 * @return 0 on success. Others on failed
 */
INT_T ty_gui_fs_is_exist(CONST CHAR_T *path, BOOL_T *is_exist)
{
#ifdef TUYA_APP_USE_TAL_IPC
    *is_exist = ufexist(path);
    return 0;
#else
    return tkl_fs_is_exist(path, is_exist);
#endif
}

/**
 * @brief File rename
 *
 * @param[in] path_old: old path of directory
 * @param[in] path_new: new path of directory
 *
 * @note This API is used to rename the file.
 *
 * @return 0 on success. Others on failed
 */
INT_T ty_gui_fs_rename(CONST CHAR_T *path_old, CONST CHAR_T *path_new)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return -1;
#else
    return tkl_fs_rename(path_old, path_new);
#endif
}

/**
 * @brief Open directory
 *
 * @param[in] path: path of directory
 * @param[out] dir: handle of directory
 *
 * @note This API is used to open a directory
 *
 * @return 0 on success. Others on failed
 */
INT_T ty_gui_dir_open(CONST CHAR_T *path, TUYA_DIR *dir)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return -1;
#else
    return tkl_dir_open(path, dir);
#endif
}

/**
 * @brief Close directory
 *
 * @param[in] dir: handle of directory
 *
 * @note This API is used to close a directory
 *
 * @return 0 on success. Others on failed
 */
INT_T ty_gui_dir_close(TUYA_DIR dir)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return -1;
#else
    return tkl_dir_close(dir);
#endif
}

/**
 * @brief Read directory
 *
 * @param[in] dir: handle of directory
 * @param[out] info: file information
 *
 * @note This API is used to read a directory.
 * Read the file information of the current node, and the internal pointer points to the next node.
 *
 * @return 0 on success. Others on failed
 */
INT_T ty_gui_dir_read(TUYA_DIR dir, TUYA_FILEINFO *info)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return -1;
#else
    return tkl_dir_read(dir, info);
#endif
}

/**
 * @brief Get the name of the file node
 *
 * @param[in] info: file information
 * @param[out] name: file name
 *
 * @note This API is used to get the name of the file node.
 *
 * @return 0 on success. Others on failed
 */
INT_T ty_gui_dir_name(TUYA_FILEINFO info, CONST CHAR_T **name)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return -1;
#else
    return tkl_dir_name(info, name);
#endif
}

/**
 * @brief Check whether the node is a directory
 *
 * @param[in] info: file information
 * @param[out] is_dir: is directory or not
 *
 * @note This API is used to check whether the node is a directory.
 *
 * @return 0 on success. Others on failed
 */
INT_T ty_gui_dir_is_directory(TUYA_FILEINFO info, BOOL_T *is_dir)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return -1;
#else
    return tkl_dir_is_directory(info, is_dir);
#endif
}

/**
 * @brief Check whether the node is a normal file
 *
 * @param[in] info: file information
 * @param[out] is_regular: is normal file or not
 *
 * @note This API is used to check whether the node is a normal file.
 *
 * @return 0 on success. Others on failed
 */
INT_T ty_gui_dir_is_regular(TUYA_FILEINFO info, BOOL_T *is_regular)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return -1;
#else
    return tkl_dir_is_regular(info, is_regular);
#endif
}

/**
 * @brief Open file
 *
 * @param[in] path: path of file
 * @param[in] mode: file open mode: "r","w"...
 *
 * @note This API is used to open a file
 *
 * @return the file handle, NULL means failed
 */
TUYA_FILE ty_gui_fopen(CONST CHAR_T *path, CONST CHAR_T *mode)
{
    PR_DEBUG("---------------->ty_gui_fopen%s",path);
#ifdef TUYA_APP_USE_TAL_IPC
    return (TUYA_FILE)ufopen(path, mode);
#else
    return tkl_fopen(path, mode);
#endif
}

/**
 * @brief Close file
 *
 * @param[in] file: file handle
 *
 * @note This API is used to close a file
 *
 * @return 0 on success. EOF on failed
 */
INT_T ty_gui_fclose(TUYA_FILE file)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return ufclose((uFILE *)file);
#else
    return tkl_fclose(file);
#endif
}

/**
 * @brief Read file
 *
 * @param[in] buf: buffer for reading file
 * @param[in] bytes: buffer size
 * @param[in] file: file handle
 *
 * @note This API is used to read a file
 *
 * @return the bytes read from file
 */
INT_T ty_gui_fread(VOID_T *buf, INT_T bytes, TUYA_FILE file)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return ufread((uFILE *)file, buf, bytes);
#else
    return tkl_fread(buf, bytes, file);
#endif
}

/**
 * @brief write file
 *
 * @param[in] buf: buffer for writing file
 * @param[in] bytes: buffer size
 * @param[in] file: file handle
 *
 * @note This API is used to write a file
 *
 * @return the bytes write to file
 */
INT_T ty_gui_fwrite(VOID_T *buf, INT_T bytes, TUYA_FILE file)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return ufwrite((uFILE *)file, buf, bytes);
#else
    return tkl_fwrite(buf, bytes, file);
#endif
}

/**
 * @brief write buffer to flash
 *
 * @param[in] fd: file fd
 *
 * @note This API is used to write buffer to flash
 *
 * @return 0 on success. others on failed
 */
INT_T ty_gui_fsync(INT_T fd)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return ufsync(fd);
#else
    return tkl_fsync(fd);
#endif
}

/**
 * @brief Read string from file
 *
 * @param[in] buf: buffer for reading file
 * @param[in] len: buffer size
 * @param[in] file: file handle
 *
 * @note This API is used to read string from file
 *
 * @return the content get from file, NULL means failed
 */
CHAR_T *ty_gui_fgets(CHAR_T *buf, INT_T len, TUYA_FILE file)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return ufgets(buf, len, (uFILE *)file);
#else
    return tkl_fgets(buf, len, file);
#endif
}

/**
 * @brief Check wheather to reach the end fo the file
 *
 * @param[in] file: file handle
 *
 * @note This API is used to check wheather to reach the end fo the file
 *
 * @return 0 on not eof, others on eof
 */
INT_T ty_gui_feof(TUYA_FILE file)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return (ufeof((uFILE *)file)==TRUE)?1:0;
#else
    return tkl_feof(file);
#endif
}

/**
 * @brief Seek to the offset position of the file
 *
 * @param[in] file: file handle
 * @param[in] offs: offset
 * @param[in] whence: seek start point mode
 *
 * @note This API is used to seek to the offset position of the file.
 *
 * @return 0 on success, others on failed
 */
INT_T ty_gui_fseek(TUYA_FILE file, INT64_T offs, INT_T whence)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return ufseek((uFILE *)file, offs, whence);
#else
    return tkl_fseek(file, offs, whence);
#endif
}

/**
 * @brief Get current position of file
 *
 * @param[in] file: file handle
 *
 * @note This API is used to get current position of file.
 *
 * @return the current offset of the file
 */
INT64_T ty_gui_ftell(TUYA_FILE file)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return uftell((uFILE *)file);
#else
    return tkl_ftell(file);
#endif
}

/**
 * @brief Get file size
 *
 * @param[in] filepath file path + file name
 *
 * @note This API is used to get the size of file.
 *
 * @return the sizeof of file
 */
INT_T ty_gui_fgetsize(CONST CHAR_T *filepath)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return ufgetsize(filepath);
#else
    return tkl_fgetsize(filepath);
#endif
}

/**
 * @brief Judge if the file can be access
 *
 * @param[in] filepath file path + file name
 *
 * @param[in] mode access mode
 *
 * @note This API is used to access one file.
 *
 * @return 0 success,-1 failed
 */
INT_T ty_gui_faccess(CONST CHAR_T *filepath, INT_T mode)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return ufaccess(filepath, mode);
#else
    return tkl_faccess(filepath, mode);
#endif
}

/**
 * @brief read the next character from stream
 *
 * @param[in] file char stream
 *
 * @note This API is used to get one char from stream.
 *
 * @return as an unsigned char cast to a int ,or EOF on end of file or error
 */
INT_T ty_gui_fgetc(TUYA_FILE file)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return ufgetc((uFILE *)file);
#else
    return tkl_fgetc(file);
#endif
}

/**
 * @brief flush the IO read/write stream
 *
 * @param[in] file char stream
 *
 * @note This API is used to flush the IO read/write stream.
 *
 * @return 0 success,-1 failed
 */
INT_T ty_gui_fflush(TUYA_FILE file)
{
#ifdef TUYA_APP_USE_TAL_IPC
        return ufflush((uFILE *)file);
#else
        return tkl_fflush(file);
#endif
}

/**
 * @brief get the file fd
 *
 * @param[in] file char stream
 *
 * @note This API is used to get the file fd.
 *
 * @return the file fd
 */
INT_T ty_gui_fileno(TUYA_FILE file)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return ufileno((uFILE *)file);
#else
    return tkl_fileno(file);
#endif
}

/**
 * @brief truncate one file according to the length
 *
 * @param[in] fd file description
 *
 * @param[in] length the length want to truncate
 *
 * @note This API is used to truncate one file.
 *
 * @return 0 success,-1 failed
 */
INT_T ty_gui_ftruncate(INT_T fd, UINT64_T length)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return -1;
#else
    return tkl_ftruncate(fd, length);
#endif
}

/**
* @brief mount file system
*
* @param[in] mount point
*
* @param[in] device type fs based
*
* @note This API is used to mount file system.
*
* @return 0 success,-1 failed
*/
INT_T ty_gui_fs_mount(CONST CHAR_T *path, GUI_FS_DEV_TYPE_T dev_type)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return -1;
#else
    FS_DEV_TYPE_T type = DEV_EXT_FLASH;

    if (dev_type == GUI_DEV_INNER_FLASH)
        type = DEV_INNER_FLASH;
    else if (dev_type == GUI_DEV_EXT_FLASH)
        type = DEV_EXT_FLASH;
    else if (dev_type == GUI_DEV_SDCARD)
        type = DEV_SDCARD;
    return tkl_fs_mount(path, type);
#endif
}

/**
* @brief unmount file system
*
* @param[in] mount point
*
* @note This API is used to unmount file system.
*
* @return 0 success,-1 failed
*/
INT_T ty_gui_fs_unmount(CONST CHAR_T *path)
{
#ifdef TUYA_APP_USE_TAL_IPC
    return -1;
#else
    return tkl_fs_unmount(path);
#endif
}

