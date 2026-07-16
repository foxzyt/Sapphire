var UI = 1;

function animateBox() {
    print("Animating! The button was clicked.");
    Animate("animBox", {
        "duration": 2.0,
        "loop": false,
        "easing": "ease-in-out",
        "keyframes": [
            { "time": 0.0, "width": 100, "height": 50, "color": "#FF0000FF" },
            { "time": 0.5, "width": 200, "height": 100, "color": "#00FF00FF" },
            { "time": 1.0, "width": 100, "height": 50, "color": "#0000FFFF" }
        ]
    });
}

let window = Window({
    "id": "mainWindow",
    "title": "Sapphire Advanced UI Showcase",
    "style": "default",
    "width": 800,
    "height": 600,
    "direction": "column",
    "align": "center",
    "justify": "center",
    "gap": 20,
    "children": [
        Text({
            "id": "titleText",
            "text": "Advanced UI & Animations",
            "size": 24,
            "color": "#FFFFFF"
        }),
        StackPanel({
            "id": "panel1",
            "direction": "row",
            "gap": 15,
            "children": [
                Button({
                    "id": "animBtn",
                    "label": "Trigger Animation",
                    "onClick": animateBox
                }),
                ToggleSwitch({
                    "id": "myToggle",
                    "checked": true
                })
            ]
        }),
        Border({
            "id": "animBox",
            "width": 100,
            "height": 50,
            "color": "#FF0000FF"
        }),
        ProgressBar({
            "id": "myProgress",
            "progress": 75.0,
            "width": 300,
            "height": 20
        }),
        Slider({
            "id": "mySlider",
            "value": 50.0,
            "min": 0.0,
            "max": 100.0,
            "width": 300
        }),
        StackPanel({
            "direction": "row",
            "gap": 10,
            "children": [
                RadioBox({ "id": "rb1", "checked": true, "label": "Option A" }),
                RadioBox({ "id": "rb2", "checked": false, "label": "Option B" })
            ]
        }),
        Hyperlink({
            "id": "link",
            "label": "Visit Sapphire Lang",
            "href": "https://sapphire-lang.org"
        })
    ]
});

function updateUI() {
    let handler = Render(window);
    if (handler) {
        handler(); // Execute the click handler
    }
    
    // Auto-fill progress bar
    if (window.children[3].progress < 100.0) {
        window.children[3].progress = window.children[3].progress + 0.5;
    } else {
        window.children[3].progress = 0.0;
    }
    
    return true;
}

animateBox();
while (updateUI()) {}
