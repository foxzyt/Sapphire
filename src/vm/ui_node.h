#ifndef SAPPHIRE_UI_NODE_H
#define SAPPHIRE_UI_NODE_H

#include <string>
#include <vector>
#include <memory>
#include <SFML/Graphics.hpp>

// Forward declaration of UIStyle
struct UIStyle;

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
    Window,
    // === NEW in v1.0.9 ===
    Card,       // Container elevado com sombra e gradiente embutidos
    Badge,      // Bolinha numérica (ex: 3 notificações)
    Tag,        // Chip / etiqueta inline colorida
    Stepper,    // Indicador de progresso em etapas
    Spinner     // Arco giratório de carregamento
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
    std::string src = "";       // For Image
    float progress = 0.0f;     // For ProgressBar
    std::vector<std::string> options; // For ComboBox, ListBox
    std::string selectedOption = "";
    int selectedIndex = -1;
    bool isPassword = false;   // For PasswordBox
    std::string href = "";     // For Hyperlink
    bool expanded = false;     // For Expander

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

    // === NEW v1.0.9: Gradient ===
    std::string gradientFrom = "";   // Hex cor inicial do gradiente
    std::string gradientTo   = "";   // Hex cor final do gradiente
    std::string gradientDir  = "vertical"; // "vertical" | "horizontal" | "diagonal"

    // === NEW v1.0.9: Shadow ===
    std::string shadowColor   = "#00000080"; // Hex com alpha da sombra
    float shadowBlur    = 0.0f;   // "raio" simulado (número de camadas)
    float shadowOffsetX = 4.0f;   // deslocamento horizontal
    float shadowOffsetY = 4.0f;   // deslocamento vertical

    // === NEW v1.0.9: Glow / Hover ===
    std::string glowColor = "";   // Hex do halo de hover (Card, Button)

    // === NEW v1.0.9: Badge ===
    int badgeCount = 0;           // número exibido no Badge

    // === NEW v1.0.9: Stepper ===
    int steps = 3;                // total de etapas
    int currentStep = 0;          // etapa ativa (0-indexed)
    std::vector<std::string> stepLabels; // rótulos opcionais

    // === NEW v1.0.9: Spinner ===
    float spinAngle = 0.0f;       // ângulo atual (atualizado pelo renderer por dt)

    // === NEW v1.0.9: Tag ===
    // label já existe; customColor define a cor do chip

    std::vector<std::shared_ptr<UINode>> children;
    UINode* parent = nullptr;

    UINode(UINodeType type, const std::string& id) : type(type), id(id) {}
    virtual ~UINode() = default;
    virtual void render(sf::RenderWindow& window, UIStyle* activeStyle) {}
};

// Forward declaration of UIStyle
struct UIStyle;

class UIButtonNode : public UINode {
public:
    using UINode::UINode;
    void render(sf::RenderWindow& window, UIStyle* activeStyle);
};

class UITextNode : public UINode {
public:
    using UINode::UINode;
    void render(sf::RenderWindow& window, UIStyle* activeStyle);
};

class UIContainerNode : public UINode {
public:
    using UINode::UINode;
    void render(sf::RenderWindow& window, UIStyle* activeStyle);
};

#endif
