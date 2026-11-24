#include "AVLTree/AVLTreeIO.hpp"
#include "LAB2_sem2/DynamicArray.hpp"
#include "Set/SetIO.hpp"
#include "TreeDiagram.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "misc/cpp/imgui_stdlib.h"
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
using Tree = AVLTree<int, int>;
using Numbers = Set<int>;

struct Remainder {
    int divisor;
    int operator()(int value) const { return value % divisor; }
};

struct AtLeast {
    int threshold;
    bool operator()(int value) const { return value >= threshold; }
    bool operator()(int, int value) const { return (*this)(value); }
};

struct Sum {
    std::int64_t operator()(std::int64_t total, int value) const { return total + value; }
    std::int64_t operator()(std::int64_t total, int, int value) const { return total + value; }
};

std::string display(const Tree& tree) { return avl_io::to_string(tree); }
std::string display(const Numbers& set) { return set_io::to_string(set); }
const char* yes_no(bool value) { return value ? "Yes" : "No"; }

struct AppendNode {
    std::string& output;

    void operator()(int key, int value) const {
        output += std::to_string(key) + " -> " + std::to_string(value) + "\n";
    }
};

struct Generator {
    int count = 10;
    int low = 0;
    int high = 99;
    int seed = 42;

    template <typename Container>
    Container generate() const {
        const auto width = std::int64_t(high) - low + 1;
        if (count < 0 || high < low || count > width) {
            throw std::invalid_argument("Enter a valid range with enough distinct values.");
        }

        Container result;
        if (count == 0) return result;
        std::mt19937 rng(seed);
        std::unordered_set<int> chosen;
        DynamicArray<int> keys(count);
        for (int i = 0; i < count; ++i) {
            const int upper = high - (count - 1 - i);
            int key = std::uniform_int_distribution<int>(low, upper)(rng);
            if (!chosen.insert(key).second) {
                key = upper;
                chosen.insert(key);
            }
            keys[i] = key;
        }
        std::shuffle(keys.begin(), keys.end(), rng);
        for (int key : keys) {
            if constexpr (std::is_same_v<Container, Tree>) {
                result.insert(key, std::uniform_int_distribution<int>(low, high)(rng));
            } else {
                result.insert(key);
            }
        }
        return result;
    }
};

template <typename Container>
struct Workspace {
    Container data;
    std::string contents = display(data);
    std::string result;
    std::string error;
    std::string serialized;
    Generator generator;
    int key = 0;
    int value = 0;
    int operand = 2;
    int transform = 0;
    std::optional<std::size_t> other_index;
    std::string other_text = std::is_same_v<Container, Tree> ? "" : "{}";
    std::string format{avl_io::default_format};
    std::string traversal_order = "LNR";
    int set_operation = 0;
};

template <typename Container>
struct WorkspaceList {
    const char* kind;
    std::vector<Workspace<Container>> entries{1};
    std::size_t active = 0;

    std::string name(std::size_t index) const {
        return std::string(kind) + " " + std::to_string(index + 1);
    }
};

template <typename Container>
const Container* select_other(const WorkspaceList<Container>& list, Workspace<Container>& workspace,
                              const char* label, const char* text_label) {
    const auto preview = workspace.other_index ? list.name(*workspace.other_index) : "Enter as text";
    if (ImGui::BeginCombo(label, preview.c_str())) {
        if (ImGui::Selectable("Enter as text", !workspace.other_index)) workspace.other_index.reset();
        for (std::size_t i = 0; i < list.entries.size(); ++i) {
            if (i == list.active) continue;
            if (ImGui::Selectable(list.name(i).c_str(), workspace.other_index == i)) {
                workspace.other_index = i;
            }
        }
        ImGui::EndCombo();
    }
    if (workspace.other_index) return &list.entries[*workspace.other_index].data;
    ImGui::InputTextMultiline(text_label, &workspace.other_text, {-1, 70});
    return nullptr;
}

template <typename Container>
void edit_inputs(Workspace<Container>& workspace) {
    auto& data = workspace.data;
    ImGui::InputInt("Key or element", &workspace.key);
    if constexpr (std::is_same_v<Container, Tree>) {
        ImGui::InputInt("Value", &workspace.value);
    }
    bool changed = false;
    if (ImGui::Button("Insert")) {
        if constexpr (std::is_same_v<Container, Tree>) data.insert(workspace.key, workspace.value);
        else data.insert(workspace.key);
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Find")) {
        if constexpr (std::is_same_v<Container, Tree>) {
            workspace.result = data.contains(workspace.key)
                ? std::to_string(data.get(workspace.key)) : "Key not found.";
        } else {
            workspace.result = yes_no(data.contains(workspace.key));
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove")) {
        data.erase(workspace.key);
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        data = Container{};
        changed = true;
    }
    if (ImGui::CollapsingHeader("Generate data")) {
        ImGui::InputInt("Count", &workspace.generator.count);
        ImGui::InputInt("Minimum", &workspace.generator.low);
        ImGui::InputInt("Maximum", &workspace.generator.high);
        ImGui::InputInt("Seed", &workspace.generator.seed);
        if (ImGui::Button("Generate and replace")) {
            data = workspace.generator.template generate<Container>();
            changed = true;
        }
    }
    if (changed) workspace.contents = display(data);
}

template <typename Container>
void transform(Workspace<Container>& workspace) {
    const char* choices[]{"Map (remainder)", "Filter (lower bound)", "Reduce (sum)"};
    ImGui::Combo("Transformation", &workspace.transform, choices, IM_ARRAYSIZE(choices));
    if (workspace.transform != 2) {
        ImGui::InputInt(workspace.transform == 0 ? "Divisor" : "Lower bound", &workspace.operand);
    }
    if (!ImGui::Button("Run transformation")) return;

    const auto& data = workspace.data;
    switch (workspace.transform) {
        case 0:
            if (workspace.operand <= 0) throw std::invalid_argument("Enter a positive divisor.");
            workspace.result = display(data.map(Remainder{workspace.operand}));
            break;
        case 1:
            workspace.result = display(data.where(AtLeast{workspace.operand}));
            break;
        case 2:
            workspace.result = std::to_string(data.reduce(std::int64_t{0}, Sum{}));
            break;
    }
}

template <typename Container>
void output(Workspace<Container>& workspace) {
    ImGui::Text("Elements: %d", workspace.data.size());
    if constexpr (std::is_same_v<Container, Tree>) {
        draw_tree_diagram(workspace.data);
    }
    ImGui::TextUnformatted("Current contents");
    ImGui::InputTextMultiline("##contents", &workspace.contents,
                              {-1, 90}, ImGuiInputTextFlags_ReadOnly);
    if (!workspace.result.empty()) {
        ImGui::TextUnformatted("Result");
        ImGui::InputTextMultiline("##result", &workspace.result,
                                  {-1, 110}, ImGuiInputTextFlags_ReadOnly);
    }
}

void tree_panel(Workspace<Tree>& workspace, const WorkspaceList<Tree>& list) {
    auto& format = workspace.format;

    if (ImGui::CollapsingHeader("Traversal and subtrees")) {
        ImGui::InputTextWithHint("Order", "e.g. LNR", &workspace.traversal_order);
        if (ImGui::Button("Traverse")) {
            std::string result;
            workspace.data.traverse(workspace.traversal_order, AppendNode{result});
            workspace.result = result.empty() ? "Empty tree" : std::move(result);
            workspace.error.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Extract subtree at key")) {
            workspace.result = display(workspace.data.extract_subtree(workspace.key));
        }
    }
    if (ImGui::CollapsingHeader("Serialization and second tree")) {
        ImGui::InputText("Format", &format);
        ImGui::InputTextMultiline("Tree text", &workspace.serialized, {-1, 70});
        if (ImGui::Button("Export tree")) workspace.serialized = avl_io::to_string(workspace.data, format);
        ImGui::SameLine();
        if (ImGui::Button("Import tree")) {
            workspace.data = avl_io::from_string<int, int>(workspace.serialized, format);
            workspace.contents = display(workspace.data);
        }
        const auto* other = select_other(list, workspace, "Second tree", "Second tree text");
        const bool merge = ImGui::Button("Merge");
        ImGui::SameLine();
        const bool contains = ImGui::Button("Contains subtree");
        if (merge || contains) {
            Tree entered;
            if (!other) {
                entered = avl_io::from_string<int, int>(workspace.other_text, format);
                other = &entered;
            }
            workspace.result = merge ? display(workspace.data.merge(*other))
                                     : yes_no(workspace.data.contains_subtree(*other));
        }
    }
}

void set_panel(Workspace<Numbers>& workspace, const WorkspaceList<Numbers>& list) {
    auto& operation = workspace.set_operation;
    const char* choices[]{"Union", "Intersection", "A minus B", "Is A a subset of B?", "Are A and B equal?"};
    if (ImGui::CollapsingHeader("Set operations")) {
        const auto* other = select_other(list, workspace, "Set B", "Set B text");
        ImGui::Combo("Operation", &operation, choices, IM_ARRAYSIZE(choices));
        if (ImGui::Button("Run set operation")) {
            Numbers entered;
            if (!other) {
                entered = set_io::from_string<int>(workspace.other_text);
                other = &entered;
            }
            const auto& data = workspace.data;
            switch (operation) {
                case 0: workspace.result = display(data.set_union(*other)); break;
                case 1: workspace.result = display(data.set_intersection(*other)); break;
                case 2: workspace.result = display(data.set_difference(*other)); break;
                case 3: workspace.result = yes_no(data.is_subset_of(*other)); break;
                case 4: workspace.result = yes_no(data == *other); break;
            }
        }
    }
    if (ImGui::CollapsingHeader("Serialization")) {
        ImGui::InputTextMultiline("Set text", &workspace.serialized, {-1, 70});
        if (ImGui::Button("Export set")) workspace.serialized = workspace.contents;
        ImGui::SameLine();
        if (ImGui::Button("Import set")) {
            workspace.data = set_io::from_string<int>(workspace.serialized);
            workspace.contents = display(workspace.data);
        }
    }
}

template <typename Container>
void panel(WorkspaceList<Container>& list) {
    if (ImGui::BeginCombo("##current", list.name(list.active).c_str())) {
        for (std::size_t i = 0; i < list.entries.size(); ++i) {
            if (ImGui::Selectable(list.name(i).c_str(), i == list.active)) list.active = i;
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button((std::string("New ") + list.kind).c_str())) {
        list.entries.emplace_back();
        list.active = list.entries.size() - 1;
    }
    ImGui::Separator();

    auto& workspace = list.entries[list.active];
    ImGui::PushID(static_cast<int>(list.active));
    try {
        edit_inputs(workspace);
        ImGui::Separator();
        transform(workspace);
        if constexpr (std::is_same_v<Container, Tree>) tree_panel(workspace, list);
        else set_panel(workspace, list);
    } catch (const std::exception& error) {
        workspace.error = error.what();
    }
    if (!workspace.error.empty()) {
        ImGui::TextColored({1.0f, 0.45f, 0.40f, 1.0f}, "%s", workspace.error.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Dismiss")) workspace.error.clear();
    }
    ImGui::Separator();
    output(workspace);
    ImGui::PopID();
}
}

void draw_benchmarks();

int main() {
    if (!glfwInit()) {
        std::cerr << "Could not initialize GLFW.\n";
        return 1;
    }
    GLFWwindow* window = glfwCreateWindow(900, 720, "AVL Tree and Set", nullptr, nullptr);
    if (!window) {
        std::cerr << "Could not create the application window.\n";
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    const bool platform_ready = ImGui_ImplGlfw_InitForOpenGL(window, true);
    const bool renderer_ready = platform_ready && ImGui_ImplOpenGL3_Init("#version 130");
    if (renderer_ready) {
        WorkspaceList<Tree> trees{"Tree"};
        WorkspaceList<Numbers> sets{"Set"};
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            const auto* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);
            if (ImGui::Begin("Laboratory", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)) {
                if (ImGui::BeginTabBar("Containers")) {
                    if (ImGui::BeginTabItem("AVL Tree")) {
                        panel(trees);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Set")) {
                        panel(sets);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Benchmarks")) {
                        draw_benchmarks();
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
            }
            ImGui::End();
            ImGui::Render();

            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            glViewport(0, 0, width, height);
            glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
        }
        ImGui_ImplOpenGL3_Shutdown();
    } else {
        std::cerr << "Could not initialize Dear ImGui.\n";
    }
    if (platform_ready) ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return renderer_ready ? 0 : 1;
}
