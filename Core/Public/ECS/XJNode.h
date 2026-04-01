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
            XJUUID mId = 0;
            std::string mName;

            XJNode *mParent = nullptr;//父级
            std::vector<XJNode*> mChildren;//子集
        public:
            XJNode(/* args */);
            virtual ~XJNode();

            XJUUID XJGetId() const {return mId;};
            void XJSetId(const XJUUID &nodeId)  { mId = nodeId;};
            const std::string &XJGetName() const {return mName;};
            void XJSetName(const std::string &name)  { mName = name;};

            const std::vector<XJNode *> &XJGetChildren() const;
            bool HasParent();
            bool HasChildren();
            void XJSetParent(XJNode *node) { mParent = node;};
            XJNode *XJGetParent() const {return mParent;};
            void XJAddChild(XJNode *node);
            void XJRemoveChild(XJNode *node);

    };
    

}

#endif