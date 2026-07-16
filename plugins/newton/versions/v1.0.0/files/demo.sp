// Newton v1.0.0 - Demo
// Demonstrates: gravity, stacking boxes, bouncing ball,
//               spring joint, material system, sleep system,
//               and keyboard interaction.
//
// Controls:
//   SPACE  -- throw a new rubber ball from the top
//   W/S    -- push the spring-pinned metal ball up/down
//   Escape -- close window

import newton@"1.0.0";

var SCREEN_W = 800.0;
var SCREEN_H = 600.0;
var FPS      = 60.0;
var DT       = 1.0 / FPS;

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
var floor = Newton.add(world, Newton.staticBox(
    SCREEN_W / 2.0, SCREEN_H - 20.0,
    SCREEN_W, 40.0,
    { "friction": 0.6 }
));

var wallL = Newton.add(world, Newton.staticBox(
    -10.0, SCREEN_H / 2.0,
    20.0, SCREEN_H, {}
));

var wallR = Newton.add(world, Newton.staticBox(
    SCREEN_W + 10.0, SCREEN_H / 2.0,
    20.0, SCREEN_H, {}
));

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
    400.0, 200.0, 18.0,
    { "material": Newton.METAL, "mass": 3.0 }
));
var pin = Newton.pinJoint(pinBall, 400.0, 100.0, 200.0, 15.0);
Newton.addJoint(world, pin);

// --- Two balls connected by distance spring ---
var springA = Newton.add(world, Newton.circle(
    100.0, 300.0, 12.0,
    { "material": Newton.RUBBER, "mass": 0.8 }
));
var springB = Newton.add(world, Newton.circle(
    140.0, 300.0, 12.0,
    { "material": Newton.RUBBER, "mass": 0.8 }
));
var spring = Newton.distanceJoint(springA, springB, 60.0, 300.0, 20.0);
Newton.addJoint(world, spring);

// --- Ice block (slides far) ---
Newton.add(world, Newton.box(
    350.0, SCREEN_H - 60.0, 50.0, 30.0,
    { "material": Newton.ICE }
));

// --- Stone ball (heavy) ---
Newton.add(world, Newton.circle(
    500.0, 100.0, 15.0,
    { "material": Newton.STONE, "mass": 6.0 }
));

// --- Spawn a rubber ball at random X ---
function spawnBall() {
    Newton.add(world, Newton.circle(
        400.0, 30.0, 14.0,
        { "material": Newton.RUBBER }
    ));
    Newton.wakeNear(world, 400.0, 30.0, 150.0);
}

// --- Main game loop ---
var frame = 0;
while (isWindowOpen()) {
    pollEvents();
    clear();

    if (isKeyPressed("Space")) spawnBall();
    if (isKeyPressed("W"))     Newton.applyImpulse(pinBall,  0.0, -800.0);
    if (isKeyPressed("S"))     Newton.applyImpulse(pinBall,  0.0,  800.0);
    if (isKeyPressed("Escape")) {
        print("[Newton] Closing demo.\n");
        break;
    }

    Newton.step(world, DT);
    Newton.render(world, { "showContacts": false });

    display();

    if (frame == 0 || frame == 120 || frame == 240) {
        Newton.debug(world);
    }
    frame = frame + 1;
}

print("[Newton] Demo ended. Total frames: " + valueToString(frame) + "\n");
var finalStats = Newton.stats(world);
print("[Newton] Final -- bodies: " + valueToString(finalStats["bodies"])
    + ", sleeping: " + valueToString(finalStats["sleeping"]) + "\n");