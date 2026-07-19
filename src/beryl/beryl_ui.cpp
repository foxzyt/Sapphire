#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include "beryl.h"
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#endif
#include <cstdint>
#include <cmath>

using namespace sf;
using namespace std;

// Colors based on GitHub Dark Dimmed / Modern Cyber aesthetic
const Color bg_color(13, 17, 23);
const Color panel_color(22, 27, 34);
const Color input_color(33, 38, 45);
const Color focus_color(88, 166, 255);
const Color text_color(201, 209, 217);
const Color muted_color(139, 148, 158);
const Color primary_color(35, 134, 54); // Green for build button
const Color primary_hover(46, 160, 67);
const Color error_color(248, 81, 73);

float lerp_anim(float a, float b, float f) {
    return a + f * (b - a);
}

Color lerpColor(Color a, Color b, float f) {
    return Color(
        (uint8_t)lerp_anim(a.r, b.r, f),
        (uint8_t)lerp_anim(a.g, b.g, f),
        (uint8_t)lerp_anim(a.b, b.b, f),
        (uint8_t)lerp_anim(a.a, b.a, f)
    );
}

ConvexShape createRoundedRect(float width, float height, float radius, int points = 8) {
    ConvexShape shape(points * 4);
    float x = 0, y = 0;
    for (int i = 0; i < points; i++) {
        float angle = (180.0f + 90.0f * i / (points - 1)) * 3.14159f / 180.0f;
        shape.setPoint(i, Vector2f(x + radius + radius * cos(angle), y + radius + radius * sin(angle)));
    }
    x = width - radius * 2;
    for (int i = 0; i < points; i++) {
        float angle = (270.0f + 90.0f * i / (points - 1)) * 3.14159f / 180.0f;
        shape.setPoint(points + i, Vector2f(x + radius + radius * cos(angle), y + radius + radius * sin(angle)));
    }
    y = height - radius * 2;
    for (int i = 0; i < points; i++) {
        float angle = (0.0f + 90.0f * i / (points - 1)) * 3.14159f / 180.0f;
        shape.setPoint(points * 2 + i, Vector2f(x + radius + radius * cos(angle), y + radius + radius * sin(angle)));
    }
    x = 0;
    for (int i = 0; i < points; i++) {
        float angle = (90.0f + 90.0f * i / (points - 1)) * 3.14159f / 180.0f;
        shape.setPoint(points * 3 + i, Vector2f(x + radius + radius * cos(angle), y + radius + radius * sin(angle)));
    }
    return shape;
}

struct AnimValue {
    float current = 0.0f;
    float target = 0.0f;
    void update(float dt) { current = lerp_anim(current, target, dt * 15.0f); }
};

struct TextBox {
    ConvexShape rect;
    string value;
    bool isFocused = false;
    Text labelText;
    Text text;
    float mx, my, mw;
    AnimValue focusAnim;

    TextBox(Font& font, const string& lbl, float x, float y, float width, const string& def = "") 
        : labelText(font, lbl, 14), text(font, def, 14) {
        value = def; mx = x; my = y; mw = width;
        labelText.setPosition(Vector2f(x, y));
        labelText.setFillColor(muted_color);
        rect = createRoundedRect(width, 36, 6);
        rect.setPosition(Vector2f(x, y + 22));
        rect.setFillColor(input_color);
        rect.setOutlineThickness(1.5f);
        text.setPosition(Vector2f(x + 12, y + 30));
        text.setFillColor(text_color);
    }

    void update(float dt) {
        focusAnim.target = isFocused ? 1.0f : 0.0f;
        focusAnim.update(dt);
    }

    void draw(RenderWindow& window) {
        Color outColor = lerpColor(Color(48, 54, 61), focus_color, focusAnim.current);
        rect.setOutlineColor(outColor);
        window.draw(labelText);
        window.draw(rect);
        text.setString(value + (isFocused ? "|" : ""));
        window.draw(text);
    }

    bool handleClick(float mx_in, float my_in) {
        isFocused = rect.getGlobalBounds().contains(Vector2f(mx_in, my_in));
        return isFocused;
    }

    void handleInput(uint32_t unicode) {
        if (!isFocused) return;
        if (unicode == 8 && !value.empty()) value.pop_back(); // Backspace
        else if (unicode >= 32 && unicode < 127) value += (char)unicode;
    }
    
    string browseFile(bool isSave) {
#ifdef _WIN32
        OPENFILENAMEA ofn;
        char fileName[MAX_PATH] = "";
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFile = fileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        if (isSave) {
            ofn.lpstrFilter = "Executable (*.exe)\0*.exe\0";
            if (GetSaveFileNameA(&ofn)) { value = fileName; return value; }
        } else {
            ofn.lpstrFilter = "Sapphire Script (*.sp)\0*.sp\0All Files (*.*)\0*.*\0";
            if (GetOpenFileNameA(&ofn)) { value = fileName; return value; }
        }
#endif
        return "";
    }
};

struct ToggleSwitch {
    ConvexShape track;
    CircleShape thumb;
    Text labelText;
    bool isChecked;
    float mx, my;
    AnimValue checkAnim;
    AnimValue hoverAnim;

    ToggleSwitch(Font& font, const string& lbl, float x, float y, bool def = false) 
        : labelText(font, lbl, 14) {
        isChecked = def; mx = x; my = y;
        checkAnim.current = def ? 1.0f : 0.0f;
        checkAnim.target = checkAnim.current;
        labelText.setPosition(Vector2f(x + 50, y + 2));
        labelText.setFillColor(text_color);
        track = createRoundedRect(40, 20, 10);
        track.setPosition(Vector2f(x, y));
        thumb.setRadius(7);
        thumb.setOrigin(Vector2f(7, 7));
    }

    void update(float dt, float mouseX, float mouseY) {
        checkAnim.target = isChecked ? 1.0f : 0.0f;
        checkAnim.update(dt);
        
        bool isHovered = track.getGlobalBounds().contains(Vector2f(mouseX, mouseY));
        hoverAnim.target = isHovered ? 1.0f : 0.0f;
        hoverAnim.update(dt);
    }

    void draw(RenderWindow& window) {
        Color baseTrack = lerpColor(input_color, Color(60, 60, 70), hoverAnim.current);
        track.setFillColor(lerpColor(baseTrack, primary_color, checkAnim.current));
        float thumbX = lerp_anim(mx + 10, mx + 30, checkAnim.current);
        thumb.setPosition(Vector2f(thumbX, my + 10));
        thumb.setFillColor(Color(255, 255, 255));
        window.draw(track);
        window.draw(thumb);
        window.draw(labelText);
    }

    bool handleClick(float mx_in, float my_in) {
        if (track.getGlobalBounds().contains(Vector2f(mx_in, my_in))) {
            isChecked = !isChecked;
            return true;
        }
        return false;
    }
};

struct Button {
    ConvexShape rect;
    Text text;
    float mx, my, mw, mh;
    bool isPrimary;
    AnimValue hoverAnim;
    AnimValue pressAnim;

    Button(Font& font, const string& lbl, float x, float y, float w, float h, bool primary = false) 
        : text(font, lbl, 16) {
        mx = x; my = y; mw = w; mh = h; isPrimary = primary;
        rect = createRoundedRect(w, h, 8);
        rect.setPosition(Vector2f(x, y));
        FloatRect tb = text.getLocalBounds();
        text.setPosition(Vector2f(x + w/2.0f - tb.size.x/2.0f, y + h/2.0f - tb.size.y/2.0f - 4));
        text.setFillColor(Color::White);
    }

    void update(float dt, float mouseX, float mouseY, bool isMouseDown) {
        bool isHovered = rect.getGlobalBounds().contains(Vector2f(mouseX, mouseY));
        hoverAnim.target = isHovered ? 1.0f : 0.0f;
        hoverAnim.update(dt);
        pressAnim.target = (isHovered && isMouseDown) ? 1.0f : 0.0f;
        pressAnim.update(dt);
    }

    void draw(RenderWindow& window) {
        Color base = isPrimary ? primary_color : input_color;
        Color hover = isPrimary ? primary_hover : Color(48, 54, 61);
        Color target = lerpColor(base, hover, hoverAnim.current);
        target = lerpColor(target, Color(target.r/2, target.g/2, target.b/2), pressAnim.current);
        rect.setFillColor(target);
        window.draw(rect);
        window.draw(text);
    }

    bool isClicked(float mx_in, float my_in) {
        return rect.getGlobalBounds().contains(Vector2f(mx_in, my_in));
    }
};

void run_beryl_ui() {
    ContextSettings settings;
    settings.antiAliasingLevel = 8;
    RenderWindow window(VideoMode(sf::Vector2u(800, 640)), "Beryl App Packager", Style::Titlebar | Style::Close, State::Windowed, settings);
    window.setFramerateLimit(60);

    Font font;
    // Load a font, in SFML 3 it's openFromFile, in 2 it's loadFromFile.
    // We will use openFromFile because the original code used it.
    if (!font.openFromFile("C:/Windows/Fonts/segoeui.ttf")) {
        if (!font.openFromFile("C:/Windows/Fonts/arial.ttf")) {
            cerr << "Could not load font." << endl;
        }
    }

    TextBox tbEntry(font, "Entry Script (.sp)", 40, 80, 580);
    Button btnEntryBrowse(font, "Browse", 630, 102, 120, 36);
    TextBox tbOutput(font, "Output Executable (.exe)", 40, 150, 580, "app.exe");
    Button btnOutputBrowse(font, "Browse", 630, 172, 120, 36);

    TextBox tbAssets(font, "Assets Folder (Optional)", 40, 220, 580);

    ToggleSwitch tsConsole(font, "Hide Console Window", 40, 320, true);
    ToggleSwitch tsEncrypt(font, "Encrypt Bytecode", 40, 360, true);
    ToggleSwitch tsCompress(font, "Compress Assets", 300, 320, false);
    ToggleSwitch tsAdmin(font, "Require Admin", 300, 360, false);

    Button btnBuild(font, "Package Application", 200, 480, 400, 50, true);

    Text title(font, "Beryl", 32);
    title.setFillColor(Color::White);
    title.setPosition(Vector2f(40, 20));
    
    Text subtitle(font, "Sapphire Native Packager", 14);
    subtitle.setFillColor(focus_color);
    subtitle.setPosition(Vector2f(130, 35));
    
    Text statusText(font, "Ready", 14);
    statusText.setFillColor(muted_color);
    statusText.setPosition(Vector2f(40, 560));

    Clock clock;
    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();
        Vector2i mousePos = Mouse::getPosition(window);
        bool mouseDown = Mouse::isButtonPressed(Mouse::Button::Left);

        while (const std::optional event = window.pollEvent()) {
            if (event->is<Event::Closed>()) {
                window.close();
            }
            else if (const auto* mouseBtn = event->getIf<Event::MouseButtonPressed>()) {
                if (mouseBtn->button == Mouse::Button::Left) {
                    tbEntry.handleClick(mouseBtn->position.x, mouseBtn->position.y);
                    tbOutput.handleClick(mouseBtn->position.x, mouseBtn->position.y);
                    tbAssets.handleClick(mouseBtn->position.x, mouseBtn->position.y);
                    
                    tsConsole.handleClick(mouseBtn->position.x, mouseBtn->position.y);
                    tsEncrypt.handleClick(mouseBtn->position.x, mouseBtn->position.y);
                    tsCompress.handleClick(mouseBtn->position.x, mouseBtn->position.y);
                    tsAdmin.handleClick(mouseBtn->position.x, mouseBtn->position.y);

                    if (btnEntryBrowse.isClicked(mouseBtn->position.x, mouseBtn->position.y)) tbEntry.browseFile(false);
                    if (btnOutputBrowse.isClicked(mouseBtn->position.x, mouseBtn->position.y)) tbOutput.browseFile(true);
                    
                    if (btnBuild.isClicked(mouseBtn->position.x, mouseBtn->position.y)) {
                        statusText.setFillColor(Color(220, 220, 100));
                        statusText.setString("Building...");
                        window.draw(statusText);
                        window.display();
                        
                        BerylConfig cfg;
                        cfg.EntryFile = tbEntry.value;
                        cfg.OutputFile = tbOutput.value;
                        cfg.AssetsFolder = tbAssets.value;
                        cfg.NoConsole = tsConsole.isChecked;
                        cfg.Encrypt = tsEncrypt.isChecked;
                        cfg.Compress = tsCompress.isChecked;
                        cfg.RequireAdmin = tsAdmin.isChecked;
                        
                        if (pack_executable(cfg, "runner.exe")) {
                            statusText.setFillColor(primary_hover);
                            statusText.setString("Success! Application packaged successfully.");
                        } else {
                            statusText.setFillColor(error_color);
                            statusText.setString("Failed to package application. Check console for details.");
                        }
                    }
                }
            }
            else if (const auto* textEv = event->getIf<Event::TextEntered>()) {
                tbEntry.handleInput(textEv->unicode);
                tbOutput.handleInput(textEv->unicode);
                tbAssets.handleInput(textEv->unicode);
            }
        }

        tbEntry.update(dt);
        tbOutput.update(dt);
        tbAssets.update(dt);
        
        tsConsole.update(dt, mousePos.x, mousePos.y);
        tsEncrypt.update(dt, mousePos.x, mousePos.y);
        tsCompress.update(dt, mousePos.x, mousePos.y);
        tsAdmin.update(dt, mousePos.x, mousePos.y);

        btnEntryBrowse.update(dt, mousePos.x, mousePos.y, mouseDown);
        btnOutputBrowse.update(dt, mousePos.x, mousePos.y, mouseDown);
        btnBuild.update(dt, mousePos.x, mousePos.y, mouseDown);

        window.clear(bg_color);
        
        window.draw(title);
        window.draw(subtitle);

        tbEntry.draw(window);
        btnEntryBrowse.draw(window);
        tbOutput.draw(window);
        btnOutputBrowse.draw(window);
        tbAssets.draw(window);

        tsConsole.draw(window);
        tsEncrypt.draw(window);
        tsCompress.draw(window);
        tsAdmin.draw(window);

        btnBuild.draw(window);
        
        window.draw(statusText);

        window.display();
    }
}
