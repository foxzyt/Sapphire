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
    MenuItem,
    // Advanced & Layouts
    Grid,
    StackPanel,
    DockPanel,
    WrapPanel,
    ScrollView,
    Border,
    // Controls
    Image,
    ProgressBar,
    RadioBox,
    ToggleSwitch,
    ComboBox,
    ListBox,
    PasswordBox,
    Hyperlink,
    Expander,
    // Specialized
    DataGrid,
    Canvas,
    Tooltip,
    Popup,
    Window
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
    
    // New specific properties
    std::string src = ""; // For Image
    float progress = 0.0f; // For ProgressBar
    std::vector<std::string> options; // For ComboBox, ListBox
    std::string selectedOption = "";
    int selectedIndex = -1;
    bool isPassword = false; // For PasswordBox
    std::string href = ""; // For Hyperlink
    bool expanded = false; // For Expander
    
    // Grid/Canvas positioning specifics
    int row = 0;
    int column = 0;
    int rowSpan = 1;
    int columnSpan = 1;
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;

    // Transform / Display specifics
    float opacity = 1.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float rotation = 0.0f;

    std::vector<std::shared_ptr<UINode>> children;
    UINode* parent = nullptr;
    
    UINode(UINodeType type, const std::string& id) : type(type), id(id) {}
    virtual ~UINode() = default;
};

#endif
