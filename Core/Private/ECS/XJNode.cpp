#include "ECS/XJNode.h"


namespace
{
    bool IsAncestorOf(const XJ::XJNode* possibleAncestor, const XJ::XJNode* node)
    {
        const XJ::XJNode* current = node;
        while (current)
        {
            if (current == possibleAncestor)
                return true;

            current = current->XJGetParent();
        }

        return false;
    }
}

namespace XJ
{
    XJNode::XJNode(/* args */) = default;
    XJNode::~XJNode()
    {
        if (mParent)
            mParent->XJRemoveChild(this);

        XJClearChildren();
    }
   
    const std::vector<XJNode *>& XJNode::XJGetChildren() const
    {
        return mChildren;
    }
    bool XJNode::HasParent() const
    {
        return mParent != nullptr;
    }

    bool XJNode::HasChildren()
    {
        return !mChildren.empty();
    }

    void XJNode::XJAddChild(XJNode *node)//先移除子节点后添加
    {
        if (!node || node == this)
            return;

        // 防止把祖先挂到子节点下面形成环，DestroyEntity 递归会爆栈。
        if (IsAncestorOf(node, this))
            return;

        if(node->HasParent())
        {
            node->XJGetParent()->XJRemoveChild(node);
        }
        node->mParent = this;
        mChildren.push_back(node);
    }

    void XJNode::XJRemoveChild(XJNode *node)//移除子节点
    {
        if(!node || !HasChildren())
        {
            return;
        }

        for(auto it = mChildren.begin(); it != mChildren.end(); ++it)
        {
            if(node == *it)
            {
                mChildren.erase(it);
                if (node->mParent == this)
                    node->mParent = nullptr;
                break;
            }
        }
    }

    void XJNode::XJClearChildren()
    {
        for(auto* child : mChildren)
        {
            if(child)
            {
                if (child->mParent == this)
                    child->mParent = nullptr;
            }
        }

        mChildren.clear();
    }
}
