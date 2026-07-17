import newton@"0.1.0";

function main() {
    var world = Newton.world({ "gravity": [0.0, 10.0], "iterations": 4.0 });
    var floor = Newton.add(world, Newton.staticBox(0.0, 10.0, 20.0, 2.0, {}));
    var ball = Newton.add(world, Newton.circle(0.0, 0.0, 1.0, {
        "mass": 1.0,
        "restitution": 0.0
    }));

    ball["position"][1] = 8.5;
    Newton.step(world, 0.0);
    if (len(world["contacts"]) != 1) {
        print("FAIL: circle and box collision was not detected");
        return;
    }
    ball["position"] = [0.0, 0.0];

    var i = 0;
    while (i < 180) {
        Newton.step(world, 1.0 / 60.0);
        i = i + 1;
    }

    if (ball["position"][1] < 7.5 || ball["position"][1] > 8.5) {
        print("FAIL: circle did not settle on static box");
        return;
    }

    Newton.applyImpulse(ball, [0.0, -5.0]);
    if (ball["velocity"][1] >= 0.0) {
        print("FAIL: impulse was not applied");
        return;
    }

    var vector = Newton.normalize([3.0, 4.0]);
    if (vector[0] < 0.59 || vector[0] > 0.61 || vector[1] < 0.79 || vector[1] > 0.81) {
        print("FAIL: vector normalization");
        return;
    }

    print("Test passed.");
}

main();
