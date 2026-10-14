#include "Render/Material/XJMaterialPipelineRuntimeDescriptor.h"
#include "Render/Material/XJMaterialUploadState.h"
#include "Render/Resource/XJMaterial.h"
#include "Render/Resource/XJMaterialFactory.h"

#include <cstdlib>
#include <spdlog/spdlog.h>
#include <limits>
#include <memory>
#include <string_view>

namespace
{
    void Check(bool condition, std::string_view message)
    {
        if (!condition)
        {
            spdlog::error("FAILED: {}", message);
            std::exit(EXIT_FAILURE);
        }
    }

    class TestMaterial final : public XJ::XJMaterial
    {
        public:
            TestMaterial() = default;
    };

    class OtherTestMaterial final : public XJ::XJMaterial
    {
        public:
            OtherTestMaterial() = default;
    };

    void TestFactorySlotReuse()
    {
        auto* factory = XJ::XJMaterialFactory::GetInstance();

        auto a = factory->CreateMaterial<TestMaterial>();
        auto b = factory->CreateMaterial<TestMaterial>();
        auto c = factory->CreateMaterial<TestMaterial>();

        Check(b->GetIndex() == a->GetIndex() + 1,
              "material indices are consecutive");
        Check(c->GetIndex() == b->GetIndex() + 1,
              "third material index is consecutive");

        const uint32_t releasedIndex = b->GetIndex();
        const uint64_t releasedOwner = b->GetInstanceId();
        const uint32_t lastLiveIndex = c->GetIndex();
        b.reset();

        Check(factory->GetMaterialSize<TestMaterial>() == 2,
              "counting expired slots does not compact live indices");
        factory->ClearExpiredMaterials();
        Check(c->GetIndex() == lastLiveIndex,
              "cleanup preserves indices after a middle hole");

        auto replacement = factory->CreateMaterial<TestMaterial>();
        Check(replacement->GetIndex() == releasedIndex,
              "expired middle slot is reused");
        Check(replacement->GetInstanceId() != releasedOwner,
              "reused slot receives a new owner id");
        Check(factory->GetMaterialSize<TestMaterial>() == 3,
              "only live materials are counted");
        Check(replacement->GetIndex() != a->GetIndex() &&
              replacement->GetIndex() != c->GetIndex(),
              "replacement never aliases a live material slot");

        const uint32_t trailingIndex = c->GetIndex();
        c.reset();
        factory->ClearExpiredMaterials();

        auto trailingReplacement =
            factory->CreateMaterial<TestMaterial>();
        Check(trailingReplacement->GetIndex() == trailingIndex,
              "expired trailing slot is reused");

        auto other = factory->CreateMaterial<OtherTestMaterial>();
        Check(other->GetIndex() == 0,
              "different material types use independent index spaces");
    }

    void TestRevisionRouting()
    {
        auto material =
            XJ::XJMaterialFactory::GetInstance()
                ->CreateMaterial<TestMaterial>();

        uint64_t params = material->GetParameterRevision();
        uint64_t resources = material->GetResourceRevision();

        material->MarkParameterDirty();
        Check(material->GetParameterRevision() == params + 1,
              "parameter dirty increments parameter revision");
        Check(material->GetResourceRevision() == resources,
              "parameter dirty leaves resource revision unchanged");

        params = material->GetParameterRevision();
        material->MarkTextureDirty();
        Check(material->GetParameterRevision() == params,
              "texture dirty leaves parameter revision unchanged");
        Check(material->GetResourceRevision() == resources + 1,
              "texture dirty increments resource revision");

        params = material->GetParameterRevision();
        resources = material->GetResourceRevision();
        material->SetTextureView(7, {}, {});
        Check(material->GetParameterRevision() == params + 1,
              "texture assignment updates texture-derived parameters");
        Check(material->GetResourceRevision() == resources + 1,
              "texture assignment updates resource descriptors");

        params = material->GetParameterRevision();
        resources = material->GetResourceRevision();
        material->UpdateTextureViewUVRotation(7, 1.0f);
        Check(material->GetParameterRevision() == params + 1,
              "UV change updates parameters");
        Check(material->GetResourceRevision() == resources,
              "UV change does not update resource descriptors");

        params = material->GetParameterRevision();
        resources = material->GetResourceRevision();
        material->SetSamplerTextureView("albedo", {}, {});
        Check(material->GetParameterRevision() == params + 1,
              "sampler assignment updates texture parameters");
        Check(material->GetResourceRevision() == resources + 1,
              "sampler assignment updates descriptors");

        params = material->GetParameterRevision();
        resources = material->GetResourceRevision();
        material->UpdateTextureViewEnable(999, false);
        Check(material->GetParameterRevision() == params &&
              material->GetResourceRevision() == resources,
              "missing texture update changes no revision");

        material->SetTextureBindings({});
        Check(material->GetParameterRevision() == params &&
              material->GetResourceRevision() == resources + 1,
              "texture binding changes route only to resources");

        params = material->GetParameterRevision();
        resources = material->GetResourceRevision();
        material->UpdateTextureViewEnable(7, false);
        material->UpdateTextureViewUVTranslation(7, glm::vec2(0.5f));
        material->UpdateTextureViewUVScale(7, glm::vec2(2.0f));
        material->UpdateSamplerTextureViewEnable("albedo", false);
        Check(material->GetParameterRevision() == params + 4 &&
              material->GetResourceRevision() == resources,
              "texture enable and UV changes route only to parameters");

        params = material->GetParameterRevision();
        material->SetParameterLayout({});
        material->SetParameterBlock({});
        material->SetParameterBlocks({});
        Check(material->GetParameterRevision() == params + 3 &&
              material->GetResourceRevision() == resources,
              "parameter layout and blocks route only to parameters");

        params = material->GetParameterRevision();
        Check(!material->SetPrimaryUboMemberBytes("missing", &params, sizeof(params)),
              "missing UBO member write fails");
        Check(material->GetParameterRevision() == params &&
              material->GetResourceRevision() == resources,
              "failed writes do not advance revisions");
    }

    void TestFrameSlotUploadState()
    {
        XJ::XJMaterialUploadState state;
        state.Configure(8, 2);

        Check(state.GetSlotCount() == 16,
              "upload state contains every frame/material pair");

        auto requirements = state.Prepare(0, 3, 10, 4, 7);
        Check(requirements &&
              requirements->Parameters &&
              requirements->Resources,
              "new material requires both uploads");

        Check(state.CommitParameters(0, 3, 10, 4),
              "parameter upload commits");
        Check(!state.IsCurrent(0, 3, 10, 4, 7),
              "resource upload remains pending");

        requirements = state.Prepare(0, 3, 10, 4, 7);
        Check(requirements &&
              !requirements->Parameters &&
              requirements->Resources,
              "only resource upload remains pending");

        Check(state.CommitResources(0, 3, 10, 7),
              "resource upload commits");
        Check(state.IsCurrent(0, 3, 10, 4, 7),
              "frame slot becomes current");

        state.Configure(8, 2);
        Check(state.IsCurrent(0, 3, 10, 4, 7),
              "unchanged capacity preserves upload stamps");

        XJ::XJMaterialUploadState otherConsumer;
        otherConsumer.Configure(8, 2);
        requirements = otherConsumer.Prepare(0, 3, 10, 4, 7);
        Check(requirements && requirements->Parameters && requirements->Resources,
              "another render consumer has independent upload stamps");

        requirements = state.Prepare(1, 3, 10, 4, 7);
        Check(requirements &&
              requirements->Parameters &&
              requirements->Resources,
              "second frame slot remains independent");
        Check(state.CommitParameters(1, 3, 10, 4) &&
              state.CommitResources(1, 3, 10, 7),
              "second frame slot uploads independently");
        requirements = state.Prepare(0, 3, 10, 4, 8);
        Check(requirements && !requirements->Parameters && requirements->Resources,
              "resource revision invalidates only resources");
        requirements = state.Prepare(0, 3, 10, 4, 8);
        Check(requirements && requirements->Resources,
              "failed upload without commit remains pending");

        requirements = state.Prepare(0, 3, 10, 5, 7);
        Check(requirements &&
              requirements->Parameters &&
              !requirements->Resources,
              "parameter revision invalidates only parameters");

        requirements = state.Prepare(0, 3, 11, 1, 1);
        Check(requirements &&
              requirements->Parameters &&
              requirements->Resources,
              "new owner invalidates reused material slot");
        Check(!state.CommitParameters(0, 3, 10, 5),
              "stale owner cannot commit after slot reuse");

        state.Configure(16, 2);
        requirements = state.Prepare(0, 3, 11, 1, 1);
        Check(requirements &&
              requirements->Parameters &&
              requirements->Resources,
              "capacity change invalidates all stamps");

        Check(!state.Prepare(2, 0, 1, 1, 1),
              "out-of-range frame slot is rejected");
        Check(!state.Prepare(0, 16, 1, 1, 1),
              "out-of-range material slot is rejected");
        Check(!state.IsCurrent(0, 0, 0, 0, 0) &&
              !state.CommitParameters(0, 0, 0, 0),
              "uninitialized stamps are never treated as uploaded");
        requirements = state.Prepare(1, 3, 10, 4, 7);
        Check(requirements && requirements->Parameters && requirements->Resources,
              "growth also invalidates the second frame slot");
    }

    void TestDescriptorGrowthPlan()
    {
        using Descriptor =
            XJ::XJMaterialPipelineRuntimeDescriptor;

        auto plan = Descriptor::BuildAllocationPlan(0, 1, 2, 1);
        Check(plan && plan->MaterialCapacity == 256,
              "first pool uses capacity 256");
        Check(plan->DescriptorSetsPerLayout == 512,
              "descriptor count includes both frame slots");
        Check(plan->MaxSets == 1024,
              "pool contains parameter and resource sets");
        Check(plan->MaterialUboBufferCount == 512,
              "one UBO is allocated per descriptor slot");

        plan = Descriptor::BuildAllocationPlan(256, 257, 2, 2);
        Check(plan && plan->MaterialCapacity == 512,
              "capacity grows to next power of two");
        Check(plan->MaterialUboBufferCount == 2048,
              "UBO count includes frames and UBO layouts");

        plan = Descriptor::BuildAllocationPlan(512, 257, 2, 1);
        Check(plan && plan->MaterialCapacity == 512,
              "pool never shrinks");

        plan = Descriptor::BuildAllocationPlan(0, 2049, 2, 1);
        Check(plan && plan->MaterialCapacity == 4096,
              "pool grows to maximum capacity");

        Check(!Descriptor::BuildAllocationPlan(4096, 4097, 2, 1),
              "request above maximum is rejected");
        Check(!Descriptor::BuildAllocationPlan(0, 1, 0, 1),
              "zero frame slots are rejected");
        Check(!Descriptor::BuildAllocationPlan(4097, 1, 2, 1),
              "invalid existing capacity is rejected");
        Check(!Descriptor::BuildAllocationPlan(
                  0, 1, std::numeric_limits<uint32_t>::max(), 1),
              "descriptor count overflow is rejected");
        Check(!Descriptor::BuildAllocationPlan(
                  0, 1, 2, std::numeric_limits<uint32_t>::max()),
              "UBO count overflow is rejected");
    }
}

int main()
{
    TestFactorySlotReuse();
    TestRevisionRouting();
    TestFrameSlotUploadState();
    TestDescriptorGrowthPlan();

    spdlog::info("XJMaterial state tests passed.");
    return EXIT_SUCCESS;
}
