#include <Edit/SpdlogDebug.h>
#ifdef _WIN32
#include <windows.h>
#endif

namespace XJ
{
    #ifdef _WIN32
    void SpdlogDebug::enableConsoleColor() 
    {
        // 获取控制台输出句柄
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hConsole == INVALID_HANDLE_VALUE) return;
        
        // 获取当前控制台模式
        DWORD consoleMode = 0;
        if (!GetConsoleMode(hConsole, &consoleMode)) return;
        
        // 开启ANSI颜色转义序列支持（Windows 10/11必备）
        consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hConsole, consoleMode);
    }
    #endif
    SpdlogDebug::SpdlogDebug()
    {
        // ✅ 先启用控制台颜色，再初始化日志（顺序不能反）
        #ifdef _WIN32
        enableConsoleColor();
        #endif
        EngineLog();
        LogBuddha();
        
        //不同等级的kog
        spdlog::trace("初始化SPDLOG!");
        spdlog::debug("调试信息！");
        spdlog::info("你好，XJEngine");
        spdlog::warn("警告，很可能出错");
        spdlog::error("程序出错了！");
        spdlog::critical("最高级别Log！，比error还严重！！！");
        
        //格式输出
        spdlog::info("日志格式化输出：{}{}{}", 1, "hello Debug!", 3.14);
        //** */
        //下面是Logs测试
        //** */
        //设置日志等级，不设置的话默认Info
        //spdlog::set_level(spdlog::level::trace);//显示treace 及其更高等级的log
        //spdlog::set_level(spdlog::level::err);//显示err 及其更高等级的log
        //spdlog::set_level(spdlog::level::off);//关闭log
        
      
    }

    void SpdlogDebug::EngineLog()
    {
        try
        {
            //如果日志不存在 创建日志  放在logs文件夹
            std::filesystem::create_directories("logs");
            //创建文件夹 sink： 写入到logs/xj_engine.log
            //相对路径  是否追加  false覆盖 true 追加
            /*auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>
            (
                "logs/XJEngine.log",
                true
            );*/

            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>
            (
                "logs/XJEngine.log",
                104857600, // 100MB
                5          // 最多保留 5 个旧文件（xj_engine.log.1 ~ .5）
            );
            //创建控制台sink：带颜色
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            //组合两个sink 日志同时输出文件和控制台
            spdlog::sinks_init_list sinks = {file_sink, console_sink};

            //创建全局日志器， 并设置成默认
            auto logger = std::make_shared<spdlog::logger>("XJEngine_logger", sinks);
            spdlog::set_default_logger(logger);
            // 设置日志级别（必须 >= SPDLOG_ACTIVE_LEVEL，否则日志不会输出）
            // 这里设为 trace，匹配你在 CMake 中定义的 SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
            logger->set_level(spdlog::level::trace);
            //设置日志格式（可选，推荐包含时间、级别、日志内容）
            // 格式说明：[年-月-日 时:分:秒.毫秒] [日志级别] 日志内容
            //logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");    //无颜色
            console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");     // 有颜色
             // 测试日志（验证文件和控制台是否都能输出）
            spdlog::trace("日志系统初始化成功（文件+控制台双输出）");
            spdlog::info("程序启动");

        }
        catch(const std::exception& e)
        {
            //日志初始化失败
            std::cerr << "日志初始化失败" << e.what()<<std::endl;
        }
        
        
    }

    void SpdlogDebug::LogBuddha()
    {
        spdlog::info("                   .ooOoo.");
        spdlog::info("                  o8888888o");
        spdlog::info("                  88\" . \"88");
        spdlog::info("                  (| -_- |)");
        spdlog::info("                  O\\  =  /O");
        spdlog::info("               ____/`---'\\____");
        spdlog::info("             .'  \\\\|     |//  `.");
        spdlog::info("            /  \\\\|||  :  |||//  \\");
        spdlog::info("           /  _||||| -:- |||||-  \\");
        spdlog::info("           |   | \\\\\\  -  /// |   |");
        spdlog::info("           | \\_|  ''\\---/''  |   |");
        spdlog::info("           \\  .-\\__  `-`  ___/-. /");
        spdlog::info("         ___`. .'  /--.--\\  `. . __");
        spdlog::info("      .\"\" '<  `.___\\_<|>_/___.'  >'\"\".");
        spdlog::info("     | | :  `- \\`.;`\\ _ /`;.`/ - ` : | |");
        spdlog::info("     \\  \\ `-.   \\_ __\\ /__ _/   .-` /  /");
        spdlog::info("======`-.____`-.___\\_____/___.-`____.-'======");
        spdlog::info("                   `=---='");
        spdlog::info("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");
        spdlog::info("         佛祖保佑       永无BUG");
        spdlog::info("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");
    }
}