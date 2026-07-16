var UI = 1;

function animateBox() {
    Animate("animBox", {
        "duration": 2.0,
        "loop": true,
        "easing": "ease-in-out",
        "keyframes": [
            { "time": 0.0, "width": 100, "height": 50, "color": "#FF0000FF", "opacity": 255 },
            { "time": 0.5, "width": 200, "height": 100, "color": "#00FF00FF", "opacity": 128 },
            { "time": 1.0, "width": 100, "height": 50, "color": "#0000FFFF", "opacity": 255 }
        ]
    });
}

let window = Window({
    "id": "mainWindow",
    "title": "Animation Test",
    "width": 600,
    "height": 400,
    "direction": "column",
    "align": "center",
    "justify": "center",
    "gap": 20,
    "children": [
        Button({
            "id": "animBtn",
            "label": "Start Animation",
            "onClick": animateBox
        }),
        Border({
            "id": "animBox",
            "width": 100,
            "height": 50,
            "color": "#FF0000FF"
        })
    ]
});

function updateUI() {
    let handler = Render(window);
    if (handler) {
        handler();
    }
    return true;
}

while (updateUI()) {}
