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

    class XJTexture
    {
        private:
            uint32_t mWidth;
            uint32_t mHeight;
            /* data */
            std::shared_ptr<XJVulkanImage> mImage;
            std::shared_ptr<VulkanImageView> mImageView;
            // std::shared_ptr<XJVulkanTextureSampler> mTextureSampler;

            
          
        public:
            XJTexture(const std::string &filePath);
            ~XJTexture();

            uint32_t XJGetWdidth() const{return mWidth;}
            uint32_t XJGetHeight() const{return mHeight;}

            XJVulkanImage *XJGetImage() const {return mImage.get();}
            VulkanImageView *XJGetImageView() const {return mImageView.get();}
        

            VkFormat mFormat;
            
    };
}



#endif