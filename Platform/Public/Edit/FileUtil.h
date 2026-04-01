//头文件保护宏
#ifndef FILE_UTIL_H
#define FILE_UTIL_H

#include "EditIncludes.h"
// #include <filesystem> // 添加这一行
//#include <string>
//资源路径定义
#ifdef XJ_DEFINE_RES_ROOT_DIR
#define XJ_RES_ROOT_DIR XJ_DEFINE_RES_ROOT_DIR
#else
#define XJ_RES_ROOT_DIR "Resource/"
#endif
//资源目录宏
#define XJ_RES_CONFIG_DIR                       XJ_RES_ROOT_DIR"Config/"
#define XJ_RES_SHADER_DIR                       XJ_RES_ROOT_DIR"Shader/"
#define XJ_RES_FONT_DIR                         XJ_RES_ROOT_DIR"Font/"
#define XJ_RES_MODEL_DIR                        XJ_RES_ROOT_DIR"Model/"
#define XJ_RES_MATERIAL_DIR                     XJ_RES_ROOT_DIR"Material/"
#define XJ_RES_TEXTURE_DIR                      XJ_RES_ROOT_DIR"Texture/"
#define XJ_RES_SCRNE_DIR                        XJ_RES_ROOT_DIR"Scene/"

namespace XJ
{  
     //获取文件名
    static std::string GetFileName(const std::string &filePath)
    {
        if(filePath.empty())
        {
            return filePath;
        }
        std::filesystem::path path(filePath);
        return path.filename().string();
    }
    //格式化文件大小
    static void FormatFileSize(std::uintmax_t fileSize, float *outSize, std::string &outUnit)
    {
        float size = static_cast<float>(fileSize);
        if(size < 1024)
        {
            outUnit = "B";
        }else if (size < 1024*1024)
        {
            size /= 1024;
            outUnit = "KB";
            /* code */
        }else if (size < 1024*1024*1024)
        {
            size /= (1024*1024);
            outUnit = "MB";
            /* code */
        }else
        {
            size /= (1024*1024*1024);
            outUnit = "GB";
            /* code */
        }
        *outSize = size;
    } 
    //格式化系统时间
    static std::string FormatSystemTime(std::filesystem::file_time_type fileTime)
    {
        std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm* tm = std::localtime(&time);

        std::stringstream ss;
        ss << std::put_time(tm, "%Y/%m/%d %H:%M");

        return ss.str();
    }
    //从文件读取字符数组
    static std::vector<char> ReadCharArrayFromFile(const std::string &filePath)
    {
        std::ifstream file(filePath, std::ios::ate | std::ios::binary);

        if(!file.is_open())
        {
            throw std::runtime_error("Could not open the file:" + filePath);
        }

        auto fileSzie =(size_t)file.tellg();
        std::vector<char> buffer(fileSzie);
        file.seekg(0);
        file.read(buffer.data(),fileSzie);
        file.close();
        return buffer;

    }
}



#endif
