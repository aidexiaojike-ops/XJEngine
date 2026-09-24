#include "ECS/Component/XJScriptComponent.h"

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
}

int main()
{
    XJ::XJScriptComponent component;
    const XJ::XJAssetRef scriptA{1001, XJ::XJAssetType::Script};
    const XJ::XJAssetRef scriptB{1002, XJ::XJAssetType::Script};

    auto* first = component.AddSlotWithId(XJ::XJUUID{2001}, scriptA, true);
    Check(first != nullptr && component.GetSlotCount() == 1, "explicit slot added");
    Check(!component.AddSlotWithId(XJ::XJUUID{2001}, scriptB), "duplicate slot ID rejected");
    Check(!component.AddSlot({1003, XJ::XJAssetType::Material}), "non-script asset rejected");

    auto* second = component.AddSlot(scriptB, false);
    Check(second != nullptr && component.GetSlotCount() == 2, "generated slot added");
    const XJ::XJUUID secondId = second->SlotId;
    Check(component.SetSlotEnabled(secondId, true), "slot enabled");

    Check(component.SetFieldOverride(XJ::XJUUID{2001}, 3001, XJ::XJScriptValue{45.0}),
          "field override added");
    const auto* overrideValue = component.GetFieldOverride(XJ::XJUUID{2001}, 3001);
    Check(overrideValue && std::get<double>(*overrideValue) == 45.0,
          "field override read");
    Check(!component.SetFieldOverride(XJ::XJUUID{2001}, 0, XJ::XJScriptValue{1.0}),
          "zero FieldId rejected");
    Check(!component.SetFieldOverride(
              XJ::XJUUID{2001}, 3002,
              XJ::XJScriptValue{std::numeric_limits<double>::infinity()}),
          "non-finite override rejected");
    Check(component.ClearFieldOverride(XJ::XJUUID{2001}, 3001), "override cleared");

    Check(component.SetSlotScript(XJ::XJUUID{2001}, scriptB), "slot script replaced");
    Check(component.GetFieldOverride(XJ::XJUUID{2001}, 3001) == nullptr,
          "changing script clears overrides");
    Check(component.RemoveSlot(secondId) && component.GetSlotCount() == 1,
          "slot removed");

    std::cout << "XJScriptComponent tests passed\n";
    return EXIT_SUCCESS;
}
