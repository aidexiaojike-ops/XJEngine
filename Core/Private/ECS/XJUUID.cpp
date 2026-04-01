#include "ECS/XJUUID.h"


namespace XJ
{
    //随机引擎
    static std::random_device s_RandomDevice;
    static std::mt19937_64 s_Engine(s_RandomDevice());
    static std::uniform_int_distribution<uint32_t> s_UniformDistribution(1, UINT32_MAX);


    XJUUID::XJUUID(/* args */) : mUUID(s_UniformDistribution(s_Engine))
    {

    }
    XJUUID::XJUUID(uint32_t uuid) : mUUID(uuid)
    {

    }
    XJUUID::XJUUID(const XJUUID&) = default;
    XJUUID::~XJUUID()
    {

    }
}