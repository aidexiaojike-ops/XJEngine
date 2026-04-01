#include "ECS/XJNode.h"


namespace XJ
{
    XJNode::XJNode(/* args */) = default;
    XJNode::~XJNode() = default;
   
    const std::vector<XJNode *>& XJNode::XJGetChildren() const
    {

    }
    bool XJNode::HasParent()
    {
        return mParent != nullptr;
    }

    bool XJNode::HasChildren()
    {
        return !mChildren.empty();
    }

    void XJNode::XJAddChild(XJNode *node)//先移除子节点后添加
    {
        if(node->HasParent())
        {
            node->XJGetParent()->XJRemoveChild(node);
        }
        node->mParent = this;
        mChildren.push_back(node);
    }

    void XJNode::XJRemoveChild(XJNode *node)//移除子节点
    {
        if(!HasChildren())
        {
            return;
        }

        for(auto it = mChildren.begin(); it != mChildren.end(); ++it)
        {
            if(node == *it)
            {
                mChildren.erase(it);
                break;
            }
        }
    }
}