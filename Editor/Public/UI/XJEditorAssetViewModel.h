#ifndef XJ_EDITOR_ASSET_VIEW_MODEL_H
#define XJ_EDITOR_ASSET_VIEW_MODEL_H

#include "Asset/XJAsset.h"
#include "Edit/Mathinclude.h"

#include <filesystem>
#include <string>
#include <vector>

namespace XJ
{
    // 资产验证时的严重性级别
    enum class XJEditorAssetValidationSeverity
    {
        Info = 0,   // 提示信息
        Warning,    // 警告
        Error       // 错误
    };

    // 单条资产验证消息的描述
    struct XJEditorAssetValidationMessageView
    {
        XJEditorAssetValidationSeverity Severity = XJEditorAssetValidationSeverity::Info;// 严重性
        std::string ParameterName;
        std::string Message;
    };

    // 着色器验证结果的视图
    struct XJEditorShaderValidationView
    {
        bool Valid = false;
        std::vector<XJEditorAssetValidationMessageView> Messages;

        bool Empty() const
        {
            return Messages.empty();
        }

        // 是否包含至少一个错误级别消息
        bool HasErrors() const
        {
            for (const auto& message : Messages)
            {
                if (message.Severity == XJEditorAssetValidationSeverity::Error)
                    return true;
            }

            return false;
        }

        // 是否包含至少一个警告级别消息
        bool HasWarnings() const
        {
            for (const auto& message : Messages)
            {
                if (message.Severity == XJEditorAssetValidationSeverity::Warning)
                    return true;
            }

            return false;
        }
    };

    // 网格包围盒数据显示结构（整体 + 每个 submesh）
    struct XJEditorMeshBoundsView
    {
        bool Valid = false;
        glm::vec3 Min{0.0f};
        glm::vec3 Max{0.0f};
        glm::vec3 Center{0.0f};
        glm::vec3 Extents{0.0f};

        struct SubmeshBounds
        {
            uint32_t SubmeshIndex = 0;
            uint32_t MaterialSlot = 0;
            glm::vec3 Min{0.0f};
            glm::vec3 Max{0.0f};
        };
        std::vector<SubmeshBounds> Submeshes;
    };

    // 编辑器资产详细信息视图，汇集资产的基本信息、验证状态与包围盒
    struct XJEditorAssetDetailsView
    {
        bool Valid = false; // 视图数据是否有效（资产是否存在等）

        // 资产基本信息
        XJAssetHandle Handle = 0;
        XJAssetType Type = XJAssetType::None;
        std::string Name;
        std::filesystem::path SourcePath;
        std::filesystem::path ImportedPath;

        // 着色器验证信息
        bool HasShaderValidation = false;
        std::filesystem::path ShaderPath;                      // 对应的着色器文件路径
        XJEditorShaderValidationView ShaderValidation;         // 着色器验证详情

        // 网格包围盒信息
        bool HasMeshBounds = false;
        XJEditorMeshBoundsView MeshBounds;                     // 网格包围盒详情
    };
}

#endif
