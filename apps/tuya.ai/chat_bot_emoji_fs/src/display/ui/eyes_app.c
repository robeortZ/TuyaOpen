#include "img_utility.h"
#include "ty_gui_fs.h"
#include "dl_list.h"
#include "tkl_mutex.h"
#include "lvgl.h"
#include "tal_log.h"

#define LCD_WIDTH           320
#define LCD_HEIGHT          480

STATIC TKL_MUTEX_HANDLE ai_msg_mutex = NULL;
static lv_img_dsc_t ui_img_png = {0};

static lv_obj_t *png_img;
static int current_gif_index = 0;
LV_IMG_DECLARE(TuyaOpen_img);
#if 0
#define LCD_WIDTH           128
#define LCD_HEIGHT          128

LV_IMG_DECLARE(Nature128);
LV_IMG_DECLARE(Touch128);
LV_IMG_DECLARE(Angry128);
LV_IMG_DECLARE(Fearful128);
LV_IMG_DECLARE(Surprise128);
LV_IMG_DECLARE(Sad128);
LV_IMG_DECLARE(Think128);
LV_IMG_DECLARE(Happy128);
LV_IMG_DECLARE(Confused128);
LV_IMG_DECLARE(Disappointed128);

static lv_obj_t *gif_img;

static int current_gif_index = 0;
static const char *gif_files[] = {
    &Nature128,
    &Surprise128,
    &Angry128,
    &Fearful128,
    &Touch128,
    &Sad128,
    &Think128,
    &Happy128,
    &Confused128,
    &Disappointed128,
};
#endif

char * gif_name[]={"angry.gif","happy.gif","neutral.gif","neutral.gif","neutral.gif"}; 
char * png_name[]={"ANGRY.png","CONFUSED.png","DISAPPOINTED.png","HAPPY.png","NEUTRAL.png","SURPRISE.png"}; 

uint8_t load_png_emoji(uint8_t idx)
{
    PR_DEBUG("---------------->png_img_unload");
    png_img_unload(&ui_img_png);
    PR_DEBUG("----------------->%s load_png_emoji!!!",png_name[idx]);
    return png_img_load(tuya_app_gui_get_picture_full_path(png_name[idx]), &ui_img_png);
}

uint8_t tuya_emoji_get(char  *emoji_string)
{
    uint8_t which = 0;

    char *emoji_set[] = {
        "NEUTRAL", "SURPRISE", "ANGRY", "FEARFUL", "HAPPY", "SAD",  "DISAPPOINTED", "CONFUSED", "ANNOYED", "THINKING"
    };

    uint8_t emoji_index[] = 
        {4,             5,          0,       5,         3,      4,      2,              1,          4,          4};

    int i = 0;
    for (i = 0; i < sizeof(emoji_set)/sizeof(emoji_set[0]); i++) {
        if (0 == strcasecmp(emoji_set[i], emoji_string)) {
            which = emoji_index[i];
            break;
        }
    }

    return which;
}

void tuya_eyes_init()
{
    PR_DEBUG("-------------->tuya_eyes_init!!!");
    // png_img = lv_img_create(lv_scr_act());
    // lv_obj_set_size(png_img, LV_HOR_RES, LV_VER_RES);
    // lv_obj_center(png_img);
    // lv_img_set_src(png_img, &TuyaOpen_img); 
    // PR_DEBUG("-------------->test tkl_open begin!!!");
    // tkl_fopen("/t5_fs/picture/NEUTRAL.png", "r");
    // PR_DEBUG("-------------->test tkl_open end!!!");
    // if(load_png_emoji(4)==0)
    //     lv_img_set_src(png_img, &ui_img_png); 
    // else 
    //     LV_LOG_WARN("%s png load err!!!",png_name[4]);
}


void tuya_eyes_app(char *msg)
{
    uint8_t emoji;

    PR_DEBUG("-------------->%s ",msg);
    // tkl_mutex_lock(ai_msg_mutex);
    // emoji = tuya_emoji_get(msg);
    // if (current_gif_index != emoji) {
    //     current_gif_index = emoji;
    //     if(load_png_emoji(emoji)==0)
    //         lv_img_set_src(png_img, &ui_img_png); 
    //     else 
    //         PR_DEBUG("%s png load err!!!",png_name[emoji]);
    // }
    // tkl_mutex_unlock(ai_msg_mutex);
}