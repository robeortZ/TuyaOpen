#include "png_loader.h"
#include "tkl_system.h"
#include "tkl_file.h"
#include "tkl_memory.h"
#include "tkl_psram.h"
#include "bk_printf.h"
#include "tuya_cloud_types.h"
#include "lvgl.h"

#ifdef PNG_IMG_DECODE_IN_MEMORY
#include "raw_img_loader.h"
#endif

#ifdef PNG_DECODE_WITH_STB_IMAGE
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"
#elif defined(PNG_DECODE_WITH_LIBSPNG)
#include <spng.h>
#include <inttypes.h>
#endif

#ifdef IMG_DECODING_TIME_TEST
extern unsigned long long int __current_timestamp(void);
#endif

#if defined(PNG_DECODE_WITH_STB_IMAGE)
static void stabi_convert_color_depth(uint8_t * img, uint32_t px_cnt)
{
#if LV_COLOR_DEPTH == 32
    lv_color32_t * img_argb = (lv_color32_t *)img;
    lv_color_t c;
    lv_color_t * img_c = (lv_color_t *) img;
    uint32_t i;
    for(i = 0; i < px_cnt; i++) {
        c = lv_color_make(img_argb[i].ch.red, img_argb[i].ch.green, img_argb[i].ch.blue);
        img_c[i].ch.red = c.ch.blue;
        img_c[i].ch.blue = c.ch.red;
    }
#elif LV_COLOR_DEPTH == 16
    lv_color32_t * img_argb = (lv_color32_t *)img;
    lv_color_t c;
    uint32_t i;
    for(i = 0; i < px_cnt; i++) {
        c = lv_color_make(img_argb[i].ch.blue, img_argb[i].ch.green, img_argb[i].ch.red);
        img[i * 3 + 2] = img_argb[i].ch.alpha;
        img[i * 3 + 1] = c.full >> 8;
        img[i * 3 + 0] = c.full & 0xFF;
    }
#elif LV_COLOR_DEPTH == 8
    lv_color32_t * img_argb = (lv_color32_t *)img;
    lv_color_t c;
    uint32_t i;
    for(i = 0; i < px_cnt; i++) {
        c = lv_color_make(img_argb[i].ch.red, img_argb[i].ch.green, img_argb[i].ch.blue);
        img[i * 2 + 1] = img_argb[i].ch.alpha;
        img[i * 2 + 0] = c.full;
    }
#elif LV_COLOR_DEPTH == 1
    lv_color32_t * img_argb = (lv_color32_t *)img;
    uint8_t b;
    uint32_t i;
    for(i = 0; i < px_cnt; i++) {
        b = img_argb[i].ch.red | img_argb[i].ch.green | img_argb[i].ch.blue;
        img[i * 2 + 1] = img_argb[i].ch.alpha;
        img[i * 2 + 0] = b > 128 ? 1 : 0;
    }
#endif
}

static int _stabi_io_custom_read(void *user, char *data, int size)
{
    int read_len = 0;
    TUYA_FILE_HANDLE fd = (TUYA_FILE_HANDLE)user;

    read_len = tkl_file_read(fd, (uint8_t*)data, size, (uint32_t*)&read_len);
    if(read_len < 0)
        read_len = 0;
    return read_len;
}

static void _stabi_io_custom_skip(void *user, int n)
{
    TUYA_FILE_HANDLE fd = (TUYA_FILE_HANDLE)user;
    tkl_file_seek(fd, n, TUYA_FILE_SEEK_CUR);
}

static int _stabi_io_custom_eof(void *user)
{
    TUYA_FILE_HANDLE fd = (TUYA_FILE_HANDLE)user;
    uint32_t pos = 0, size = 0;
    tkl_file_tell(fd, &pos);
    tkl_file_size(fd, &size);
    return (pos >= size) ? 1 : 0;
}
#elif defined(PNG_DECODE_WITH_LIBSPNG)
#if LVGL_VERSION_MAJOR < 9
static void spng_convert_color_depth(uint8_t * img, uint32_t px_cnt)
{
#if LV_COLOR_DEPTH == 32
    lv_color32_t * img_argb = (lv_color32_t *)img;
    lv_color_t c;
    lv_color_t * img_c = (lv_color_t *) img;
    uint32_t i;
    for(i = 0; i < px_cnt; i++) {
        c = lv_color_make(img_argb[i].ch.red, img_argb[i].ch.green, img_argb[i].ch.blue);
        img_c[i].ch.red = c.ch.blue;
        img_c[i].ch.blue = c.ch.red;
    }
#elif LV_COLOR_DEPTH == 16
    lv_color32_t * img_argb = (lv_color32_t *)img;
    lv_color_t c;
    uint32_t i;
    for(i = 0; i < px_cnt; i++) {
        c = lv_color_make(img_argb[i].ch.blue, img_argb[i].ch.green, img_argb[i].ch.red);
        img[i * 3 + 2] = img_argb[i].ch.alpha;
        img[i * 3 + 1] = c.full >> 8;
        img[i * 3 + 0] = c.full & 0xFF;
    }
#elif LV_COLOR_DEPTH == 8
    lv_color32_t * img_argb = (lv_color32_t *)img;
    lv_color_t c;
    uint32_t i;
    for(i = 0; i < px_cnt; i++) {
        c = lv_color_make(img_argb[i].ch.red, img_argb[i].ch.green, img_argb[i].ch.blue);
        img[i * 2 + 1] = img_argb[i].ch.alpha;
        img[i * 2 + 0] = c.full;
    }
#elif LV_COLOR_DEPTH == 1
    lv_color32_t * img_argb = (lv_color32_t *)img;
    uint8_t b;
    uint32_t i;
    for(i = 0; i < px_cnt; i++) {
        b = img_argb[i].ch.red | img_argb[i].ch.green | img_argb[i].ch.blue;
        img[i * 2 + 1] = img_argb[i].ch.alpha;
        img[i * 2 + 0] = b > 128 ? 1 : 0;
    }
#endif
}
#else
static void spng_convert_color_depth(uint8_t * img_p, uint32_t px_cnt)
{
#if 1
    lv_color32_t * img_argb = (lv_color32_t *)img_p;
    uint32_t i;
    for(i = 0; i < px_cnt; i++) {
        uint8_t blue = img_argb[i].blue;
        img_argb[i].blue = img_argb[i].red;
        img_argb[i].red = blue;
    }
#else
    lv_color32_t * img_argb = (lv_color32_t *)img_p;
    uint32_t i;

#if LV_COLOR_DEPTH == 32 || LV_COLOR_DEPTH == 24
    for(i = 0; i < px_cnt; i++) {
        uint8_t blue = img_argb[i].blue;
        img_argb[i].blue = img_argb[i].red;
        img_argb[i].red = blue;
    }
#elif LV_COLOR_DEPTH == 16
    uint16_t rgb565_value = 0;
    for (int i = 0; i < px_cnt; i++) {
        uint8_t r = (img_argb[i].red >> 3) & 0x1F;
        uint8_t g = (img_argb[i].green >> 2) & 0x3F;
        uint8_t b = (img_argb[i].blue >> 3) & 0x1F;
        rgb565_value = (b << 11) | (g << 5) | r;
        img_p[i * 3 + 2] = img_argb[i].alpha;
        img_p[i * 3 + 1] = rgb565_value >> 8;
        img_p[i * 3 + 0] = rgb565_value & 0xFF;
    }
#elif LV_COLOR_DEPTH == 8
    uint8_t rgb565_value = 0;
    for (int i = 0; i < px_cnt; i++) {
        uint8_t r = (img_argb[i].red >> 5) & 0x07;
        uint8_t g = (img_argb[i].green >> 5) & 0x07;
        uint8_t b = (img_argb[i].blue >> 6) & 0x03;
        rgb565_value = (r << 5) | (g << 2) | b;
        img_p[i * 2 + 1] = img_argb[i].alpha;
        img_p[i * 2 + 0] = rgb565_value;
    }
#endif
#endif
}
#endif
#endif

static uint32_t _read_filelen(const char *filename)
{
    uint32_t ret = 0;
    TUYA_FILE_HANDLE file_handle = NULL;
    OPERATE_RET result = OPRT_OK;

    do {
        if(!filename) {
            bk_printf("[%s][%d]param is null.\r\n", __FUNCTION__, __LINE__);
            ret = 0;
            break;
        }
        
        result = tkl_file_open(filename, "rb", &file_handle);
        if(OPRT_OK != result) {
            bk_printf("[%s][%d] open fail:%s\r\n", __FUNCTION__, __LINE__, filename);
            break;
        }

        result = tkl_file_size(file_handle, &ret);
        if(OPRT_OK != result) {
            bk_printf("[%s][%d] get size fail:%s\r\n", __FUNCTION__, __LINE__, filename);
            ret = 0;
            break;
        }

        bk_printf("[%s][%d] %s size:%d\r\n", __FUNCTION__, __LINE__, filename, ret);
    } while(0);

    if (file_handle != NULL) {
        tkl_file_close(file_handle);
    }
    
    return ret;
}

static frame_buffer_t *_read_file(const char *file_name)
{
    frame_buffer_t *png_frame = NULL;
    uint32_t file_len = 0;
    OPERATE_RET ret = OPRT_OK;
    TUYA_FILE_HANDLE file_handle = NULL;

    do {
        ret = tkl_file_open(file_name, "rb", &file_handle);
        if(OPRT_OK != ret) {
            bk_printf("[%s][%d] %s open fail ?\r\n", __FUNCTION__, __LINE__, file_name);
            break;
        }
        
        bk_printf("tkl_file_open success");
        
        ret = tkl_file_size(file_handle, &file_len);
        if(OPRT_OK != ret || file_len <= 0) {
            bk_printf("[%s][%d] %s don't exist or size is 0\r\n", __FUNCTION__, __LINE__, file_name);
            break;
        }

        png_frame = tkl_system_malloc(sizeof(frame_buffer_t));
        if(!png_frame) {
            bk_printf("[%s][%d] malloc fail\r\n", __FUNCTION__, __LINE__);
            break;
        }

        memset(png_frame, 0, sizeof(frame_buffer_t));
        png_frame->frame = tkl_system_psram_malloc(file_len);
        png_frame->length = file_len;
        if(!png_frame->frame) {
            tkl_system_free(png_frame);
            png_frame = NULL;
            bk_printf("[%s][%d] psram malloc fail\r\n", __FUNCTION__, __LINE__);
            break;
        }

        uint32_t read_len = 0;
        ret = tkl_file_read(file_handle, (uint8_t*)png_frame->frame, file_len, &read_len);
        if(OPRT_OK != ret || read_len != file_len) {
            bk_printf("[%s][%d] read file fail\r\n", __FUNCTION__, __LINE__);
            tkl_system_psram_free(png_frame->frame);
            png_frame->frame = NULL;
            tkl_system_free(png_frame);
            png_frame = NULL;
        }
    } while(0);

    if (file_handle != NULL) {
        tkl_file_close(file_handle);
    }
    
    return png_frame;
}

/**
 * 图像文件加载: PNG格式
 * @brief load png file from storage, and store data in img_dst
 * @param[in] filename: only filename with path
 * @param[in] img_dst: if the data of img_dst is NULL, will use memory of psram malloc, so when you don't use, should free this ram
 * @retval  OPRT_OK:success
 * @retval  <0: decode fail or file don't exist in storage
*/
OPERATE_RET png_img_load(const char *filename, lv_img_dsc_t *img_dst)
{
    OPERATE_RET ret = OPRT_COM_ERROR;

#ifdef PNG_IMG_DECODE_IN_MEMORY
    unsigned long long int last_run_ms = 0, curr_run_ms = 0;

    #ifdef IMG_DECODING_TIME_TEST
    last_run_ms = __current_timestamp();
    #endif
    
    frame_buffer_t *png_frame = _read_file(filename);
    
    #ifdef IMG_DECODING_TIME_TEST
    curr_run_ms = __current_timestamp();
    #endif
    
    bk_printf("[%s][%d] file '%s' get use '%llu'ms\r\n", __FUNCTION__, __LINE__, filename,
        curr_run_ms-last_run_ms);

    if (png_frame == NULL) {
        bk_printf("[%s][%d]read file '%s' fail ?\r\n", __FUNCTION__, __LINE__, filename);
    } else {
        #ifdef IMG_DECODING_TIME_TEST
        last_run_ms = __current_timestamp();
        #endif
        
        ret = raw_img_load(image_type_png, (uint8_t*)png_frame->frame, png_frame->length, img_dst, false);
        
        #ifdef IMG_DECODING_TIME_TEST
        curr_run_ms = __current_timestamp();
        #endif
        
        tkl_system_psram_free(png_frame->frame);
        png_frame->frame = NULL;
        tkl_system_free(png_frame);
        png_frame = NULL;
        
        bk_printf("[%s][%d] file '%s' decode use '%llu'ms\r\n", __FUNCTION__, __LINE__, filename,
            curr_run_ms-last_run_ms);
    }
#else
    
    if(!filename || !img_dst) {
        ret = OPRT_COM_ERROR;
        bk_printf("[%s][%d]param invalid\r\n", __FUNCTION__, __LINE__);
        return ret;
    }
    
    #if defined(PNG_DECODE_WITH_STB_IMAGE)
        int channels = 0, png_width = 0, png_height = 0;
        uint8_t * data = NULL;
        TUYA_FILE_HANDLE file_handle = NULL;
        stbi_io_callbacks callbacks = {
            .read = _stabi_io_custom_read,
            .skip = _stabi_io_custom_skip,
            .eof = _stabi_io_custom_eof
        };
        
        ret = tkl_file_open(filename, "rb", &file_handle);
        if(OPRT_OK != ret) {
            bk_printf("[%s][%d] stb image decode open png file '%s' fail ???\r\n", __FUNCTION__, __LINE__, filename);
            return ret;
        }
        
        #if LVGL_VERSION_MAJOR < 9
        img_dst->header.always_zero = 0;
        #else
        img_dst->header.magic = LV_IMAGE_HEADER_MAGIC;
        #endif
        
        img_dst->header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
        data = (uint8_t *)stbi_load_from_callbacks(&callbacks, (void *)file_handle, (int *)&png_width, (int *)&png_height, &channels, 0);
        tkl_file_close(file_handle);
        
        if (data != NULL) {
            stabi_convert_color_depth(data, png_width * png_height);
            img_dst->data = data;
            img_dst->data_size = _read_filelen(filename);
            img_dst->header.w = png_width;
            img_dst->header.h = png_height;
            ret = OPRT_OK;
        } else {
            bk_printf("[%s][%d] stb image decode png fail [reason '%s']???\r\n", __FUNCTION__, __LINE__, 
                (stbi_failure_reason()!=NULL)?stbi_failure_reason():"unknown");
            return ret;
        }
        
    #elif defined(PNG_DECODE_WITH_LIBSPNG)
        struct spng_ihdr ihdr;
        size_t image_size = 0;
        int fmt = SPNG_FMT_PNG;
        spng_ctx *ctx = spng_ctx_new(0);
        
        if (!ctx) {
            bk_printf("[%s][%d] Failed to create SPNG context ?\r\n", __FUNCTION__, __LINE__);
            return ret;
        }
        
        // Read file into memory first
        frame_buffer_t *png_frame = _read_file(filename);
        if (!png_frame) {
            spng_ctx_free(ctx);
            return ret;
        }
        
        ret = spng_set_png_buffer(ctx, (uint8_t*)png_frame->frame, png_frame->length);
        if (ret != 0) {
            bk_printf("[%s][%d] spng_set_png_buffer failed: %s ?\r\n", __FUNCTION__, __LINE__, spng_strerror(ret));
            spng_ctx_free(ctx);
            tkl_system_psram_free(png_frame->frame);
            tkl_system_free(png_frame);
            return ret;
        }
        
        ret = spng_get_ihdr(ctx, &ihdr);
        if (ret != 0) {
            bk_printf("[%s][%d] spng_get_ihdr failed: %s ?\r\n", __FUNCTION__, __LINE__, spng_strerror(ret));
            spng_ctx_free(ctx);
            tkl_system_psram_free(png_frame->frame);
            tkl_system_free(png_frame);
            return ret;
        }
        
        bk_printf("[%s][%d] png Image width: %u, height: %u, bit depth: %u, Color type: %u\r\n",  
            __FUNCTION__, __LINE__, ihdr.width, ihdr.height, ihdr.bit_depth, ihdr.color_type);
        
        fmt = SPNG_FMT_RGBA8;
        
        ret = spng_decoded_image_size(ctx, fmt, &image_size);
        if (ret != 0) {
            bk_printf("[%s][%d] spng_decoded_image_size failed: %s ?\r\n", __FUNCTION__, __LINE__, spng_strerror(ret));
            spng_ctx_free(ctx);
            tkl_system_psram_free(png_frame->frame);
            tkl_system_free(png_frame);
            return ret;
        }
        
        img_dst->data = tkl_system_psram_malloc(image_size);
        if (img_dst->data == NULL) {
            bk_printf("[%s][%d] Failed to allocate memory for image ?\r\n", __FUNCTION__, __LINE__);
            spng_ctx_free(ctx);
            tkl_system_psram_free(png_frame->frame);
            tkl_system_free(png_frame);
            ret = OPRT_MALLOC_FAILED;
            return ret;
        }
        
        ret = spng_decode_image(ctx, (void *)img_dst->data, image_size, fmt, 0);
        if (ret != 0) {
            bk_printf("[%s][%d] spng_decode_image failed: %s ?\r\n", __FUNCTION__, __LINE__, spng_strerror(ret));
            tkl_system_psram_free((void *)img_dst->data);
            img_dst->data = NULL;
            spng_ctx_free(ctx);
            tkl_system_psram_free(png_frame->frame);
            tkl_system_free(png_frame);
            return ret;
        }
        
        spng_ctx_free(ctx);
        tkl_system_psram_free(png_frame->frame);
        tkl_system_free(png_frame);
        
        spng_convert_color_depth((uint8_t *)img_dst->data, ihdr.width * ihdr.height);
        bk_printf("[%s][%d] spng_decode_image successful [real size %lu]!\r\n", __FUNCTION__, __LINE__, image_size);
        
        #if LVGL_VERSION_MAJOR < 9
        img_dst->header.always_zero = 0;
        img_dst->header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
        #else
        img_dst->header.magic = LV_IMAGE_HEADER_MAGIC;
        img_dst->header.cf = LV_COLOR_FORMAT_ARGB8888;
        #endif
        
        img_dst->data_size = image_size;
        img_dst->header.w = ihdr.width;
        img_dst->header.h = ihdr.height;
        
    #else
        lv_img_decoder_dsc_t img_decoder_dsc;
        memset((char *)&img_decoder_dsc, 0, sizeof(img_decoder_dsc));
        img_decoder_dsc.src_type = LV_IMG_SRC_FILE;
        ret = lv_img_decoder_open(&img_decoder_dsc, filename, img_decoder_dsc.color, img_decoder_dsc.frame_id);
        if(ret != LV_RES_OK) {
            bk_printf("[%s][%d] decode fail:%d\r\n", __FUNCTION__, __LINE__, ret);
            ret = OPRT_COM_ERROR;
            return ret;
        }

        memcpy(&img_dst->header, &img_decoder_dsc.header, sizeof(lv_img_header_t));
        img_dst->data_size = _read_filelen(filename);
        img_dst->data = img_decoder_dsc.img_data;
        lv_mem_free((void *)img_decoder_dsc.src);
        ret = OPRT_OK;
    #endif
#endif

    return ret;
}

/**
 * 图像文件卸载: PNG格式
*/
void png_img_unload(lv_img_dsc_t *img_dst)
{
    if (img_dst->data != NULL) {
        #if defined(PNG_DECODE_WITH_STB_IMAGE)
        stbi_image_free((void *)img_dst->data);
        #elif defined(PNG_DECODE_WITH_LIBSPNG)
        bk_printf("-------------->png_img_unload now!");
        tkl_system_psram_free((void *)img_dst->data);
        #else
        lv_mem_free((void *)img_dst->data);
        #endif
    }
    img_dst->data = NULL;
} 