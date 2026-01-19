# PNG图像加载器使用指南

## 概述

PNG图像加载器是一个专门用于加载和解析PNG格式图像文件的模块，支持多种解码方式，并与LVGL图形库完全兼容。

## 文件结构

```
img_utility/
├── png_loader.h          # 主要头文件
├── png_loader.c          # 主要实现文件
├── png_loader_test.h     # 测试头文件
├── png_loader_test.c     # 测试实现文件
├── png_loader_example.c  # 使用示例文件
└── README_PNG_LOADER.md  # 本文档
```

## 主要功能

### 1. PNG文件加载
- 支持多种PNG解码方式（STB Image、libspng、LVGL内置解码器）
- 自动检测图像格式和颜色深度
- 支持透明通道（Alpha）

### 2. 内存管理
- 使用PSRAM进行大容量图像数据存储
- 自动内存分配和释放
- 防止内存泄漏

### 3. LVGL兼容性
- 支持LVGL v8和v9版本
- 自动设置正确的图像描述符
- 可直接用于LVGL图像显示

## 使用方法

### 基本用法

```c
#include "img_utility/png_loader.h"

// 声明图像描述符
lv_img_dsc_t my_image = {0};

// 加载PNG文件
OPERATE_RET ret = png_img_load("my_image.png", &my_image);
if (ret == OPRT_OK) {
    // 加载成功，可以使用my_image
    printf("图像尺寸: %d x %d\n", my_image.header.w, my_image.header.h);
    
    // 在LVGL中使用
    lv_obj_t *img_obj = lv_img_create(lv_scr_act());
    lv_img_set_src(img_obj, &my_image);
    
    // 使用完毕后卸载
    png_img_unload(&my_image);
}
```

### 批量加载

```c
const char* image_files[] = {"icon1.png", "icon2.png", "background.png"};
lv_img_dsc_t images[3] = {0};

// 批量加载
for (int i = 0; i < 3; i++) {
    if (png_img_load(image_files[i], &images[i]) == OPRT_OK) {
        printf("图像 %d 加载成功\n", i);
    }
}

// 使用图像...

// 批量卸载
for (int i = 0; i < 3; i++) {
    png_img_unload(&images[i]);
}
```

## 配置选项

### 编译时配置

在编译前，可以通过定义以下宏来选择解码方式：

```c
// 使用STB Image库解码（推荐用于小图像）
#define PNG_DECODE_WITH_STB_IMAGE

// 使用libspng库解码（推荐用于高质量图像）
#define PNG_DECODE_WITH_LIBSPNG

// 使用内存解码模式
#define PNG_IMG_DECODE_IN_MEMORY

// 启用解码时间测试
#define IMG_DECODING_TIME_TEST
```

### 运行时配置

```c
// 设置解码质量（如果支持）
// 某些解码器可能支持质量设置
```

## 错误处理

### 返回值说明

- `OPRT_OK`: 操作成功
- `OPRT_INVALID_PARM`: 参数无效
- `OPRT_MALLOC_FAILED`: 内存分配失败
- `OPRT_COM_ERROR`: 通用错误

### 错误处理示例

```c
OPERATE_RET ret = png_img_load(filename, &img);
switch (ret) {
    case OPRT_OK:
        printf("加载成功\n");
        break;
    case OPRT_INVALID_PARM:
        printf("参数错误\n");
        break;
    case OPRT_MALLOC_FAILED:
        printf("内存不足\n");
        break;
    default:
        printf("未知错误: %d\n", ret);
        break;
}
```

## 测试

### 运行完整测试套件

```c
#include "img_utility/png_loader_test.h"

// 运行所有测试
png_loader_run_tests();
```

### 运行单个测试

```c
#include "img_utility/png_loader_test.h"

// 测试单个文件
test_simple_png_load("test.png");
```

### 测试覆盖范围

1. **功能测试**: 测试PNG文件加载的基本功能
2. **内存管理测试**: 测试内存分配、释放和泄漏
3. **错误处理测试**: 测试各种错误情况的处理
4. **性能测试**: 测试加载速度和资源使用

## 示例代码

### 运行所有示例

```c
#include "img_utility/png_loader_example.h"

// 运行所有示例
run_png_loader_examples();
```

### 示例类型

1. **基本显示示例**: 加载PNG并在LVGL中显示
2. **批量加载示例**: 同时加载多个PNG文件
3. **错误处理示例**: 处理各种错误情况
4. **性能监控示例**: 监控加载性能

## 性能优化建议

### 1. 图像格式优化
- 使用适当的颜色深度（16位通常足够）
- 压缩PNG文件大小
- 考虑使用硬件加速解码

### 2. 内存管理
- 及时卸载不需要的图像
- 避免同时加载过多大图像
- 使用PSRAM进行大图像存储

### 3. 解码器选择
- 小图像：使用STB Image
- 大图像：使用libspng
- 特殊需求：使用LVGL内置解码器

## 常见问题

### Q1: 加载PNG文件失败怎么办？
A: 检查文件路径、文件格式、内存是否充足，查看错误返回值。

### Q2: 图像显示不正确？
A: 检查颜色格式设置、LVGL版本兼容性、图像数据完整性。

### Q3: 内存不足？
A: 使用PSRAM、及时卸载图像、检查内存泄漏。

### Q4: 加载速度慢？
A: 优化图像大小、选择合适的解码器、使用硬件加速。

## 版本历史

- v0.1: 初始版本，支持基本PNG加载功能
- 支持多种解码方式
- 完整的错误处理
- 测试和示例代码

## 许可证

Copyright 2021-2030 Tuya Inc. All Rights Reserved.

## 联系方式

如有问题或建议，请联系开发团队。 