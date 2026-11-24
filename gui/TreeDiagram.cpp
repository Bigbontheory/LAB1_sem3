#include "TreeDiagram.hpp"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <cstdio>

namespace {
using NodeView = AVLTree<int, int>::NodeView;

constexpr int visible_levels = 8;
constexpr float column_width = 104.0f;
constexpr float row_height = 76.0f;
constexpr float node_width = 94.0f;
constexpr float node_height = 42.0f;
constexpr float margin = 10.0f;

struct DiagramNode {
    int key;
    int value;
    int level;
    int left_child;
    int right_child;
};

struct Layout {
    std::array<DiagramNode, (1 << visible_levels) - 1> nodes{};
    int count = 0;
    int levels = 0;
    bool truncated = false;

    int add(NodeView node, int level) {
        if (!node) return -1;
        if (level == visible_levels) {
            truncated = true;
            return -1;
        }

        const int left = add(node.left(), level + 1);
        const int index = count++;
        const int right = add(node.right(), level + 1);
        nodes[index] = {node.key(), node.value(), level, left, right};
        levels = std::max(levels, level + 1);
        return index;
    }
};

ImVec2 center(const Layout& layout, ImVec2 origin, int index) {
    return {origin.x + (index + 0.5f) * column_width,
            origin.y + margin + layout.nodes[index].level * row_height + node_height / 2.0f};
}

void draw_edges(const Layout& layout, ImVec2 origin, ImDrawList& draw) {
    const auto color = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    for (int i = 0; i < layout.count; ++i) {
        const auto parent = center(layout, origin, i);
        const auto& node = layout.nodes[i];
        for (int child : {node.left_child, node.right_child}) {
            if (child < 0) continue;
            const auto next = center(layout, origin, child);
            draw.AddLine({parent.x, parent.y + node_height / 2.0f},
                         {next.x, next.y - node_height / 2.0f}, color, 1.5f);
        }
    }
}

void draw_nodes(const Layout& layout, ImVec2 origin, int root_index, ImDrawList& draw) {
    const auto text = ImGui::GetColorU32(ImGuiCol_Text);
    const auto border = ImGui::GetColorU32(ImGuiCol_SliderGrabActive);
    for (int i = 0; i < layout.count; ++i) {
        const auto& node = layout.nodes[i];
        const auto pos = center(layout, origin, i);
        const ImVec2 top{pos.x - node_width / 2.0f, pos.y - node_height / 2.0f};
        const ImVec2 bottom{pos.x + node_width / 2.0f, pos.y + node_height / 2.0f};
        const auto fill = ImGui::GetColorU32(i == root_index ? ImGuiCol_HeaderActive : ImGuiCol_FrameBg);
        draw.AddRectFilled(top, bottom, fill, 5.0f);
        draw.AddRect(top, bottom, border, 5.0f);

        char key[16], value[16];
        std::snprintf(key, sizeof key, "%d", node.key);
        std::snprintf(value, sizeof value, "%d", node.value);
        const float line_height = ImGui::GetTextLineHeight();
        draw.AddText({pos.x - ImGui::CalcTextSize(key).x / 2.0f, pos.y - line_height}, text, key);
        draw.AddText({pos.x - ImGui::CalcTextSize(value).x / 2.0f, pos.y}, text, value);
    }
}
}

void draw_tree_diagram(const AVLTree<int, int>& tree) {
    if (!ImGui::CollapsingHeader("Tree structure", ImGuiTreeNodeFlags_DefaultOpen)) return;
    if (tree.size() == 0) {
        ImGui::TextDisabled("Empty tree");
        return;
    }

    Layout layout;
    const int root = layout.add(tree.root_view(), 0);
    ImGui::TextDisabled("Key above value");
    if (layout.truncated) {
        ImGui::TextDisabled("Showing the first %d levels", visible_levels);
    }

    const float width = layout.count * column_width;
    const float height = layout.levels * row_height - (row_height - node_height) + 2.0f * margin;
    const float viewport_height = std::min(height + ImGui::GetStyle().ScrollbarSize, 320.0f);
    if (ImGui::BeginChild("##tree_diagram", {0.0f, viewport_height}, ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        const auto start = ImGui::GetCursorScreenPos();
        const float left_padding = std::max(0.0f, (ImGui::GetContentRegionAvail().x - width) * 0.5f);
        const ImVec2 origin{start.x + left_padding, start.y};
        ImGui::Dummy({width + left_padding, height});
        auto& draw = *ImGui::GetWindowDrawList();
        draw_edges(layout, origin, draw);
        draw_nodes(layout, origin, root, draw);
    }
    ImGui::EndChild();
}
