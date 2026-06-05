#ifndef XJ_NODE_H
#define XJ_NODE_H

#include "ECS/XJUUID.h"

namespace XJ
{
    class XJNode
    {
        private:
            /* data */
            //哈希表要用到的
            XJUUID mUUID = 0;
            std::string mName;

            XJNode *mParent = nullptr;//父级
            std::vector<XJNode*> mChildren;//子集
        public:
            XJNode(/* args */);
            virtual ~XJNode();

            XJUUID XJGetUUID() const {return mUUID;};
            void XJSetUUID(const XJUUID &nodeUUID)  { mUUID = nodeUUID;};
            const std::string &XJGetName() const {return mName;};
            void XJSetName(const std::string &name)  { mName = name;};

            const std::vector<XJNode *> &XJGetChildren() const;
            // bool HasParent();
            bool HasParent() const;
            bool HasChildren();
            void XJSetParent(XJNode *node) { mParent = node;};
            XJNode *XJGetParent() const {return mParent;};
            void XJAddChild(XJNode *node);//添加子节点
            void XJClearChildren();//清理子节点
            void XJRemoveChild(XJNode *node);//移除子节点


    };
    

}

#endif
