#include <Edit/SpdlogDebug.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace XJ
{
#ifdef _WIN32
    void SpdlogDebug::enableConsoleColor()
    {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hConsole == INVALID_HANDLE_VALUE)
            return;

        DWORD consoleMode = 0;
        if (!GetConsoleMode(hConsole, &consoleMode))
            return;

        consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        if (!SetConsoleMode(hConsole, consoleMode))
        {
            std::cerr << "启用控制台颜色失败" << std::endl;
        }
    }
#endif

    SpdlogDebug::SpdlogDebug()
    {
#ifdef _WIN32
        enableConsoleColor();
#endif
        EngineLog();
    }

    SpdlogDebug::~SpdlogDebug()
    {
        spdlog::shutdown();
    }

    void SpdlogDebug::EngineLog()
    {
        try
        {
            std::filesystem::create_directories("logs");

            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                "logs/XJEngine.log",
                104857600,
                5);

            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            spdlog::sinks_init_list sinks = {file_sink, console_sink};

            auto logger = std::make_shared<spdlog::logger>("XJEngine_logger", sinks);
            spdlog::set_default_logger(logger);
            logger->set_level(spdlog::level::trace);
            console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

            spdlog::trace("日志系统初始化成功（文件+控制台双输出）");
            spdlog::info("程序启动");
        }
        catch (const std::exception& e)
        {
            std::cerr << "日志初始化失败" << e.what() << std::endl;
        }
    }
}
