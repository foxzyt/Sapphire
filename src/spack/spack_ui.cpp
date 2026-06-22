#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <iostream>
#include <fstream>
#include "spack.h"
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

struct TextBox {
    sf::RectangleShape rect;
    std::string value;
    bool isFocused = false;
    std::string label;
    sf::Text labelText;
    sf::Text text;

    TextBox(sf::Font& font, const std::string& lbl, float x, float y, float width, const std::string& defaultValue = "", bool hasBrowseBtn = false) 
        : labelText(font, lbl, 18), text(font, defaultValue, 18) {
        label = lbl;
        value = defaultValue;
        
        labelText.setPosition({x, y});
        labelText.setFillColor(sf::Color(200, 200, 200));

        rect.setPosition({x, y + 25});
        rect.setSize({width, 30});
        rect.setFillColor(sf::Color(50, 50, 50));
        rect.setOutlineThickness(1);
        rect.setOutlineColor(sf::Color(100, 100, 100));

        text.setPosition({x + 5, y + 28});
        text.setFillColor(sf::Color::White);
    }

    void draw(sf::RenderWindow& window) {
        if (isFocused) rect.setOutlineColor(sf::Color(0, 122, 204));
        else rect.setOutlineColor(sf::Color(100, 100, 100));
        
        window.draw(labelText);
        window.draw(rect);
        
        std::string displayStr = value + (isFocused ? "_" : "");
        text.setString(displayStr);
        window.draw(text);
    }

    std::string browseFile(bool isSave) {
#ifdef _WIN32
        char filename[MAX_PATH];
        filename[0] = '\0';
        OPENFILENAMEA ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFilter = "All Files\0*.*\0Sapphire Scripts (*.sp)\0*.sp\0";
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
        if (isSave) ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

        if (isSave ? GetSaveFileNameA(&ofn) : GetOpenFileNameA(&ofn)) {
            value = std::string(filename);
            return value;
        }
#endif
        return value;
    }

    bool checkBrowseClicked(sf::Vector2f mousePos) {
        // Browse button is drawn next to it manually in the main loop for simplicity
        return false;
    }

    void handleClick(sf::Vector2f mousePos) {
        isFocused = rect.getGlobalBounds().contains(mousePos);
    }

    void handleTextEntered(const sf::Event::TextEntered& textEvent) {
        if (!isFocused) return;
        if (textEvent.unicode == '\b') { // Backspace
            if (!value.empty()) value.pop_back();
        } else if (textEvent.unicode < 128 && textEvent.unicode > 31) {
            value += static_cast<char>(textEvent.unicode);
        }
    }
};

struct CheckBox {
    sf::RectangleShape box;
    bool checked;
    sf::Text labelText;

    CheckBox(sf::Font& font, const std::string& lbl, float x, float y, bool defaultChecked = false) 
        : labelText(font, lbl, 18) {
        checked = defaultChecked;

        box.setPosition({x, y});
        box.setSize({20, 20});
        box.setFillColor(checked ? sf::Color(0, 122, 204) : sf::Color(50, 50, 50));
        box.setOutlineThickness(1);
        box.setOutlineColor(sf::Color(100, 100, 100));

        labelText.setPosition({x + 30, y - 2});
        labelText.setFillColor(sf::Color(200, 200, 200));
    }

    void draw(sf::RenderWindow& window) {
        box.setFillColor(checked ? sf::Color(0, 122, 204) : sf::Color(50, 50, 50));
        window.draw(box);
        window.draw(labelText);
    }

    void handleClick(sf::Vector2f mousePos) {
        if (box.getGlobalBounds().contains(mousePos)) {
            checked = !checked;
        }
    }
};

void run_native_spack_ui(const std::string& runner_path) {
    sf::RenderWindow window(sf::VideoMode({1000, 800}), "Spack C++ Native UI");
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.openFromFile("C:/Windows/Fonts/arial.ttf")) {
        std::cerr << "Could not load font." << std::endl;
    }

    sf::Text title(font, "Spack Native Packager", 28);
    title.setPosition({50, 20});
    title.setFillColor(sf::Color::White);

    TextBox tbEntry(font, "Entry File (.sp):", 50, 70, 700, "main.sp");
    TextBox tbOutput(font, "Output File (.exe):", 50, 140, 700, "app.exe");
    TextBox tbAuthor(font, "Author:", 50, 210, 700, "Spack User");
    TextBox tbVersion(font, "Version:", 50, 280, 700, "1.0.0");
    TextBox tbIcon(font, "Icon Path (.ico) (optional):", 50, 350, 700, "");

    CheckBox cbNoConsole(font, "Hide Console Window (-noconsole)", 50, 430, false);
    CheckBox cbOptimize(font, "Optimize Bytecode", 50, 470, true);
    CheckBox cbSoftMode(font, "Soft Mode (Disable Type Checking)", 50, 510, false);
    CheckBox cbAdmin(font, "Require Admin Privileges (Manifest)", 50, 550, false);

    sf::RectangleShape btnEntryBrowse({80, 30});
    btnEntryBrowse.setPosition({760, 95});
    btnEntryBrowse.setFillColor(sf::Color(70, 70, 70));
    sf::Text tBrowseEntry(font, "Browse", 16);
    tBrowseEntry.setPosition({770, 100});
    
    sf::RectangleShape btnOutBrowse({80, 30});
    btnOutBrowse.setPosition({760, 165});
    btnOutBrowse.setFillColor(sf::Color(70, 70, 70));
    sf::Text tBrowseOut(font, "Browse", 16);
    tBrowseOut.setPosition({770, 170});

    sf::RectangleShape btnIconBrowse({80, 30});
    btnIconBrowse.setPosition({760, 375});
    btnIconBrowse.setFillColor(sf::Color(70, 70, 70));
    sf::Text tBrowseIcon(font, "Browse", 16);
    tBrowseIcon.setPosition({770, 380});

    sf::RectangleShape button({250, 50});
    button.setPosition({50, 620});
    button.setFillColor(sf::Color(0, 122, 204));

    sf::Text btnText(font, "GENERATE & PACK", 20);
    btnText.setPosition({80, 632});
    btnText.setFillColor(sf::Color::White);

    sf::Text status(font, "Ready.", 18);
    status.setPosition({320, 635});
    status.setFillColor(sf::Color(150, 150, 150));

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
            if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mb->button == sf::Mouse::Button::Left) {
                    sf::Vector2f mPos = sf::Vector2f(sf::Mouse::getPosition(window));
                    tbEntry.handleClick(mPos);
                    tbOutput.handleClick(mPos);
                    tbAuthor.handleClick(mPos);
                    tbVersion.handleClick(mPos);
                    tbIcon.handleClick(mPos);
                    cbNoConsole.handleClick(mPos);
                    cbOptimize.handleClick(mPos);
                    cbSoftMode.handleClick(mPos);
                    cbAdmin.handleClick(mPos);

                    if (btnEntryBrowse.getGlobalBounds().contains(mPos)) {
                        tbEntry.browseFile(false);
                    }
                    if (btnOutBrowse.getGlobalBounds().contains(mPos)) {
                        tbOutput.browseFile(true);
                    }
                    if (btnIconBrowse.getGlobalBounds().contains(mPos)) {
                        tbIcon.browseFile(false);
                    }

                    if (button.getGlobalBounds().contains(mPos)) {
                        std::ofstream confFile("SpackConfig.txt");
                        if (confFile.is_open()) {
                            confFile << "EntryFile=" << tbEntry.value << "\n";
                            confFile << "OutputFile=" << tbOutput.value << "\n";
                            confFile << "Author=" << tbAuthor.value << "\n";
                            confFile << "Version=" << tbVersion.value << "\n";
                            confFile << "IconPath=" << tbIcon.value << "\n";
                            confFile << "NoConsole=" << (cbNoConsole.checked ? "true" : "false") << "\n";
                            confFile << "Optimize=" << (cbOptimize.checked ? "true" : "false") << "\n";
                            confFile << "SoftMode=" << (cbSoftMode.checked ? "true" : "false") << "\n";
                            confFile << "RequireAdmin=" << (cbAdmin.checked ? "true" : "false") << "\n";
                            confFile.close();
                        }

                        // Empacotar
                        SpackConfig config = parse_spack_config("SpackConfig.txt");
                        if (config.EntryFile.empty()) {
                            status.setFillColor(sf::Color::Red);
                            status.setString("Error: Entry File is empty.");
                        } else {
                            if (pack_executable(config, runner_path)) {
                                status.setFillColor(sf::Color::Green);
                                status.setString("Success! Created " + config.OutputFile);
                            } else {
                                status.setFillColor(sf::Color::Red);
                                status.setString("Error! Check terminal for details.");
                            }
                        }
                    }
                }
            }
            if (const auto* te = event->getIf<sf::Event::TextEntered>()) {
                tbEntry.handleTextEntered(*te);
                tbOutput.handleTextEntered(*te);
                tbAuthor.handleTextEntered(*te);
                tbVersion.handleTextEntered(*te);
                tbIcon.handleTextEntered(*te);
            }
        }

        window.clear(sf::Color(30, 30, 30));
        
        window.draw(title);
        tbEntry.draw(window);
        window.draw(btnEntryBrowse); window.draw(tBrowseEntry);
        
        tbOutput.draw(window);
        window.draw(btnOutBrowse); window.draw(tBrowseOut);
        
        tbAuthor.draw(window);
        tbVersion.draw(window);
        tbIcon.draw(window);
        window.draw(btnIconBrowse); window.draw(tBrowseIcon);

        cbNoConsole.draw(window);
        cbOptimize.draw(window);
        cbSoftMode.draw(window);
        cbAdmin.draw(window);

        window.draw(button);
        window.draw(btnText);
        window.draw(status);

        window.display();
    }
}
