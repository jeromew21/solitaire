#include "layout.h"

CanvasLayer::CanvasLayer() {}

void CanvasLayer::add_root_node(LayoutNode *node) {
    root_node = node;
    root_node->node_id = frameflow::add_generic(&layout_system, frameflow::NullNode);
    root_node->layout_system = &layout_system;
}

void CanvasLayer::compute_layout() { frameflow::compute_layout(&layout_system, root_node->node_id); }

void LayoutNode::add_child(LayoutNode *child) {
    children.push_back(child);
    child->layout_system = layout_system;
    child->node_id = child->init(this);
}

void LayoutNode::add_child_with_margin(LayoutNode *child, float x) {
    Margin *margin_node = new Margin(x, x, x, x);
    add_child(margin_node);
    margin_node->add_child(child);
    margin_node->anchors_fill();
}

frameflow::Rect LayoutNode::get_rect() { return frameflow::get_node(layout_system, node_id)->bounds; }

Generic::Generic() {}

HBox::HBox() {}

VBox::VBox() {}

HFlow::HFlow() {}

VFlow::VFlow() {}

Margin::Margin(float top, float right, float bottom, float left) { margin_data = {top, right, bottom, left}; }

Center::Center() {}

frameflow::NodeId Generic::init(LayoutNode *parent) { return frameflow::add_generic(layout_system, parent->node_id); }

frameflow::NodeId HFlow::init(LayoutNode *parent) {
    frameflow::FlowData flow_data{frameflow::Direction::Horizontal, frameflow::Align::Start};
    return frameflow::add_flow(layout_system, parent->node_id, flow_data);
}

frameflow::NodeId VFlow::init(LayoutNode *parent) {
    frameflow::FlowData flow_data{frameflow::Direction::Vertical, frameflow::Align::Start};
    return frameflow::add_flow(layout_system, parent->node_id, flow_data);
}

frameflow::NodeId HBox::init(LayoutNode *parent) {
    frameflow::BoxData box_data{frameflow::Direction::Horizontal, frameflow::Align::Start};
    return frameflow::add_box(layout_system, parent->node_id, box_data);
}

frameflow::NodeId VBox::init(LayoutNode *parent) {
    frameflow::BoxData box_data{frameflow::Direction::Vertical, frameflow::Align::Start};
    return frameflow::add_box(layout_system, parent->node_id, box_data);
}

frameflow::NodeId Margin::init(LayoutNode *parent) {
    return frameflow::add_margin(layout_system, parent->node_id, margin_data);
}

frameflow::NodeId Center::init(LayoutNode *parent) { return frameflow::add_center(layout_system, parent->node_id); }
