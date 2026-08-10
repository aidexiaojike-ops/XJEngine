#include "ECS/XJUUID.h"
#include "ECS/XJReservedUUID.h"

#include <chrono>
#include <functional>
#include <limits>
#include <random>
#include <thread>


namespace XJ
{

    namespace
    {
        uint64_t BuildThreadSeed()
        {
            std::random_device randomDevice;//随机设备

            const uint64_t rendomPart = (static_cast<uint64_t>(randomDevice()) << 32) | randomDevice();//随机数生成器

            const uint64_t timePart = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());//获取当前时间戳

            const uint64_t threadPart = static_cast<uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));//获取当前线程ID

            return rendomPart ^ timePart ^ threadPart;
        }

        uint64_t GenerateUUID()
        {
            // 每个线程一套随机引擎，避免多个线程同时访问 mt19937_64 造成数据竞争。
            thread_local std::mt19937_64 engine(BuildThreadSeed());
            thread_local std::uniform_int_distribution<uint64_t> distribution(
                1,
                std::numeric_limits<uint64_t>::max());

            uint64_t uuid = 0;
            do
            {
                uuid = distribution(engine);
            }
            while (XJIsReservedUUID(uuid));

            return uuid;
        }
    }

    XJUUID::XJUUID(/* args */) : mUUID(GenerateUUID())
    {

    }
    XJUUID::XJUUID(uint64_t uuid) : mUUID(uuid)
    {

    }
    XJUUID::XJUUID(const XJUUID&) = default;
    XJUUID::~XJUUID()
    {

    }
}
