#include "png_loader.h"
#include "tkl_system.h"
#include "bk_printf.h"
#include "lvgl.h"

// 示例：加载并显示PNG图像
void example_load_and_display_png(void)
{
    bk_printf("=== PNG加载器使用示例 ===\n");
    
    // 1. 加载PNG图像
    lv_img_dsc_t emoji_img = {0};
    const char* emoji_file = "emoji.png";
    
    bk_printf("正在加载表情图像: %s\n", emoji_file);
    
    OPERATE_RET ret = png_img_load(emoji_file, &emoji_img);
    if (ret == OPRT_OK) {
        bk_printf("表情图像加载成功!\n");
        bk_printf("图像尺寸: %d x %d\n", emoji_img.header.w, emoji_img.header.h);
        
        // 2. 在LVGL中创建图像对象
        lv_obj_t *img_obj = lv_img_create(lv_scr_act());
        if (img_obj) {
            // 设置图像源
            lv_img_set_src(img_obj, &emoji_img);
            
            // 设置位置和大小
            lv_obj_set_pos(img_obj, 100, 100);
            lv_obj_set_size(img_obj, emoji_img.header.w, emoji_img.header.h);
            
            bk_printf("图像对象创建成功，位置: (100, 100)\n");
            
            // 3. 等待一段时间后卸载图像
            tkl_system_delay_millisecond(3000);
            
            // 4. 清理资源
            lv_obj_del(img_obj);
            png_img_unload(&emoji_img);
            
            bk_printf("图像已卸载，资源已清理\n");
            
        } else {
            bk_printf("创建图像对象失败\n");
            png_img_unload(&emoji_img);
        }
        
    } else {
        bk_printf("表情图像加载失败，错误码: %d\n", ret);
    }
}

// 示例：批量加载多个PNG图像
void example_batch_load_pngs(void)
{
    bk_printf("=== 批量加载PNG图像示例 ===\n");
    
    const char* image_files[] = {
        "icon_home.png",
        "icon_settings.png", 
        "icon_user.png",
        "background.png"
    };
    
    const int num_images = sizeof(image_files) / sizeof(image_files[0]);
    lv_img_dsc_t* image_descs = tkl_system_malloc(sizeof(lv_img_dsc_t) * num_images);
    
    if (!image_descs) {
        bk_printf("内存分配失败\n");
        return;
    }
    
    // 初始化图像描述符数组
    memset(image_descs, 0, sizeof(lv_img_dsc_t) * num_images);
    
    int loaded_count = 0;
    
    // 批量加载图像
    for (int i = 0; i < num_images; i++) {
        bk_printf("加载图像 %d/%d: %s\n", i + 1, num_images, image_files[i]);
        
        OPERATE_RET ret = png_img_load(image_files[i], &image_descs[i]);
        if (ret == OPRT_OK) {
            loaded_count++;
            bk_printf("  ✓ 加载成功\n");
        } else {
            bk_printf("  ✗ 加载失败\n");
        }
    }
    
    bk_printf("批量加载完成，成功加载 %d/%d 个图像\n", loaded_count, num_images);
    
    // 使用加载的图像（这里只是示例，实际应用中可能需要显示或处理）
    for (int i = 0; i < num_images; i++) {
        if (image_descs[i].data != NULL) {
            bk_printf("图像 %d: %d x %d, 大小: %d bytes\n", 
                     i, image_descs[i].header.w, image_descs[i].header.h, 
                     image_descs[i].data_size);
        }
    }
    
    // 清理所有图像
    for (int i = 0; i < num_images; i++) {
        if (image_descs[i].data != NULL) {
            png_img_unload(&image_descs[i]);
        }
    }
    
    // 释放描述符数组
    tkl_system_free(image_descs);
    
    bk_printf("所有图像已卸载，内存已释放\n");
}

// 示例：错误处理和恢复
void example_error_handling_and_recovery(void)
{
    bk_printf("=== 错误处理和恢复示例 ===\n");
    
    lv_img_dsc_t test_img = {0};
    
    // 测试各种错误情况
    const char* test_cases[] = {
        NULL,                    // 空文件名
        "",                      // 空字符串
        "nonexistent.png",       // 不存在的文件
        "invalid_file.txt",      // 非PNG文件
        "very_long_filename_that_might_cause_issues.png"  // 超长文件名
    };
    
    for (int i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        const char* test_file = test_cases[i];
        
        bk_printf("测试用例 %d: %s\n", i + 1, 
                 test_file ? test_file : "NULL");
        
        // 尝试加载
        OPERATE_RET ret = png_img_load(test_file, &test_img);
        
        if (ret == OPRT_OK) {
            bk_printf("  ✓ 意外成功，应该检查\n");
            png_img_unload(&test_img);
        } else {
            bk_printf("  ✓ 正确处理错误，错误码: %d\n", ret);
        }
        
        // 确保图像描述符被重置
        memset(&test_img, 0, sizeof(lv_img_dsc_t));
    }
    
    bk_printf("错误处理测试完成\n");
}

// 示例：性能监控
void example_performance_monitoring(void)
{
    bk_printf("=== 性能监控示例 ===\n");
    
    const char* test_image = "performance_test.png";
    lv_img_dsc_t img = {0};
    
    // 测试多次加载的性能
    const int test_iterations = 5;
    uint32_t total_time = 0;
    int success_count = 0;
    
    for (int i = 0; i < test_iterations; i++) {
        bk_printf("性能测试 %d/%d\n", i + 1, test_iterations);
        
        uint32_t start_time = tkl_system_get_tick_count();
        
        OPERATE_RET ret = png_img_load(test_image, &img);
        
        uint32_t end_time = tkl_system_get_tick_count();
        uint32_t load_time = end_time - start_time;
        
        if (ret == OPRT_OK) {
            success_count++;
            total_time += load_time;
            
            bk_printf("  加载时间: %d ms\n", load_time);
            bk_printf("  图像大小: %d x %d\n", img.header.w, img.header.h);
            
            // 卸载图像
            png_img_unload(&img);
            
        } else {
            bk_printf("  加载失败，错误码: %d\n", ret);
        }
        
        // 短暂延迟
        tkl_system_delay_millisecond(100);
    }
    
    // 计算性能统计
    if (success_count > 0) {
        uint32_t avg_time = total_time / success_count;
        bk_printf("性能统计:\n");
        bk_printf("  成功次数: %d/%d\n", success_count, test_iterations);
        bk_printf("  平均加载时间: %d ms\n", avg_time);
        bk_printf("  总测试时间: %d ms\n", total_time);
    } else {
        bk_printf("所有测试都失败了\n");
    }
}

// 主示例函数
void run_png_loader_examples(void)
{
    bk_printf("开始运行PNG加载器示例...\n\n");
    
    // 运行各种示例
    example_load_and_display_png();
    bk_printf("\n");
    
    example_batch_load_pngs();
    bk_printf("\n");
    
    example_error_handling_and_recovery();
    bk_printf("\n");
    
    example_performance_monitoring();
    bk_printf("\n");
    
    bk_printf("所有示例运行完成!\n");
} 