#include "Script/Bytecode/XJScriptBytecodeVerifier.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

namespace
{
    void Check(bool condition, std::string_view message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    XJ::XJScriptBytecodeFunction VoidFunction()
    {
        XJ::XJScriptBytecodeFunction function;
        function.Name = "Run";
        function.ReturnType = XJ::XJScriptValueType::Void;
        function.Code.push_back({XJ::XJScriptOpCode::ReturnVoid, 0, 0, 1, 1});
        return function;
    }
}

int main()
{
    XJ::XJScriptBytecodeModule valid;
    valid.Constants.push_back(int64_t{1});
    valid.Functions.push_back(VoidFunction());
    valid.Functions[0].Code.insert(valid.Functions[0].Code.begin(), {
        XJ::XJScriptOpCode::PushConstant, 0, 0, 1, 1
    });
    valid.Functions[0].Code.insert(valid.Functions[0].Code.begin() + 1, {
        XJ::XJScriptOpCode::Pop, 0, 0, 1, 2
    });

    const auto verified = XJ::XJScriptBytecodeVerifier::Verify(valid);
    Check(verified.IsValid(), "valid bytecode verifies");
    Check(verified.FunctionMaxStackDepths[0] == 1, "maximum stack depth calculated");

    auto badConstant = valid;
    badConstant.Functions[0].Code[0].A = 5;
    Check(!XJ::XJScriptBytecodeVerifier::Verify(badConstant).IsValid(),
          "bad constant index rejected");

    XJ::XJScriptBytecodeModule underflow;
    underflow.Functions.push_back(VoidFunction());
    underflow.Functions[0].Code.insert(underflow.Functions[0].Code.begin(), {
        XJ::XJScriptOpCode::Pop, 0, 0, 1, 1
    });
    Check(!XJ::XJScriptBytecodeVerifier::Verify(underflow).IsValid(),
          "stack underflow rejected");

    XJ::XJScriptBytecodeModule merge;
    merge.Constants.push_back(true);
    merge.Constants.push_back(int64_t{1});
    auto mergeFunction = VoidFunction();
    mergeFunction.Code = {
        {XJ::XJScriptOpCode::PushConstant, 0, 0, 1, 1},
        {XJ::XJScriptOpCode::JumpIfFalse, 4, 0, 1, 2},
        {XJ::XJScriptOpCode::PushConstant, 1, 0, 1, 3},
        {XJ::XJScriptOpCode::Jump, 4, 0, 1, 4},
        {XJ::XJScriptOpCode::Nop, 0, 0, 1, 5},
        {XJ::XJScriptOpCode::ReturnVoid, 0, 0, 1, 6}
    };
    merge.Functions.push_back(std::move(mergeFunction));
    Check(!XJ::XJScriptBytecodeVerifier::Verify(merge).IsValid(),
          "different merge stack depths rejected");

    auto badReturn = valid;
    badReturn.Functions[0].Code.back() = {
        XJ::XJScriptOpCode::ReturnValue, 0, 0, 1, 3
    };
    Check(!XJ::XJScriptBytecodeVerifier::Verify(badReturn).IsValid(),
          "ReturnValue in void function rejected");

    auto badField = valid;
    badField.Fields.push_back({
        "bad", 0, XJ::XJScriptValueType::Int,
        XJ::XJScriptAccess::Private, std::string{"wrong"}
    });
    Check(!XJ::XJScriptBytecodeVerifier::Verify(badField).IsValid(),
          "field default type mismatch rejected");

    auto badLocalType = valid;
    badLocalType.Functions[0].LocalCount = 1;
    badLocalType.Functions[0].LocalTypes = {
        static_cast<XJ::XJScriptValueType>(99)
    };
    Check(!XJ::XJScriptBytecodeVerifier::Verify(badLocalType).IsValid(),
          "invalid local type enum rejected");

    auto nonFinite = valid;
    nonFinite.Constants.push_back(std::numeric_limits<double>::infinity());
    Check(!XJ::XJScriptBytecodeVerifier::Verify(nonFinite).IsValid(),
          "non-finite constant rejected");

    XJ::XJScriptBytecodeModule uninitializedLocal;
    auto localFunction = VoidFunction();
    localFunction.LocalTypes = {XJ::XJScriptValueType::Int};
    localFunction.LocalCount = 1;
    localFunction.Code = {
        {XJ::XJScriptOpCode::LoadLocal, 0, 0, 1, 1},
        {XJ::XJScriptOpCode::Pop, 0, 0, 1, 2},
        {XJ::XJScriptOpCode::ReturnVoid, 0, 0, 1, 3}
    };
    uninitializedLocal.Functions.push_back(std::move(localFunction));
    Check(!XJ::XJScriptBytecodeVerifier::Verify(uninitializedLocal).IsValid(),
          "read before local initialization rejected");

    std::cout << "XJScriptBytecodeVerifier tests passed\n";
    return EXIT_SUCCESS;
}
