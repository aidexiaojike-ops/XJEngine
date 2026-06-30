#ifndef XJ_SHADER_VALIDATION_H
#define XJ_SHADER_VALIDATION_H

#include <string>
#include <vector>

namespace XJ
{
    enum class XJShaderValidationSeverity//验证严重性
    {
        Info = 0,
        Warning,
        Error
    };

    struct XJShaderValidationMessage//验证消息
    {
        XJShaderValidationSeverity Severity = XJShaderValidationSeverity::Info;
        std::string ParameterName;
        std::string Message;
    };

    struct XJShaderValidationResult//验证结果
    {
        std::vector<XJShaderValidationMessage> Messages;

        bool HasErrors() const
        {
            for(const auto& message : Messages)
            {
                if(message.Severity == XJShaderValidationSeverity::Error)
                    return true;
            }

            return false;
        }

        bool HasWarnings() const
        {
            for(const auto& message : Messages)
            {
                if(message.Severity == XJShaderValidationSeverity::Warning)
                    return true;
            }

            return false;
        }

        bool Empty() const
        {
            return Messages.empty();
        }
    };
}
#endif