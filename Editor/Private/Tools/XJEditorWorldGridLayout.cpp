#include "Tools/XJEditorWorldGridLayout.h"

#include <algorithm>
#include <cmath>
#include <locale>
#include <sstream>

namespace XJ
{
    namespace
    {
        constexpr double kMaximumCoordinate = 1.0e6;//世界坐标最大值
        constexpr double kMaximumStep = 1.0e4;//世界网格最大步长

        bool IsFinite(const glm::dvec3& value)//检查向量是否有限
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }

        bool IsFinite(const glm::mat4& value)//检查矩阵是否有限
        {
            for (int column = 0; column < 4; ++column)
            {
                for (int row = 0; row < 4; ++row)
                {
                    if (!std::isfinite(value[column][row]))
                        return false;
                }
            }

            return true;
        }

        bool Project(const glm::mat4& vp, const glm::vec3& position, unsigned int width, unsigned int height, glm::vec2& pixels)//将世界坐标投影到屏幕像素坐标
        {
            const glm::vec4 clip = vp * glm::vec4(position, 1.0f);

            if (!std::isfinite(clip.w) || clip.w <= 1.0e-5f)
                return false;

            const glm::vec3 ndc = glm::vec3(clip) / clip.w;

            if (!std::isfinite(ndc.x) || !std::isfinite(ndc.y) || !std::isfinite(ndc.z))
            {
                return false;
            }

            // Vulkan 的深度范围为 [0, 1]。
            if (ndc.z < 0.0f || ndc.z > 1.0f)
                return false;

            pixels = {(ndc.x * 0.5f + 0.5f) * float(width), (ndc.y * 0.5f + 0.5f) * float(height)};

            return true;
        }

        bool IsLabelVisible(const glm::mat4& vp, const glm::vec3& position, unsigned int width, unsigned int height)//检查网格标签是否在屏幕内
        {
            glm::vec2 pixels;

            if (!Project(vp, position, width, height, pixels))
                return false;

            // 先过滤视口外的标签，剩余文字仍由 Vulkan 裁剪。
            return pixels.x >= 0.0f && pixels.y >= 0.0f && pixels.x <= float(width) && pixels.y <= float(height);
        }

        float CalculateTextHeight(const glm::mat4& vp, const glm::mat4& projection, const glm::vec3& position, unsigned int viewportHeight, float maximumHeight)//计算网格标签的文字高度
        {
            const glm::vec4 clip = vp * glm::vec4(position, 1.0f);
            const float projectionY = std::abs(projection[1][1]);

            if (clip.w <= 1.0e-5f || projectionY <= 1.0e-5f)
            {
                return 0.0f;
            }

            // 近似维持 12 像素高。
            const float worldHeight = 24.0f * clip.w / (projectionY * float(viewportHeight));
            return std::min(worldHeight, maximumHeight);
        }

        std::string FormatMeters(double value)//将米数值格式化为字符串
        {
            if (std::abs(value) < 0.5)
                value = 0.0;

            std::ostringstream stream;
            stream.imbue(std::locale::classic());
            stream.setf(std::ios::fixed, std::ios::floatfield);
            stream.precision(0);
            stream << value << 'm';

            return stream.str();
        }
    }

    double XJEditorWorldGridLayoutBuilder::ChooseStep(double requested)//返回不小于 requested 的 1/2/5 系列值
    {
        if (!std::isfinite(requested) || requested <= 1.0)
            return 1.0;

        requested = std::min(requested, kMaximumStep);
        const double power = std::pow(10.0, std::floor(std::log10(requested)));
        const double normalized = requested / power;
        double multiplier = 10.0;

        if (normalized <= 1.0)
            multiplier = 1.0;
        else if (normalized <= 2.0)
            multiplier = 2.0;
        else if (normalized <= 5.0)
            multiplier = 5.0;

        return std::min(multiplier * power, kMaximumStep);
    }

    XJEditorWorldGridLayout XJEditorWorldGridLayoutBuilder::Build(//构建世界网格布局
        const glm::mat4& view,//视口
        const glm::mat4& projection,//投射
        unsigned int width,//宽
        unsigned int height)//高
    {
        XJEditorWorldGridLayout result;

        if (width == 0 || height == 0 ||
            !IsFinite(view) || !IsFinite(projection))
        {
            return result;
        }

        const double determinant = glm::determinant(glm::dmat4(view));//计算视口矩阵的行列式
        if (!std::isfinite(determinant) ||
            std::abs(determinant) < 1.0e-12)
        {
            return result;
        }
        
        const glm::dmat4 inverseView = glm::inverse(glm::dmat4(view));//计算视口矩阵的逆矩阵
        const glm::dvec3 eye(inverseView[3]);//计算视口矩阵的眼睛位置
        const glm::dvec3 rawForward(-inverseView[2]);//计算视口矩阵的前向向量

        const double forwardLength = glm::length(rawForward);

        if (!IsFinite(eye) ||
            !IsFinite(rawForward) ||
            forwardLength < 1.0e-12)
        {
            return result;
        }

        if (std::abs(eye.x) > kMaximumCoordinate ||
            std::abs(eye.y) > kMaximumCoordinate ||
            std::abs(eye.z) > kMaximumCoordinate)
        {
            return result;
        }

        const glm::dvec3 forward = rawForward / forwardLength;

        // 默认以相机在地面的投影为中心。
        glm::dvec3 center(eye.x, 0.0, eye.z);

        // 尽量围绕相机正在观察的地面区域。
        // 接近水平时不追逐非常遥远的交点。
        if (std::abs(forward.y) > 1.0e-6)
        {
            const double distance = -eye.y / forward.y;

            const double maximumDistance = std::max(100.0, std::abs(eye.y) * 8.0);

            if (distance > 0.0 && distance <= maximumDistance)
            {
                center = eye + forward * distance;
                center.y = 0.0;
            }
        }

        if (!IsFinite(center))
            return result;

        // 每格期望约占相机到观察地面距离的 1/12。
        const double distanceToCenter = glm::length(eye - center);

        const double step = ChooseStep(
            std::max(
                std::abs(eye.y),
                distanceToCenter * 0.5) / 12.0);

        // 对齐到主网格，减少中心随相机移动的频繁变化。
        const double snap = step * 5.0;

        center.x = std::round(center.x / snap) * snap;
        center.z = std::round(center.z / snap) * snap;

        const double radius =  step * double(result.HalfLineCount);

        if (std::abs(center.x) + radius > kMaximumCoordinate || std::abs(center.z) + radius > kMaximumCoordinate)
        {
            return result;
        }
        // 计算网格标签。
        result.Center = glm::vec3(center);
        result.Step = float(step);
        result.Valid = true;

        const glm::mat4 vp = projection * view;

        const auto addLabel =
            [&](XJEditorGridLabelKind kind,
                const glm::vec3& position,
                std::string text,
                float maximumTextHeight,
                float tickHalfLength)
        {
            if (text.size() > 16 || !IsLabelVisible(vp, position, width, height))
            {
                return;
            }

            const float textHeight =
                CalculateTextHeight(
                    vp,
                    projection,
                    position,
                    height,
                    maximumTextHeight);

            if (!std::isfinite(textHeight) || textHeight <= 0.0f)
            {
                return;
            }

            XJEditorGridLabel label;
            label.Kind = kind;
            label.Position = position;
            label.Text = std::move(text);
            label.Height = textHeight;
            label.TickHalfLength = tickHalfLength;

            result.Labels.push_back(std::move(label));
        };

        const auto addAxisLabels = [&](bool xAxis)//添加轴标签
        {
            // 只标记当前网格覆盖到的那条主轴。
            const double perpendicularDistance = xAxis ? std::abs(center.z) : std::abs(center.x);

            if (perpendicularDistance > radius * 0.65)
                return;

            const double centerCoordinate = xAxis ? center.x : center.z;

            glm::vec3 sample(0.0f);

            if (xAxis)
                sample.x = float(centerCoordinate);
            else
                sample.z = float(centerCoordinate);

            const glm::vec3 direction = xAxis ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);

            glm::vec2 firstPixels;
            glm::vec2 secondPixels;

            if (!Project(vp, sample, width, height, firstPixels) ||
                !Project(vp, sample + direction * float(step), width, height, secondPixels))
            {
                return;
            }

            const double pixelsPerStep = glm::length(secondPixels - firstPixels);

            // 主轴几乎正对观察方向时，不挤成一团文字。
            if (pixelsPerStep < 0.5)
                return;

            // 预留较大的文字间隔。
            // 远距离的大数字也不容易相互覆盖。
            const double stride = ChooseStep(std::max(1.0, 120.0 / pixelsPerStep));
            const double labelStep = step * stride;
            const double firstIndex = std::round(centerCoordinate / labelStep);

            // 每条轴最多 17 个标签。
            for (int offset = -8; offset <= 8; ++offset)
            {
                const double coordinate = (firstIndex + double(offset)) * labelStep;

                if (std::abs(coordinate - centerCoordinate) > radius * 0.65)
                {
                    continue;
                }

                // 原点只由 X 轴显示一次。
                if (!xAxis && std::abs(coordinate) < 0.5)
                    continue;

                const glm::vec3 position = direction * float(coordinate);

                addLabel(
                    xAxis
                        ? XJEditorGridLabelKind::XAxis
                        : XJEditorGridLabelKind::ZAxis,
                    position,
                    FormatMeters(coordinate),
                    float(labelStep * 0.2),
                    float(step * 0.12));
            }
        };

        addAxisLabels(true);
        addAxisLabels(false);

        // 世界空间的格距说明，不是 ImGui 覆盖文字。
        // G 表示 Grid，每格距离。
        const glm::vec3 spacingPosition =
            result.Center +
            glm::vec3(2.0f * result.Step, 0.0f,
                      2.0f * result.Step);

        addLabel(
            XJEditorGridLabelKind::Spacing,
            spacingPosition,
            "G=" + FormatMeters(step),
            result.Step * 0.3f,
            0.0f);

        return result;
    }
}
/*
Center 是网格绘制区域的中心，不是新世界原点。
所有刻度仍然使用真实世界坐标。
网格移动时，线条仍落在世界格点上。
当前文字间隔是保守策略，不承诺任意透视角度下完全没有重叠；极端斜视角会主动减少或隐藏标签。
*/