#include "Tools/XJEditorWorldGridLayout.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <spdlog/spdlog.h>

#include <cmath>
#include <cstdlib>
#include <limits>
#include <string_view>

namespace
{
    void Check(bool value, std::string_view message)
    {
        if (!value)
        {
            spdlog::error("FAILED: {}", message);
            std::exit(EXIT_FAILURE);
        }
    }

    bool Near(double a, double b)
    {
        return std::abs(a - b) < 1.0e-6;
    }

    void TestStepSelection()
    {
        using Builder =
            XJ::XJEditorWorldGridLayoutBuilder;

        Check(Near(Builder::ChooseStep(0.1), 1.0),
              "Minimum step is one meter");

        Check(Near(Builder::ChooseStep(1.0), 1.0),
              "One meter");

        Check(Near(Builder::ChooseStep(1.1), 2.0),
              "Round up to two");

        Check(Near(Builder::ChooseStep(2.1), 5.0),
              "Round up to five");

        Check(Near(Builder::ChooseStep(5.1), 10.0),
              "Round up to ten");

        Check(Near(Builder::ChooseStep(21.0), 50.0),
              "Higher decade");

        Check(
            Near(
                Builder::ChooseStep(
                    std::numeric_limits<double>::quiet_NaN()),
                1.0),
            "Invalid requested step has safe fallback");
    }

    void TestCameraDistanceAndTranslation()
    {
        using Builder = XJ::XJEditorWorldGridLayoutBuilder;
        auto projection = glm::perspectiveRH_ZO(
            glm::radians(60.0f), 1280.0f / 720.0f, 0.1f, 10000.0f);
        projection[1][1] *= -1.0f;

        const float heights[] = {10.0f, 20.0f, 40.0f, 100.0f};
        const float steps[] = {1.0f, 2.0f, 5.0f, 10.0f};
        for (int i = 0; i < 4; ++i)
        {
            const auto view = glm::lookAtRH(
                glm::vec3(0.0f, heights[i], heights[i]),
                glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            const auto layout = Builder::Build(view, projection, 1280, 720);
            Check(layout.Valid && Near(layout.Step, steps[i]),
                "Camera distance grows spacing through 1/2/5 series");
        }

        const auto translatedView = glm::lookAtRH(
            glm::vec3(100.0f, 10.0f, 10.0f),
            glm::vec3(100.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f));
        const auto translated = Builder::Build(translatedView, projection, 1280, 720);
        Check(translated.Valid && Near(translated.Center.x, 100.0) && Near(translated.Step, 1.0),
            "Horizontal camera movement moves coverage while retaining spacing");
        bool foundWorldCoordinate = false;
        for (const auto& label : translated.Labels)
        {
            if (label.Kind != XJ::XJEditorGridLabelKind::XAxis)
                continue;
            Check(label.Text == std::to_string(static_cast<int>(std::round(label.Position.x))) + "m",
                "Translated labels retain absolute world coordinates");
            Check(label.TickHalfLength > 0.0f, "Axis labels include short ticks");
            foundWorldCoordinate = foundWorldCoordinate || std::abs(label.Position.x) > 50.0f;
        }
        Check(foundWorldCoordinate, "Translated view displays world coordinates instead of recentered zero");
    }

    void TestLayout()
    {
        using Builder =
            XJ::XJEditorWorldGridLayoutBuilder;

        const glm::mat4 view =
            glm::lookAtRH(
                glm::vec3(8.0f, 10.0f, 8.0f),
                glm::vec3(0.0f),
                glm::vec3(0.0f, 1.0f, 0.0f));

        glm::mat4 projection =
            glm::perspectiveRH_ZO(
                glm::radians(60.0f),
                1280.0f / 720.0f,
                0.1f,
                1000.0f);

        projection[1][1] *= -1.0f;

        const auto layout =
            Builder::Build(view, projection, 1280, 720);

        Check(layout.Valid, "Normal camera produces layout");

        Check(
            layout.Center.y == 0.0f,
            "Grid stays on world ground");

        Check(
            layout.Step >= 1.0f,
            "Grid step respects minimum");

        Check(
            layout.Labels.size() <= 35,
            "Label count is bounded");

        Check(
            !layout.Labels.empty(),
            "Normal view produces labels");

        for (const auto& label : layout.Labels)
        {
            Check(
                label.Position.y == 0.0f,
                "Label anchors stay on ground");

            Check(
                label.Height > 0.0f &&
                std::isfinite(label.Height),
                "Label height is valid");

            Check(
                label.Text.size() <= 16,
                "Text fits packed character capacity");
        }

        Check(
            !Builder::Build(
                view, projection, 0, 720).Valid,
            "Zero width is rejected");

        Check(
            !Builder::Build(
                glm::mat4(0.0f),
                projection,
                1280,
                720).Valid,
            "Singular view is rejected");

        glm::mat4 invalidView = view;

        invalidView[0][0] =
            std::numeric_limits<float>::quiet_NaN();

        Check(
            !Builder::Build(
                invalidView,
                projection,
                1280,
                720).Valid,
            "Non-finite view is rejected");
    }
}

int main()
{
    TestStepSelection();
    TestCameraDistanceAndTranslation();
    TestLayout();

    spdlog::info(
        "XJEditorWorldGridLayoutTests passed.");

    return EXIT_SUCCESS;
}
