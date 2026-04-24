#ifndef XJ_TEXTRUE_H
#define XJ_TEXTRUE_H

#include "Graphic/VulkanCommon.h"

namespace XJ
{
    class XJVulkanImage;
    class VulkanImageView;
    class XJVulkanBuffer;
    class XJAppContext;
    // class XJVulkanTextureSampler;
    struct RGBAColor//每个像素的信息
    {   
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;
        
    };

    class XJTexture
    {
        private:
            void CreateImage(size_t size, void *data);//图片数据大小 和所有数据
            uint32_t mWidth;
            uint32_t mHeight;
            /* data */
            std::shared_ptr<XJVulkanImage> mImage;
            std::shared_ptr<VulkanImageView> mImageView;
            // std::shared_ptr<XJVulkanTextureSampler> mTextureSampler;

            
          
        public:
            XJTexture(const std::string &filePath);//传入文件
            XJTexture(uint32_t width, uint32_t height, RGBAColor *pixels);//图片的长宽高 和像素信息
            ~XJTexture();

            uint32_t XJGetWdidth() const{return mWidth;}
            uint32_t XJGetHeight() const{return mHeight;}

            XJVulkanImage *XJGetImage() const {return mImage.get();}
            VulkanImageView *XJGetImageView() const {return mImageView.get();}
        

            VkFormat mFormat;
            
    };
}



#endif