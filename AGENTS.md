# 项目协作约定

- 新增或修改的诊断、调试及测试日志统一使用 `spdlog::`。
- 不使用 `std::cout`、`std::cerr`、`printf` 或平台调试输出记录日志。UI 文本和字符串格式化不属于日志。
- `spdlog` 只负责记录消息。需要异常传播时，先记录日志，再抛出标准异常；不要写 `throw spdlog::error(...)`。
- 不增加逐帧 info/debug 日志来代替性能统计；正常采集使用统计快照。
- 新增 View 逻辑只读取 ViewModel、提交请求。通过 Controller/Service 应用 ECS 或渲染状态修改，保留已有渲染宿主生命周期接口。
