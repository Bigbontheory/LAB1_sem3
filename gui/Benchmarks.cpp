#include "BenchmarkData.hpp"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <locale>
#include <map>
#include <sstream>
#include <string>

namespace {
using Metrics = std::array<float, 4>;
using Trials = std::array<Metrics, 3>;
using Results = std::map<int, std::array<Trials, 2>>;

Results read_results() {
    Results results;
    std::istringstream csv(benchmark_data);
    std::string line;
    std::getline(csv, line);
    while (std::getline(csv, line)) {
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream row(line);
        row.imbue(std::locale::classic());
        std::string name;
        int size, trial;
        Metrics times;
        if (row >> name >> size >> trial >> times[0] >> times[1] >> times[2] >> times[3]
            && (name == "AVL" || name == "Set") && size > 0 && trial >= 1 && trial <= 3) {
            results[size][name == "Set"][trial - 1] = times;
        }
    }
    return results;
}

float median(const Trials& trials, int operation) {
    std::array values{trials[0][operation], trials[1][operation], trials[2][operation]};
    std::sort(values.begin(), values.end());
    return values[1];
}

float plotted_time(float milliseconds, int elements, bool per_element) {
    return per_element ? milliseconds * 1000.0f / elements : milliseconds;
}

void draw_plot(const Results& results, int operation, bool per_element) {
    constexpr ImU32 colors[]{IM_COL32(100, 195, 255, 255), IM_COL32(255, 186, 112, 255)};
    constexpr int tick_count = 4;
    constexpr float height = 200.0f;
    float width = ImGui::GetContentRegionAvail().x;
    if (width < 160.0f) return;

    float maximum = 0.0f;
    for (const auto& [size, containers] : results) {
        const float avl = plotted_time(median(containers[0], operation), size, per_element);
        const float set = plotted_time(median(containers[1], operation), size, per_element);
        maximum = std::max({maximum, avl, set});
    }
    const float upper_time = maximum > 0 ? maximum * 1.08f : 1.0f;
    const int upper_size = results.rbegin()->first;

    ImGui::TextUnformatted(per_element ? "Time per element (us)" : "Total time (ms)");
    ImGui::SameLine();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(colors[0]), "AVL");
    ImGui::SameLine();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(colors[1]), "Set");

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##benchmark_plot", {width, height});
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 top_left{origin.x + 58.0f, origin.y + 8.0f};
    const ImVec2 bottom_right{origin.x + width - 34.0f, origin.y + height - 30.0f};
    const float plot_width = bottom_right.x - top_left.x;
    const float plot_height = bottom_right.y - top_left.y;
    const ImU32 grid = ImGui::GetColorU32(ImGuiCol_Border);
    const ImU32 label = ImGui::GetColorU32(ImGuiCol_Text);
    draw->AddRectFilled(top_left, bottom_right, ImGui::GetColorU32(ImGuiCol_FrameBg));
    for (int tick = 0; tick <= tick_count; ++tick) {
        const float fraction = static_cast<float>(tick) / tick_count;
        const float y = bottom_right.y - fraction * plot_height;
        const float x = top_left.x + fraction * plot_width;
        draw->AddLine({top_left.x, y}, {bottom_right.x, y}, grid);
        draw->AddLine({x, top_left.y}, {x, bottom_right.y}, grid);

        char caption[32];
        std::snprintf(caption, sizeof caption, "%.3g", fraction * upper_time);
        const ImVec2 time_label = ImGui::CalcTextSize(caption);
        draw->AddText({top_left.x - 7.0f - time_label.x, y - time_label.y * 0.5f}, label, caption);

        const auto elements = static_cast<long long>(upper_size) * tick / tick_count;
        std::snprintf(caption, sizeof caption, "%lld", elements);
        draw->AddText({x - ImGui::CalcTextSize(caption).x * 0.5f, bottom_right.y + 5.0f}, label, caption);
    }

    auto hovered = results.end();
    float nearest = 10.0f;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool over_plot = ImGui::IsItemHovered() && mouse.y >= top_left.y && mouse.y <= bottom_right.y;
    ImVec2 previous[2]{};
    for (auto row = results.begin(); row != results.end(); ++row) {
        const float x = top_left.x + (static_cast<float>(row->first) / upper_size) * plot_width;
        const float distance = std::abs(x - mouse.x);
        if (over_plot && distance < nearest) {
            hovered = row;
            nearest = distance;
        }
        for (int container = 0; container < 2; ++container) {
            const float time = plotted_time(median(row->second[container], operation), row->first, per_element);
            const ImVec2 point{x, bottom_right.y - (time / upper_time) * plot_height};
            if (row != results.begin()) draw->AddLine(previous[container], point, colors[container], 1.5f);
            draw->AddCircleFilled(point, 3.5f, colors[container]);
            previous[container] = point;
        }
    }
    if (hovered != results.end()) {
        if (ImGui::BeginTooltip()) {
            ImGui::Text("%d elements", hovered->first);
            const char* unit = per_element ? "us / element" : "ms";
            ImGui::Text("AVL: %.3f %s",
                        plotted_time(median(hovered->second[0], operation), hovered->first, per_element), unit);
            ImGui::Text("Set: %.3f %s",
                        plotted_time(median(hovered->second[1], operation), hovered->first, per_element), unit);
            ImGui::EndTooltip();
        }
    }
    ImGui::TextUnformatted("Number of elements");
}
}

void draw_benchmarks() {
    static const Results results = read_results();
    static int operation = 0;
    static bool per_element = false;
    const char* operations[]{"Insertion", "Successful lookup", "Full traversal", "Removal"};
    ImGui::TextUnformatted("Saved Release measurements. Median of three runs.");
    ImGui::Combo("Operation", &operation, operations, IM_ARRAYSIZE(operations));
    if (results.empty()) {
        ImGui::TextUnformatted("No measurements available.");
        return;
    }

    ImGui::Checkbox("Time per element", &per_element);
    draw_plot(results, operation, per_element);

    if (ImGui::BeginTable("Measurements", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Elements");
        ImGui::TableSetupColumn(per_element ? "AVL (us / element)" : "AVL (ms)");
        ImGui::TableSetupColumn(per_element ? "Set (us / element)" : "Set (ms)");
        ImGui::TableHeadersRow();
        for (const auto& [size, containers] : results) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", size);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.3f", plotted_time(median(containers[0], operation), size, per_element));
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.3f", plotted_time(median(containers[1], operation), size, per_element));
        }
        ImGui::EndTable();
    }
}
