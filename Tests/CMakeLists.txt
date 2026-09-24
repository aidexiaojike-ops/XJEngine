add_executable(XJScriptLexerTests
    ${CMAKE_CURRENT_SOURCE_DIR}/Script/XJScriptLexerTests.cpp
    ${PROJECT_SOURCE_DIR}/Core/Private/Script/Language/XJScriptLexer.cpp
)

target_include_directories(XJScriptLexerTests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/Core/Public
)

target_compile_features(XJScriptLexerTests PRIVATE cxx_std_20)

add_test(
    NAME XJScriptLexerTests
    COMMAND $<TARGET_FILE:XJScriptLexerTests>
)

add_executable(XJScriptParserTests
    ${CMAKE_CURRENT_SOURCE_DIR}/Script/XJScriptParserTests.cpp
    ${PROJECT_SOURCE_DIR}/Core/Private/Script/Language/XJScriptLexer.cpp
    ${PROJECT_SOURCE_DIR}/Core/Private/Script/Language/XJScriptParser.cpp
)

target_include_directories(XJScriptParserTests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/Core/Public
)

target_compile_features(XJScriptParserTests PRIVATE cxx_std_20)

add_test(
    NAME XJScriptParserTests
    COMMAND $<TARGET_FILE:XJScriptParserTests>
)

add_executable(XJScriptSemanticAnalyzerTests
    ${CMAKE_CURRENT_SOURCE_DIR}/Script/XJScriptSemanticAnalyzerTests.cpp
    ${PROJECT_SOURCE_DIR}/Core/Private/Script/Language/XJScriptLexer.cpp
    ${PROJECT_SOURCE_DIR}/Core/Private/Script/Language/XJScriptParser.cpp
    ${PROJECT_SOURCE_DIR}/Core/Private/Script/Language/XJScriptSemanticAnalyzer.cpp
)

target_include_directories(XJScriptSemanticAnalyzerTests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/Core/Public
)

target_compile_features(XJScriptSemanticAnalyzerTests PRIVATE cxx_std_20)

add_test(
    NAME XJScriptSemanticAnalyzerTests
    COMMAND $<TARGET_FILE:XJScriptSemanticAnalyzerTests>
)

add_executable(XJScriptCompilerTests
    ${CMAKE_CURRENT_SOURCE_DIR}/Script/XJScriptCompilerTests.cpp
    ${PROJECT_SOURCE_DIR}/Core/Private/Script/Language/XJScriptLexer.cpp
    ${PROJECT_SOURCE_DIR}/Core/Private/Script/Language/XJScriptParser.cpp
    ${PROJECT_SOURCE_DIR}/Core/Private/Script/Language/XJScriptSemanticAnalyzer.cpp
    ${PROJECT_SOURCE_DIR}/Core/Private/Script/Compiler/XJScriptCompiler.cpp
    ${PROJECT_SOURCE_DIR}/Core/Private/Script/Bytecode/XJScriptBytecodeVerifier.cpp
)

target_include_directories(XJScriptCompilerTests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/Core/Public
)

target_compile_features(XJScriptCompilerTests PRIVATE cxx_std_20)

add_test(
    NAME XJScriptCompilerTests
    COMMAND $<TARGET_FILE:XJScriptCompilerTests>
)

add_executable(XJScriptBytecodeVerifierTests
    ${CMAKE_CURRENT_SOURCE_DIR}/Script/XJScriptBytecodeVerifierTests.cpp
    ${PROJECT_SOURCE_DIR}/Core/Private/Script/Bytecode/XJScriptBytecodeVerifier.cpp
)

target_include_directories(XJScriptBytecodeVerifierTests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/Core/Public
)

target_compile_features(XJScriptBytecodeVerifierTests PRIVATE cxx_std_20)

add_test(
    NAME XJScriptBytecodeVerifierTests
    COMMAND $<TARGET_FILE:XJScriptBytecodeVerifierTests>
)

add_executable(XJScriptRuntimeTests
    ${CMAKE_CURRENT_SOURCE_DIR}/Script/XJScriptRuntimeTests.cpp
    ${PROJECT_SOURCE_DIR}/Core/Private/Script/Language/XJScriptLexer.cpp
    ${PROJECT_SOURCE_DIR}/Core/Private/Script/Language/XJScriptParser.cpp
    ${PROJECT_SOURCE_DIR}/Core/Private/Script/Language/XJScriptSemanticAnalyzer.cpp
    ${PROJECT_SOURCE_DIR}/Core/Private/Script/Compiler/XJScriptCompiler.cpp
    ${PROJECT_SOURCE_DIR}/Core/Private/Script/Bytecode/XJScriptBytecodeVerifier.cpp
    ${PROJECT_SOURCE_DIR}/Core/Private/Script/Runtime/XJScriptRuntime.cpp
)

target_include_directories(XJScriptRuntimeTests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/Core/Public
)

target_compile_features(XJScriptRuntimeTests PRIVATE cxx_std_20)

add_test(
    NAME XJScriptRuntimeTests
    COMMAND $<TARGET_FILE:XJScriptRuntimeTests>
)


add_executable(
    XJEcsScriptNativeInvokerTests
    ${CMAKE_CURRENT_SOURCE_DIR}/Script/XJEcsScriptNativeInvokerTests.cpp
)

target_link_libraries(
    XJEcsScriptNativeInvokerTests
    PRIVATE
        Core
)

target_compile_features(
    XJEcsScriptNativeInvokerTests
    PRIVATE
        cxx_std_20
)

add_test(
    NAME XJEcsScriptNativeInvokerTests
    COMMAND
        $<TARGET_FILE:XJEcsScriptNativeInvokerTests>
)

add_executable(
    XJScriptAssetCompilerTests
    ${CMAKE_CURRENT_SOURCE_DIR}/Script/XJScriptAssetCompilerTests.cpp
)

target_link_libraries(
    XJScriptAssetCompilerTests
    PRIVATE
        Core
)

target_compile_features(
    XJScriptAssetCompilerTests
    PRIVATE
        cxx_std_20
)

add_test(
    NAME XJScriptAssetCompilerTests
    COMMAND
        $<TARGET_FILE:XJScriptAssetCompilerTests>
)

add_executable(
    XJScriptAssetLoaderTests
    ${CMAKE_CURRENT_SOURCE_DIR}/Script/XJScriptAssetLoaderTests.cpp
)

target_link_libraries(
    XJScriptAssetLoaderTests
    PRIVATE
        Core
)

target_compile_features(
    XJScriptAssetLoaderTests
    PRIVATE
        cxx_std_20
)

add_test(
    NAME XJScriptAssetLoaderTests
    COMMAND
        $<TARGET_FILE:XJScriptAssetLoaderTests>
)

add_executable(
    XJScriptComponentTests
    ${CMAKE_CURRENT_SOURCE_DIR}/Script/XJScriptComponentTests.cpp
)

target_link_libraries(
    XJScriptComponentTests
    PRIVATE
        Core
)

target_compile_features(
    XJScriptComponentTests
    PRIVATE
        cxx_std_20
)

add_test(
    NAME XJScriptComponentTests
    COMMAND
        $<TARGET_FILE:XJScriptComponentTests>
)

add_executable(
    XJScriptSceneSerializationTests
    ${CMAKE_CURRENT_SOURCE_DIR}/Script/XJScriptSceneSerializationTests.cpp
)

target_link_libraries(
    XJScriptSceneSerializationTests
    PRIVATE
        Core
)

target_compile_features(
    XJScriptSceneSerializationTests
    PRIVATE
        cxx_std_20
)

add_test(
    NAME XJScriptSceneSerializationTests
    COMMAND
        $<TARGET_FILE:XJScriptSceneSerializationTests>
)

add_executable(
    XJEditorScriptAssetTests
    ${CMAKE_CURRENT_SOURCE_DIR}/Script/XJEditorScriptAssetTests.cpp
)

target_link_libraries(
    XJEditorScriptAssetTests
    PRIVATE
        Editor
        Core
)

target_compile_features(
    XJEditorScriptAssetTests
    PRIVATE
        cxx_std_20
)

add_test(
    NAME XJEditorScriptAssetTests
    COMMAND
        $<TARGET_FILE:XJEditorScriptAssetTests>
)

add_executable(
    XJScriptSystemTests
    ${CMAKE_CURRENT_SOURCE_DIR}/Script/XJScriptSystemTests.cpp
)

target_link_libraries(
    XJScriptSystemTests
    PRIVATE
        Core
)

target_compile_features(
    XJScriptSystemTests
    PRIVATE
        cxx_std_20
)

add_test(
    NAME XJScriptSystemTests
    COMMAND
        $<TARGET_FILE:XJScriptSystemTests>
)
