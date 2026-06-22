#ifndef SAPPHIRE_UI_NODE_H
#define SAPPHIRE_UI_NODE_H

#include <string>
#include <vector>
#include <memory>

enum class UINodeType {
    Container,
    Button,
    Text,
    Checkbox,
    Slider,
    Input,
    Separator,
    Display,
    Menu,
    MenuItem
};

class UINode {
public:
    UINodeType type;
    std::string id;
    
    // Layout and Bounds
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    
    // State
    bool hovered = false;
    bool clicked = false;
    
    // Flexbox/Grid Properties
    std::string direction = "column";
    std::string justify = "flex-start";
    std::string align = "stretch";
    float gap = 10.0f;
    
    std::string label = "";
    std::string fontAlias = "";
    unsigned int fontSize = 0;
    std::string styleName = "";
    float value = 0.0f;
    float min = 0.0f;
    float max = 100.0f;
    bool checked = false;
    float thickness = 1.0f;
    float margin = 10.0f;
    std::string customColor = "";
    bool shadow = false;
    
    std::vector<std::shared_ptr<UINode>> children;
    UINode* parent = nullptr;
    
    UINode(UINodeType type, const std::string& id) : type(type), id(id) {}
    virtual ~UINode() = default;
};

#endif
