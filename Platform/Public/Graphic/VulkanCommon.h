#ifndef VULKAN_COMMON_H
#define VULKAN_COMMON_H

#include "spdlog/spdlog.h"
// Vulkan —— 注意顺序
#include <vulkan/vulkan.h>
#if defined(_WIN32)
    #include <vulkan/vulkan_win32.h>
#endif
//m = member（成员变量）
//k = konstant（常量）

#define XJDebug_Log(func) check_result(func, __FILE__, __LINE__, #func);
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))//std::size

struct DeviceFeature
{
    const char* name;
    bool required;
};

//构建layer 实验层和extension 扩展 
static bool checkDeviceFeature(const char* label,// 标签，用于打印日志
                            bool bExtension,// true = 检查扩展，false = 检查 feature
                            uint32_t availableCount,// 可用扩展/特性的数量
                            void * available, // 可用扩展/特性的数组（void* 类型）
                            uint32_t requestedCount, // 请求的数量
                            const DeviceFeature* requestedFeatures,// 请求的特性/扩展
                            uint32_t* outEnableCount,// 输出：启用了多少个扩展
                            const char* outEnableFeatures[]// 输出：启用了哪些扩展（char* 数组）
                        )
{   
    bool bFoundAllFeatures = true;//// 初始设为true，仅当缺失必需项时设为false

    *outEnableCount = 0;
    
    spdlog::trace("---------------------检查设备特性--------------------：{}", label);
    for(uint32_t  i = 0; i < requestedCount; i++)
    {
        bool bFound = false;
        const char *result = requestedFeatures[i].required ? "必需" : "可选";
        
        for(uint32_t  j = 0; j < availableCount; j++)
        {
            const char *availableName = bExtension?((VkExtensionProperties*)available)[j].extensionName:
                                                ((VkLayerProperties*)available)[j].layerName;
            if(strcmp(requestedFeatures[i].name, availableName) == 0)
            {
                outEnableFeatures[(*outEnableCount)++] = requestedFeatures[i].name;
                spdlog::trace("  已启用 {} 扩展/特性：{}", result, requestedFeatures[i].name);
                bFound = true;
                break;
            }
        }
        if(!bFound)
        {
            spdlog::warn("  未找到 {} 扩展/特性：{}", result, requestedFeatures[i].name);
            if(requestedFeatures[i].required)
                bFoundAllFeatures = false;
        }
        
    
    }
    spdlog::trace("  最终启用扩展/层总数：{}", *outEnableCount);
    spdlog::trace("-------------------设备特性检查完成--------------------：{}", label);
    return bFoundAllFeatures;
    
}

static void check_result(VkResult result, const char* fileName, uint32_t lineNumber, const char* funcName)
{
     if (result == VK_SUCCESS) 
     {
        //spdlog::trace("Vulkan 调用成功：文件：{} 行号：{} 函数：{}", fileName, lineNumber, funcName);
        return;
    }

    spdlog::error("Vulkan 错误：VkResult = {}，文件：{}，行号：{}, 函数：{}", 
                  static_cast<int>(result), fileName, lineNumber, funcName);

    if (result < 0)
        abort();
}

// ========== 格式化 VkFormat（表面格式） ==========
template <>
struct fmt::formatter<VkFormat> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(VkFormat fmt_val, format_context& ctx) {
        const char* str = "UNKNOWN_VK_FORMAT";
#define MAP_FMT(F) case F: str = #F; break
        switch (fmt_val) 
        {
            MAP_FMT(VK_FORMAT_UNDEFINED);
            MAP_FMT(VK_FORMAT_R8G8B8A8_UNORM);
            MAP_FMT(VK_FORMAT_R8G8B8A8_SRGB);
            MAP_FMT(VK_FORMAT_B8G8R8A8_UNORM);
            MAP_FMT(VK_FORMAT_B8G8R8A8_SRGB);
            MAP_FMT(VK_FORMAT_R32G32B32A32_SFLOAT);
            // 按需添加更多常用格式（参考Vulkan文档）
        }
#undef MAP_FMT
        return fmt::format_to(ctx.out(), "{}", str);
    }
};

// ========== 格式化 VkPresentModeKHR（呈现模式） ==========
template <>
struct fmt::formatter<VkPresentModeKHR> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(VkPresentModeKHR mode, format_context& ctx) 
    {
        const char* str = "UNKNOWN_VK_PRESENT_MODE";
#define MAP_MODE(M) case M: str = #M; break
        switch (mode) {
            MAP_MODE(VK_PRESENT_MODE_IMMEDIATE_KHR);    // 立即模式（可能撕裂）
            MAP_MODE(VK_PRESENT_MODE_MAILBOX_KHR);      // 邮箱模式（无撕裂，低延迟）
            MAP_MODE(VK_PRESENT_MODE_FIFO_KHR);         // 垂直同步（FIFO，无撕裂）
            MAP_MODE(VK_PRESENT_MODE_FIFO_RELAXED_KHR); // 宽松FIFO（部分场景低延迟）
        }
#undef MAP_MODE
        return fmt::format_to(ctx.out(), "{}", str);
    }
};


// 核心：将VkResult枚举转为可读字符串（覆盖90%常用返回码）
inline const char* vk_result_string(VkResult result) 
{
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS (操作成功)";
        case VK_NOT_READY: return "VK_NOT_READY (操作尚未完成)";
        case VK_TIMEOUT: return "VK_TIMEOUT (操作超时)";
        case VK_EVENT_SET: return "VK_EVENT_SET (事件已触发)";
        case VK_EVENT_RESET: return "VK_EVENT_RESET (事件已重置)";
        case VK_INCOMPLETE: return "VK_INCOMPLETE (结果不完整，需重试)";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY (主机内存不足)";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY (设备内存不足)";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED (初始化失败)";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST (设备丢失，需重建)";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED (内存映射失败)";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT (缺少指定的层)";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT (缺少指定的扩展)";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT (设备不支持该特性)";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER (驱动版本不兼容)";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS (创建的对象过多)";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED (格式不支持)";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR (表面丢失)";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR (原生窗口被占用)";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR (交换链参数非最优，但仍可用)";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR (交换链过期，需重建)";
        default: return "UNKNOWN_VK_RESULT (未知返回码)";
    }
}

// ========== 补全 VkFormat 转字符串（核心新增） ==========
inline const char* vk_format_string(VkFormat format) {
    // 重点覆盖交换链常用格式，其他格式按需扩展
    switch (format) {
        case VK_FORMAT_UNDEFINED:                return "VK_FORMAT_UNDEFINED (未定义格式)";
        // RGB/A 常用格式（桌面端主流）
        case VK_FORMAT_R8G8B8A8_UNORM:           return "VK_FORMAT_R8G8B8A8_UNORM (8位RGBA归一化)";
        case VK_FORMAT_R8G8B8A8_SRGB:            return "VK_FORMAT_R8G8B8A8_SRGB (8位RGBA sRGB)";
        case VK_FORMAT_B8G8R8A8_UNORM:           return "VK_FORMAT_B8G8R8A8_UNORM (8位BGRA归一化)";
        case VK_FORMAT_B8G8R8A8_SRGB:            return "VK_FORMAT_B8G8R8A8_SRGB (8位BGRA sRGB)";
        // 浮点格式（高性能渲染/计算）
        case VK_FORMAT_R32G32B32A32_SFLOAT:      return "VK_FORMAT_R32G32B32A32_SFLOAT (32位RGBA浮点)";
        case VK_FORMAT_R32G32_SFLOAT:            return "VK_FORMAT_R32G32_SFLOAT (32位RG浮点)";
        // 单通道格式
        case VK_FORMAT_R8_UNORM:                 return "VK_FORMAT_R8_UNORM (8位R通道归一化)";
        // 深度/模板格式（深度缓冲用）
        case VK_FORMAT_D32_SFLOAT:               return "VK_FORMAT_D32_SFLOAT (32位深度浮点)";
        case VK_FORMAT_D24_UNORM_S8_UINT:        return "VK_FORMAT_D24_UNORM_S8_UINT (24位深度+8位模板)";
        // 其他常用格式按需补充...
        default: return "UNKNOWN_VK_FORMAT (未知格式)";
    }
}

// ========== 新增：VkPresentModeKHR 转可读字符串 ==========
inline const char* vk_present_mode_string(VkPresentModeKHR presentMode) {
    // 覆盖99%场景的交换链呈现模式，附核心特性说明
    switch (presentMode) {
        case VK_PRESENT_MODE_IMMEDIATE_KHR:
            return "VK_PRESENT_MODE_IMMEDIATE_KHR (立即模式：无等待，可能屏幕撕裂，延迟最低)";
        case VK_PRESENT_MODE_MAILBOX_KHR:
            return "VK_PRESENT_MODE_MAILBOX_KHR (邮箱模式：无撕裂，低延迟，双缓冲/多缓冲覆盖)";
        case VK_PRESENT_MODE_FIFO_KHR:
            return "VK_PRESENT_MODE_FIFO_KHR (FIFO模式：垂直同步VSYNC，无撕裂，延迟中等)";
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
            return "VK_PRESENT_MODE_FIFO_RELAXED_KHR (宽松FIFO：VSYNC但超时后立即提交，低延迟+低撕裂)";
        case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR:
            return "VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR (按需刷新：共享表面专用)";
        case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR:
            return "VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR (持续刷新：共享表面专用)";
        default:
            return "UNKNOWN_VK_PRESENT_MODE (未知呈现模式)";
    }
}


// 将 VkImageLayout 枚举转换为字符串（用于调试 / 日志输出）
// 👉 非常重要：在做 Image Layout Transition / Validation Debug 时使用
static const char* vk_image_layout_string(VkImageLayout layout)
{
    switch (layout)
    {
        // ----------------------------
        // 基础 Layout
        // ----------------------------
        case VK_IMAGE_LAYOUT_UNDEFINED:
            // 初始状态（数据未定义，不能读取）
            return "VK_IMAGE_LAYOUT_UNDEFINED";
        case VK_IMAGE_LAYOUT_GENERAL:
            // 通用布局（几乎所有操作都支持，但性能较差）
            return "VK_IMAGE_LAYOUT_GENERAL";
        // ----------------------------
        // Render Pass 相关
        // ----------------------------
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            // 作为颜色附件（Render Target）使用
            return "VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            // 深度/模板写入
            return "VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
            // 深度只读（常用于后处理）
            return "VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL";
        // ----------------------------
        // Shader 采样
        // ----------------------------
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            // Shader 读取（Texture 最终状态）
            return "VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL";
        // ----------------------------
        // 传输（非常关键）
        // ----------------------------
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            // 作为拷贝源（Copy Source）
            return "VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL";
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            // 作为拷贝目标（Texture 上传阶段常用）
            return "VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL";
        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            // 预初始化（CPU 写入后 GPU 读取）
            return "VK_IMAGE_LAYOUT_PREINITIALIZED";
        // ----------------------------
        // 深度细分（新 Vulkan 版本）
        // ----------------------------
        case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
            return "VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
            return "VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
            return "VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
            return "VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
            return "VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
            return "VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL";
        // ----------------------------
        // 通用读写（Vulkan 新接口）
        // ----------------------------
        case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
            return "VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL:
            return "VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL";
        // ----------------------------
        // 交换链相关
        // ----------------------------
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            // 用于屏幕显示（Swapchain 最终状态）
            return "VK_IMAGE_LAYOUT_PRESENT_SRC_KHR";
        case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR:
            return "VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR";
        // ----------------------------
        // 扩展（部分 GPU 功能）
        // ----------------------------
        case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
            return "VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT";
        case VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR:
            return "VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR";
#ifdef VK_ENABLE_BETA_EXTENSIONS
        // 视频解码（很少用）
        case VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR:
            return "VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR";
        case VK_IMAGE_LAYOUT_VIDEO_DECODE_SRC_KHR:
            return "VK_IMAGE_LAYOUT_VIDEO_DECODE_SRC_KHR";
        case VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR:
            return "VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR";
        // 视频编码
        case VK_IMAGE_LAYOUT_VIDEO_ENCODE_DST_KHR:
            return "VK_IMAGE_LAYOUT_VIDEO_ENCODE_DST_KHR";
        case VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR:
            return "VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR";
        case VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR:
            return "VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR";
#endif
        case VK_IMAGE_LAYOUT_MAX_ENUM:
            break;
        default:
            // ❗ 非法 / 未处理的 layout（调试用）
            return "UNKNOWN_LAYOUT";
    }
    return "UNKNOWN_LAYOUT";
}

inline const char* vk_color_space_string(VkColorSpaceKHR cs)
{
    switch (cs)
    {
    case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
        return "VK_COLOR_SPACE_SRGB_NONLINEAR_KHR";
    case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT:
        return "VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT";
    case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
        return "VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT";
    default:
        return "UNKNOWN_VK_COLOR_SPACE";
    }
}


static bool IsDepthOnlyFormat(VkFormat format)
{
    return format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT;
}

static bool IsDepthStencilFormat(VkFormat format)
{
    return IsDepthOnlyFormat(format) || format == VK_FORMAT_D16_UNORM_S8_UINT
                                     || format == VK_FORMAT_D24_UNORM_S8_UINT
                                     || format == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

#endif