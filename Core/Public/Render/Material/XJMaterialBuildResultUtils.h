#ifndef XJ_MATERIAL_BUILD_RESULT_UTILS_H
#define XJ_MATERIAL_BUILD_RESULT_UTILS_H

#include <string>

namespace XJ
{
    template<typename TResult>
    inline void AddMaterialBuildError(TResult& result, const std::string& message)
    {
        result.Errors.push_back(message);
    }

    template<typename TResult>
    inline void AddMaterialBuildWarning(TResult& result, const std::string& message)
    {
        result.Warnings.push_back(message);
    }
}

#endif