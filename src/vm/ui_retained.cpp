#include "ui_node.h"
#include "vm.h"
#include <iostream>

extern VM* g_current_vm;

void UIButtonNode::render(sf::RenderWindow& window, UIStyle* activeStyle) {
    UIStyle s = activeStyle ? *activeStyle : g_current_vm->ui_state.defaultStyle;
    
    // Simplistic drawing for now, replicating the rounded rect logic
    sf::Color bgColor = hovered ? s.hoverColor : s.bgColor;
    
    sf::ConvexShape shape;
    shape.setPointCount(4);
    shape.setPoint(0, sf::Vector2f(x, y));
    shape.setPoint(1, sf::Vector2f(x + width, y));
    shape.setPoint(2, sf::Vector2f(x + width, y + height));
    shape.setPoint(3, sf::Vector2f(x, y + height));
    shape.setFillColor(bgColor);
    shape.setOutlineColor(s.accentColor);
    shape.setOutlineThickness(s.borderThickness);
    window.draw(shape);
    
    // We would draw text here using sapphire_render_text, but we need access to it.
}

void UITextNode::render(sf::RenderWindow& window, UIStyle* activeStyle) {
    // text render
}

void UIContainerNode::render(sf::RenderWindow& window, UIStyle* activeStyle) {
    // render background if any
    for (auto& child : children) {
        child->render(window, activeStyle);
    }
}

// Minimal Yoga-like Flexbox Calculation
void calculate_layout(std::shared_ptr<UINode> node, float startX, float startY) {
    node->x = startX;
    node->y = startY;
    
    if (node->type == UINodeType::Container) {
        float currentX = startX;
        float currentY = startY;
        
        for (auto& child : node->children) {
            // Very naive layout
            calculate_layout(child, currentX, currentY);
            if (node->direction == "row") {
                currentX += child->width + node->gap;
            } else {
                currentY += child->height + node->gap;
            }
        }
    }
}
