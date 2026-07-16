// Newton v1.0.0 - Demo
// Demonstrates: gravity, stacking boxes, bouncing ball,
//               spring joint, material system, sleep system.
//
// Controls:
//   SPACE  -- throw a new rubber ball from the top
//   W/S    -- push the spring-pinned metal ball up/down
//   Escape -- close window
//
// The token "UI" below activates the Sapphire graphics mode.

import newton@"1.0.0";

// --- Window configuration (read by sapphire.exe before running) ---
var config_window_width  = 800;
var config_window_height = 600;
var config_window_title  = "Newton v1.0.0 - Physics Demo";

// Trigger UI mode detection
var UI = 1;

var SCREEN_W = 800.0;
var SCREEN_H = 600.0;
var DT       = 1.0 / 60.0;

// --- Build the world ---
var world = Newton.world({
    "gravity":        [0.0, 700.0],
    "iterations":     8,
    "subSteps":       2,
    "sleepThreshold": 4.0,
    "sleepTime":      0.8,
    "cellSize":       80.0
});

// --- Static floor and walls ---
Newton.add(world, Newton.staticBox(
    SCREEN_W / 2.0, SCREEN_H - 20.0,
    SCREEN_W, 40.0,
    { "friction": 0.6 }
));
Newton.add(world, Newton.staticBox(-10.0, SCREEN_H / 2.0, 20.0, SCREEN_H, {}));
Newton.add(world, Newton.staticBox(SCREEN_W + 10.0, SCREEN_H / 2.0, 20.0, SCREEN_H, {}));

// --- Ramp (staggered static boxes) ---
var ri = 0;
while (ri < 5) {
    Newton.add(world, Newton.staticBox(
        200.0 + ri * 40.0,
        400.0 - ri * 30.0,
        42.0, 12.0,
        { "friction": 0.4 }
    ));
    ri = ri + 1;
}

// --- Stack of wooden boxes ---
var si = 0;
while (si < 5) {
    Newton.add(world, Newton.box(
        600.0,
        SCREEN_H - 60.0 - si * 42.0,
        38.0, 38.0,
        { "material": Newton.WOOD }
    ));
    si = si + 1;
}

// --- Bouncing rubber ball ---
var ball = Newton.add(world, Newton.circle(
    200.0, 100.0, 20.0,
    { "material": Newton.RUBBER }
));

// --- Metal ball pinned by a spring joint ---
var pinBall = Newton.add(world, Newton.circle(
    400.0, 250.0, 18.0,
    { "material": Newton.METAL, "mass": 3.0 }
));
var pin = Newton.pinJoint(pinBall, 400.0, 100.0, 200.0, 15.0);
Newton.addJoint(world, pin);

// --- Two circles connected by distance spring ---
var springA = Newton.add(world, Newton.circle(
    100.0, 300.0, 12.0,
    { "material": Newton.RUBBER, "mass": 0.8 }
));
var springB = Newton.add(world, Newton.circle(
    160.0, 300.0, 12.0,
    { "material": Newton.RUBBER, "mass": 0.8 }
));
Newton.addJoint(world, Newton.distanceJoint(springA, springB, 60.0, 300.0, 20.0));

// --- Ice block (slides far) ---
Newton.add(world, Newton.box(
    350.0, SCREEN_H - 60.0, 50.0, 30.0,
    { "material": Newton.ICE }
));

// --- Stone ball (heavy, sinks fast) ---
Newton.add(world, Newton.circle(
    500.0, 100.0, 15.0,
    { "material": Newton.STONE, "mass": 6.0 }
));

var frameCount = 0;

// --- Spawn a rubber ball ---
function spawnBall() {
    Newton.add(world, Newton.circle(
        400.0, 30.0, 14.0,
        { "material": Newton.RUBBER }
    ));
    Newton.wakeNear(world, 400.0, 30.0, 150.0);
}

// --- updateUI: called every frame by the Sapphire runtime loop ---
function updateUI() {
    pollEvents();

    if (!isWindowOpen()) return false;

    if (isKeyPressed("Escape")) return false;
    if (isKeyPressed("Space"))  spawnBall();
    if (isKeyPressed("W"))      Newton.applyImpulse(pinBall,  0.0, -800.0);
    if (isKeyPressed("S"))      Newton.applyImpulse(pinBall,  0.0,  800.0);

    Newton.step(world, DT);

    clear();
    Newton.render(world, {});
    display();

    if (frameCount == 0 || frameCount == 180 || frameCount == 360) {
        Newton.debug(world);
    }
    frameCount = frameCount + 1;

    return true;
}