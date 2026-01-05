/**
* @file ty_gui_fs.h
* @brief Common process - adapter the file operation api provide by OS
* @version 0.1
* @date 2020-11-09
*
* @copyright Copyright 2021-2030 Tuya Inc. All Rights Reserved.
*
*/

#ifndef __TY_GUI_FS_H__
#define __TY_GUI_FS_H__


#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GUI_DEV_INNER_FLASH,
    GUI_DEV_EXT_FLASH,
    GUI_DEV_SDCARD,
    GUI_DEV_UNKNOWN
} GUI_FS_DEV_TYPE_T;

/**
 * Seek modes.
 */
typedef enum {
    GUI_FS_SEEK_SET = 0x00,      /**< Set the position from absolutely (from the start of file)*/
    GUI_FS_SEEK_CUR = 0x01,      /**< Set the position from the current position*/
    GUI_FS_SEEK_END = 0x02,      /**< Set the position from the end of the file*/
} ty_gui_fs_whence_t;

/********************************************************************************
 *********************************tuya_os_fs_intf********************************
 ********************************************************************************/
/**
* @brief Make directory
*
* @param[in] path: path of directory
*
* @note This API is used for making a directory
*
* @return 0 on success. Others on failed
*/
int ty_gui_fs_mkdir(const char* path);

/**
* @brief Remove directory
*
* @param[in] path: path of directory
*
* @note This API is used for removing a directory
*
* @return 0 on success. Others on failed
*/
int ty_gui_fs_remove(const char* path);


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
int ty_gui_fs_mode(const char* path, unsigned int* mode);

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
int ty_gui_fs_is_exist(const char* path, BOOL_T* is_exist);

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
int ty_gui_fs_rename(const char* path_old, const char* path_new);

/**
* @brief Open directory
*
* @param[in] path: path of directory
* @param[out] dir: directory handle
*
* @note This API is used for opening a directory.
*
* @return 0 on success. Others on failed
*/
int ty_gui_dir_open(const char* path, TUYA_DIR* dir);

/**
* @brief Close directory
*
* @param[in] dir: directory handle
*
* @note This API is used for closing a directory.
*
* @return 0 on success. Others on failed
*/
int ty_gui_dir_close(TUYA_DIR dir);

/**
* @brief Read directory
*
* @param[in] dir: directory handle
* @param[out] info: file information
*
* @note This API is used for reading directory entries.
*
* @return 0 on success. Others on failed
*/
int ty_gui_dir_read(TUYA_DIR dir, TUYA_FILEINFO* info);

/**
* @brief Get directory name
*
* @param[in] info: file information
* @param[out] name: file name
*
* @note This API is used for getting file name from file information.
*
* @return 0 on success. Others on failed
*/
int ty_gui_dir_name(TUYA_FILEINFO info, const char** name);

/**
* @brief Check if directory
*
* @param[in] info: file information
* @param[out] is_dir: is directory or not
*
* @note This API is used for checking if file is directory.
*
* @return 0 on success. Others on failed
*/
int ty_gui_dir_is_directory(TUYA_FILEINFO info, BOOL_T* is_dir);

/**
* @brief Check if regular file
*
* @param[in] info: file information
* @param[out] is_regular: is regular file or not
*
* @note This API is used for checking if file is regular file.
*
* @return 0 on success. Others on failed
*/
int ty_gui_dir_is_regular(TUYA_FILEINFO info, BOOL_T* is_regular);

/**
* @brief Open file
*
* @param[in] path: file path
* @param[in] mode: open mode
*
* @note This API is used for opening a file.
*
* @return file handle on success. NULL on failed
*/
TUYA_FILE ty_gui_fopen(const char* path, const char* mode);

/**
* @brief Close file
*
* @param[in] file: file handle
*
* @note This API is used for closing a file.
*
* @return 0 on success. Others on failed
*/
int ty_gui_fclose(TUYA_FILE file);

/**
* @brief Read file
*
* @param[out] buf: buffer to store data
* @param[in] bytes: bytes to read
* @param[in] file: file handle
*
* @note This API is used for reading data from file.
*
* @return bytes read on success. -1 on failed
*/
int ty_gui_fread(void* buf, int bytes, TUYA_FILE file);

/**
* @brief Write file
*
* @param[in] buf: buffer containing data
* @param[in] bytes: bytes to write
* @param[in] file: file handle
*
* @note This API is used for writing data to file.
*
* @return bytes written on success. -1 on failed
*/
int ty_gui_fwrite(const void* buf, int bytes, TUYA_FILE file);

/**
* @brief Sync file
*
* @param[in] fd: file descriptor
*
* @note This API is used for syncing file data to storage.
*
* @return 0 on success. Others on failed
*/
int ty_gui_fsync(int fd);

/**
* @brief Get string from file
*
* @param[out] buf: buffer to store string
* @param[in] len: buffer length
* @param[in] file: file handle
*
* @note This API is used for reading a line from file.
*
* @return buffer pointer on success. NULL on failed
*/
char* ty_gui_fgets(char* buf, int len, TUYA_FILE file);

/**
* @brief Check end of file
*
* @param[in] file: file handle
*
* @note This API is used for checking if end of file.
*
* @return 0 on success. Others on failed
*/
int ty_gui_feof(TUYA_FILE file);

/**
* @brief Seek file
*
* @param[in] file: file handle
* @param[in] offs: offset
* @param[in] whence: seek mode
*
* @note This API is used for seeking file position.
*
* @return 0 on success. Others on failed
*/
int ty_gui_fseek(TUYA_FILE file, int64_t offs, int whence);

/**
* @brief Tell file position
*
* @param[in] file: file handle
*
* @note This API is used for getting file position.
*
* @return file position on success. -1 on failed
*/
int64_t ty_gui_ftell(TUYA_FILE file);

/**
* @brief Get file size
*
* @param[in] filepath: file path
*
* @note This API is used for getting file size.
*
* @return file size on success. -1 on failed
*/
int ty_gui_fgetsize(const char *filepath);

/**
* @brief Check file access
*
* @param[in] filepath: file path
* @param[in] mode: access mode
*
* @note This API is used for checking file access.
*
* @return 0 on success. Others on failed
*/
int ty_gui_faccess(const char *filepath, int mode);

/**
* @brief Get character from file
*
* @param[in] file: file handle
*
* @note This API is used for reading a character from file.
*
* @return character on success. EOF on failed
*/
int ty_gui_fgetc(TUYA_FILE file);

/**
* @brief Flush file
*
* @param[in] file: file handle
*
* @note This API is used for flushing file buffer.
*
* @return 0 on success. Others on failed
*/
int ty_gui_fflush(TUYA_FILE file);

/**
* @brief Get file descriptor
*
* @param[in] file: file handle
*
* @note This API is used for getting file descriptor.
*
* @return file descriptor on success. -1 on failed
*/
int ty_gui_fileno(TUYA_FILE file);

/**
* @brief Truncate file
*
* @param[in] fd: file descriptor
* @param[in] length: new length
*
* @note This API is used for truncating file.
*
* @return 0 on success. Others on failed
*/
int ty_gui_ftruncate(int fd, uint64_t length);

/**
* @brief Mount file system
*
* @param[in] path: mount path
* @param[in] dev_type: device type
*
* @note This API is used for mounting file system.
*
* @return 0 on success. Others on failed
*/
int ty_gui_fs_mount(const char *path, GUI_FS_DEV_TYPE_T dev_type);

/**
* @brief Unmount file system
*
* @param[in] path: mount path
*
* @note This API is used for unmounting file system.
*
* @return 0 on success. Others on failed
*/
int ty_gui_fs_unmount(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* __TY_GUI_FS_H__ */



