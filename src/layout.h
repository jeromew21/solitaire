#pragma once
#include <frameflow/layout.hpp>

struct LayoutNode;

struct CanvasLayer {
    CanvasLayer();

    frameflow::System layout_system;
    LayoutNode *root_node;

    void compute_layout();

    void add_root_node(LayoutNode *node);
};

struct LayoutNode {
    LayoutNode() = default;
    ~LayoutNode() = default;

    frameflow::NodeId node_id;

    frameflow::System *layout_system;

    std::vector<LayoutNode *> children;

    void add_child(LayoutNode *child);
    void add_child_with_margin(LayoutNode *child, float x);

    frameflow::Rect get_bounding_rect();

    LayoutNode *anchors_fill() {
        frameflow::Node *node = frameflow::get_node(layout_system, node_id);
        node->anchors = {0.0f, 0.0f, 1.f, 1.f};
        return this;
    }

    LayoutNode *anchors(float left, float top, float right, float bottom) {
        frameflow::Node *node = frameflow::get_node(layout_system, node_id);
        node->anchors = {left, top, right, bottom};
        return this;
    }

    LayoutNode *expand(float x, float y) {
        frameflow::Node *node = frameflow::get_node(layout_system, node_id);
        node->expand = {x, y};
        return this;
    }

    LayoutNode *minimum_size(float width, float height) {
        frameflow::Node *node = frameflow::get_node(layout_system, node_id);
        node->minimum_size = {width, height};
        return this;
    }

    LayoutNode *bounds(float x, float y, float width, float height) {
        frameflow::Node *node = frameflow::get_node(layout_system, node_id);
        node->bounds = {{x, y}, {width, height}};
        return this;
    }

  private:
    virtual frameflow::NodeId init(LayoutNode *parent) = 0;
};

struct Generic : public LayoutNode {
    Generic();

  private:
    frameflow::NodeId init(LayoutNode *parent) override;
};

struct HBox : public LayoutNode {
    HBox();

  private:
    frameflow::NodeId init(LayoutNode *parent) override;
};

struct VBox : public LayoutNode {
    VBox();

  private:
    frameflow::NodeId init(LayoutNode *parent) override;
};

struct Margin : public LayoutNode {
    Margin(float top, float right, float bottom, float left);

    frameflow::MarginData margin_data;

  private:
    frameflow::NodeId init(LayoutNode *parent) override;
};

struct Center : public LayoutNode {
    Center();

  private:
    frameflow::NodeId init(LayoutNode *parent) override;
};

struct HFlow : public LayoutNode {
    HFlow();

  private:
    frameflow::NodeId init(LayoutNode *parent) override;
};

struct VFlow : public LayoutNode {
    VFlow();

  private:
    frameflow::NodeId init(LayoutNode *parent) override;
};
