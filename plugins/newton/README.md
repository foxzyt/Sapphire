# Newton

Newton is an advanced 2D rigid-body physics engine implemented **entirely in Sapphire**.
No native code is injected into the Sapphire runtime — Newton runs as a pure plugin,
leveraging **Infinitum** for mathematical primitives and Sapphire's built-in graphics
globals (`drawRect`, `clear`, `display`, `isWindowOpen`) for rendering.

## Version history

| Version | Highlights |
|---------|-----------|
| 0.1.0   | Basic circles, boxes, gravity, impulse resolution |
| **1.0.0**   | Spatial grid, SAT, Coulomb friction, joints, sleep, raycast, materials |

## Installation

Newton depends on Infinitum. Install both in your project:

```
mine install infinitum --local
mine install newton --local
```

Then import:

```sapphire
import newton@"1.0.0";
```

## Quick start

```sapphire
import newton@"1.0.0";

var world = Newton.world({ "gravity": [0.0, 980.0], "iterations": 6 });

var floor = Newton.add(world, Newton.staticBox(400.0, 580.0, 800.0, 40.0, {}));
var ball  = Newton.add(world, Newton.circle(400.0, 50.0, 20.0, {
    "material": Newton.RUBBER
}));

while (isWindowOpen()) {
    pollEvents();
    clear();
    Newton.step(world, 1.0 / 60.0);
    Newton.render(world, {});
    display();
}
```

## API Reference

### World

| Function | Description |
|----------|-------------|
| `Newton.world(opts)` | Create world. Options: `gravity`, `iterations`, `subSteps`, `sleepThreshold`, `sleepTime`, `cellSize`, `onCollision`, `layerMask` |
| `Newton.add(world, body)` | Add body to world, returns body |
| `Newton.remove(world, body)` | Mark body for removal (swept next step) |
| `Newton.addJoint(world, joint)` | Add a joint |
| `Newton.step(world, dt)` | Advance simulation by `dt` seconds |

### Body Constructors

| Function | Description |
|----------|-------------|
| `Newton.circle(x, y, radius, opts)` | Dynamic circle |
| `Newton.box(x, y, w, h, opts)` | Dynamic AABB box |
| `Newton.staticCircle(x, y, radius, opts)` | Static (immovable) circle |
| `Newton.staticBox(x, y, w, h, opts)` | Static (immovable) box |

Body `opts` keys: `mass`, `material`, `restitution`, `friction`, `linearDamp`, `angularDamp`, `angle`, `tag`, `layer`, `userData`, `static`

### Materials

`Newton.RUBBER`, `Newton.WOOD`, `Newton.METAL`, `Newton.ICE`, `Newton.STONE`

Pass as `opts["material"]` in any body constructor.

### Forces & Impulses

| Function | Description |
|----------|-------------|
| `Newton.applyForce(body, fx, fy)` | Accumulate force for next step |
| `Newton.applyImpulse(body, ix, iy)` | Instant velocity change |
| `Newton.applyTorque(body, torque)` | Accumulate torque (angular force) |

### Joints

| Function | Description |
|----------|-------------|
| `Newton.distanceJoint(a, b, targetDist, stiffness, damping)` | Spring between two bodies |
| `Newton.pinJoint(body, wx, wy, stiffness, damping)` | Pin body to world point |

### Queries

| Function | Description |
|----------|-------------|
| `Newton.raycast(world, ox, oy, dx, dy, maxDist)` | Returns `{body, t, point}` or `nil` |
| `Newton.wakeNear(world, x, y, radius)` | Wake all sleeping bodies within radius |

### Rendering

| Function | Description |
|----------|-------------|
| `Newton.render(world, opts)` | Draw all bodies. `opts["showContacts"]: true` shows contact points |

### Diagnostics

| Function | Description |
|----------|-------------|
| `Newton.debug(world)` | Print stats to stdout |
| `Newton.stats(world)` | Return stats map: `frame`, `bodies`, `dynamic`, `static`, `sleeping`, `contacts`, `joints` |

## Demo

```
.\build\sapphire.exe plugins\newton\versions\v1.0.0\files\demo.sp
```

Controls in the demo:
- **Space** — spawn a rubber ball
- **W/S** — push the spring-pinned metal ball up/down
- **Escape** — exit
