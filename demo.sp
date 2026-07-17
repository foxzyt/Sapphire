import newton@"1.0.0";

var config_window_width  = 800;
var config_window_height = 600;
var config_window_title  = "Newton - Demo Simples";
var UI = 1;

var DT = 1.0 / 60.0;

// Mundo de fisica
var world = Newton.world({
    "gravity":    [0.0, 600.0],
    "iterations": 6,
    "subSteps":   2,
    "cellSize":   80.0
});

// Chao e paredes
Newton.add(world, Newton.staticBox(400.0, 580.0, 800.0, 40.0, {}));
Newton.add(world, Newton.staticBox(-10.0, 300.0, 20.0, 600.0, {}));
Newton.add(world, Newton.staticBox(810.0, 300.0, 20.0, 600.0, {}));

// Spawna uma bola no centro do topo
function spawnBall() {
    Newton.add(world, Newton.circle(
        400.0, 40.0, 18.0,
        { "material": Newton.RUBBER }
    ));
}

function updateUI() {
    pollEvents();
    if (!isWindowOpen()) return false;
    if (isKeyPressed("Escape")) return false;

    // Pressiona ESPACO para spawnar
    if (isKeyPressed("Space")) spawnBall();

    Newton.step(world, DT);

    clear();

    // Quadrado preto no fundo (fundo ja eh preto via clear())
    // Borda branca do quadrado principal
    drawRect(50.0, 50.0, 700.0, 480.0);

    // "Botao" - retangulo cinza claro embaixo
    drawRect(300.0, 520.0, 200.0, 45.0);

    // Renderiza a fisica dentro do quadrado
    Newton.render(world, {});

    display();
    return true;
}