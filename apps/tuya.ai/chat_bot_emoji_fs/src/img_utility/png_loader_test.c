#include "png_loader.h"
#include "tkl_system.h"
#include "bk_printf.h"
#include "lvgl.h"

// 测试用的图像描述符
static lv_img_dsc_t test_img_dsc = {0};

// 测试用的PNG文件路径
static const char* test_png_files[] = {
    "test_image.png",
    "icon.png", 
    "background.png",
    "emoji.png"
};

// 测试结果统计
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
} test_stats_t;

static test_stats_t test_stats = {0, 0, 0};

/**
 * 测试PNG文件加载功能
 */
static void test_png_load_functionality(void)
{
    bk_printf("=== 测试PNG加载功能 ===\n");
    
    for (int i = 0; i < sizeof(test_png_files) / sizeof(test_png_files[0]); i++) {
        const char* filename = test_png_files[i];
        bk_printf("测试文件: %s\n", filename);
        
        // 初始化图像描述符
        memset(&test_img_dsc, 0, sizeof(lv_img_dsc_t));
        
        // 尝试加载PNG文件
        OPERATE_RET ret = png_img_load(filename, &test_img_dsc);
        
        if (ret == OPRT_OK) {
            bk_printf("  ✓ 加载成功\n");
            bk_printf("    宽度: %d, 高度: %d\n", test_img_dsc.header.w, test_img_dsc.header.h);
            bk_printf("    数据大小: %d bytes\n", test_img_dsc.data_size);
            bk_printf("    颜色格式: %d\n", test_img_dsc.header.cf);
            
            // 验证图像数据
            if (test_img_dsc.data != NULL) {
                bk_printf("    数据指针: 有效\n");
                test_stats.passed_tests++;
            } else {
                bk_printf("    ✗ 数据指针无效\n");
                test_stats.failed_tests++;
            }
            
            // 卸载图像
            png_img_unload(&test_img_dsc);
            bk_printf("    图像已卸载\n");
            
        } else {
            bk_printf("  ✗ 加载失败, 错误码: %d\n", ret);
            test_stats.failed_tests++;
        }
        
        test_stats.total_tests++;
        bk_printf("\n");
    }
}

/**
 * 测试内存管理
 */
static void test_memory_management(void)
{
    bk_printf("=== 测试内存管理 ===\n");
    
    const char* test_file = "test_memory.png";
    lv_img_dsc_t img1 = {0}, img2 = {0};
    
    // 测试多次加载和卸载
    for (int i = 0; i < 3; i++) {
        bk_printf("第%d次加载测试:\n", i + 1);
        
        // 加载图像1
        OPERATE_RET ret1 = png_img_load(test_file, &img1);
        if (ret1 == OPRT_OK) {
            bk_printf("  图像1加载成功\n");
            
            // 加载图像2
            OPERATE_RET ret2 = png_img_load(test_file, &img2);
            if (ret2 == OPRT_OK) {
                bk_printf("  图像2加载成功\n");
                
                // 验证两个图像是否独立
                if (img1.data != img2.data) {
                    bk_printf("  ✓ 两个图像数据独立\n");
                    test_stats.passed_tests++;
                } else {
                    bk_printf("  ✗ 两个图像数据相同\n");
                    test_stats.failed_tests++;
                }
                
                // 卸载图像2
                png_img_unload(&img2);
                bk_printf("  图像2已卸载\n");
                
            } else {
                bk_printf("  图像2加载失败\n");
                test_stats.failed_tests++;
            }
            
            // 卸载图像1
            png_img_unload(&img1);
            bk_printf("  图像1已卸载\n");
            
        } else {
            bk_printf("  图像1加载失败\n");
            test_stats.failed_tests++;
        }
        
        test_stats.total_tests++;
        bk_printf("\n");
    }
}

/**
 * 测试错误处理
 */
static void test_error_handling(void)
{
    bk_printf("=== 测试错误处理 ===\n");
    
    lv_img_dsc_t test_img = {0};
    
    // 测试空文件名
    bk_printf("测试空文件名:\n");
    OPERATE_RET ret = png_img_load(NULL, &test_img);
    if (ret != OPRT_OK) {
        bk_printf("  ✓ 正确处理空文件名\n");
        test_stats.passed_tests++;
    } else {
        bk_printf("  ✗ 未正确处理空文件名\n");
        test_stats.failed_tests++;
    }
    test_stats.total_tests++;
    
    // 测试空图像描述符
    bk_printf("测试空图像描述符:\n");
    ret = png_img_load("test.png", NULL);
    if (ret != OPRT_OK) {
        bk_printf("  ✓ 正确处理空图像描述符\n");
        test_stats.passed_tests++;
    } else {
        bk_printf("  ✗ 未正确处理空图像描述符\n");
        test_stats.failed_tests++;
    }
    test_stats.total_tests++;
    
    // 测试不存在的文件
    bk_printf("测试不存在的文件:\n");
    ret = png_img_load("nonexistent_file.png", &test_img);
    if (ret != OPRT_OK) {
        bk_printf("  ✓ 正确处理不存在的文件\n");
        test_stats.passed_tests++;
    } else {
        bk_printf("  ✗ 未正确处理不存在的文件\n");
        test_stats.failed_tests++;
    }
    test_stats.total_tests++;
    
    bk_printf("\n");
}

/**
 * 测试性能
 */
static void test_performance(void)
{
    bk_printf("=== 测试性能 ===\n");
    
    const char* test_file = "performance_test.png";
    lv_img_dsc_t test_img = {0};
    
    // 测试加载时间
    uint32_t start_time = tkl_system_get_tick_count();
    
    OPERATE_RET ret = png_img_load(test_file, &test_img);
    
    uint32_t end_time = tkl_system_get_tick_count();
    uint32_t load_time = end_time - start_time;
    
    if (ret == OPRT_OK) {
        bk_printf("  加载时间: %d ms\n", load_time);
        bk_printf("  图像大小: %d x %d\n", test_img.header.w, test_img.header.h);
        bk_printf("  数据大小: %d bytes\n", test_img.data_size);
        
        // 计算加载速度
        if (load_time > 0) {
            float speed = (float)test_img.data_size / load_time;
            bk_printf("  加载速度: %.2f KB/s\n", speed);
        }
        
        test_stats.passed_tests++;
        
        // 卸载图像
        png_img_unload(&test_img);
        
    } else {
        bk_printf("  性能测试失败\n");
        test_stats.failed_tests++;
    }
    
    test_stats.total_tests++;
    bk_printf("\n");
}

/**
 * 打印测试统计
 */
static void print_test_summary(void)
{
    bk_printf("=== 测试总结 ===\n");
    bk_printf("总测试数: %d\n", test_stats.total_tests);
    bk_printf("通过测试: %d\n", test_stats.passed_tests);
    bk_printf("失败测试: %d\n", test_stats.failed_tests);
    
    if (test_stats.total_tests > 0) {
        float pass_rate = (float)test_stats.passed_tests / test_stats.total_tests * 100.0f;
        bk_printf("通过率: %.1f%%\n", pass_rate);
        
        if (pass_rate >= 80.0f) {
            bk_printf("测试结果: 优秀 ✓\n");
        } else if (pass_rate >= 60.0f) {
            bk_printf("测试结果: 良好 ✓\n");
        } else {
            bk_printf("测试结果: 需要改进 ✗\n");
        }
    }
}

/**
 * 主测试函数
 */
void png_loader_run_tests(void)
{
    bk_printf("开始PNG加载器测试...\n\n");
    
    // 重置测试统计
    memset(&test_stats, 0, sizeof(test_stats));
    
    // 运行各项测试
    test_png_load_functionality();
    test_memory_management();
    test_error_handling();
    test_performance();
    
    // 打印测试总结
    print_test_summary();
    
    bk_printf("PNG加载器测试完成!\n");
}

/**
 * 简单的PNG加载测试
 */
OPERATE_RET test_simple_png_load(const char* filename)
{
    if (!filename) {
        bk_printf("错误: 文件名为空\n");
        return OPRT_INVALID_PARM;
    }
    
    lv_img_dsc_t img = {0};
    
    bk_printf("正在加载PNG文件: %s\n", filename);
    
    // 加载PNG文件
    OPERATE_RET ret = png_img_load(filename, &img);
    
    if (ret == OPRT_OK) {
        bk_printf("加载成功!\n");
        bk_printf("图像信息:\n");
        bk_printf("  宽度: %d 像素\n", img.header.w);
        bk_printf("  高度: %d 像素\n", img.header.h);
        bk_printf("  数据大小: %d bytes\n", img.data_size);
        bk_printf("  颜色格式: %d\n", img.header.cf);
        
        // 卸载图像
        png_img_unload(&img);
        bk_printf("图像已卸载\n");
        
        return OPRT_OK;
    } else {
        bk_printf("加载失败, 错误码: %d\n", ret);
        return ret;
    }
} 