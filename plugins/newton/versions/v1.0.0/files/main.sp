// Newton - Advanced 2D Physics Engine written entirely in Sapphire
// Version: 1.0.0
// Author: Sapphire Team
// Depends on: infinitum@"1.0.0"
//
// Architecture:
//   1.  Math Helpers
//   2.  Material Presets
//   3.  Rigid Bodies (circle, box)
//   4.  World + Spatial Grid (broad-phase)
//   5.  Narrow-Phase Collision Detection (circle-circle, box-box SAT, circle-box)
//   6.  Impulse Resolution (Coulomb friction + positional correction)
//   7.  Joints (DistanceJoint / PinJoint)
//   8.  Sleep System
//   9.  Velocity Verlet Integration
//  10.  Rendering (drawRect / drawCircle approximation via SFML globals)
//  11.  Raycast
//  12.  Public API: Newton object

import infinitum@"1.0.0";

// ===========================================================================
// 1. MATH HELPERS
// ===========================================================================

// 2D cross product (returns scalar: ax*by - ay*bx)
function nw_cross2d(a, b) {
    return (a[0] * b[1]) - (a[1] * b[0]);
}

// Scalar x Vec2 cross: produces a Vec2 perpendicular to v scaled by s
function nw_cross_sv(s, v) {
    return [-s * v[1], s * v[0]];
}

// Scalar absolute value
function nw_abs(x) {
    if (x < 0.0) return -x;
    return x;
}

// Clamp scalar between lo and hi
function nw_clamp(x, lo, hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

// Vec2 length squared
function nw_len_sq(v) {
    return (v[0] * v[0]) + (v[1] * v[1]);
}

// Vec2 length
function nw_len(v) {
    return sqrt(nw_len_sq(v));
}

// Normalize Vec2 safely (returns [0,0] for near-zero vectors)
function nw_normalize(v) {
    var l = nw_len(v);
    if (l < 0.0000001) return [0.0, 0.0];
    return [v[0] / l, v[1] / l];
}

// Vec2 dot product
function nw_dot(a, b) {
    return (a[0] * b[0]) + (a[1] * b[1]);
}

// Vec2 addition
function nw_add(a, b) {
    return [a[0] + b[0], a[1] + b[1]];
}

// Vec2 subtraction
function nw_sub(a, b) {
    return [a[0] - b[0], a[1] - b[1]];
}

// Vec2 scale
function nw_scale(v, s) {
    return [v[0] * s, v[1] * s];
}

// Vec2 negate
function nw_neg(v) {
    return [-v[0], -v[1]];
}

// Euclidean distance between two points
function nw_dist(a, b) {
    return nw_len(nw_sub(b, a));
}

// Min of two scalars
function nw_min2(a, b) {
    if (a < b) return a;
    return b;
}

// Max of two scalars
function nw_max2(a, b) {
    if (a > b) return a;
    return b;
}

// ===========================================================================
// 2. MATERIAL PRESETS
// ===========================================================================

function nw_material(restitution, friction, density) {
    return { "restitution": restitution, "friction": friction, "density": density };
}

var NW_MAT_RUBBER  = nw_material(0.80, 0.90, 1.2);
var NW_MAT_WOOD    = nw_material(0.30, 0.50, 0.6);
var NW_MAT_METAL   = nw_material(0.10, 0.30, 7.8);
var NW_MAT_ICE     = nw_material(0.05, 0.02, 0.9);
var NW_MAT_STONE   = nw_material(0.15, 0.60, 2.5);
var NW_MAT_DEFAULT = nw_material(0.30, 0.40, 1.0);

// ===========================================================================
// 3. RIGID BODIES
// ===========================================================================

function nw_make_body(shape, x, y, opts) {
    var isStatic = opts["static"] ?? false;
    var mat      = opts["material"] ?? NW_MAT_DEFAULT;
    var mass     = opts["mass"]     ?? mat["density"];
    if (isStatic) mass = 0.0;
    var invMass = (mass > 0.0) ? (1.0 / mass) : 0.0;

    return {
        "shape":       shape,
        "position":    [x, y],
        "velocity":    [0.0, 0.0],
        "force":       [0.0, 0.0],
        "mass":        mass,
        "invMass":     invMass,
        "inertia":     0.0,
        "invInertia":  0.0,
        "angle":       opts["angle"]      ?? 0.0,
        "angularVel":  0.0,
        "torque":      0.0,
        "restitution": opts["restitution"] ?? mat["restitution"],
        "friction":    opts["friction"]    ?? mat["friction"],
        "linearDamp":  opts["linearDamp"]  ?? 0.001,
        "angularDamp": opts["angularDamp"] ?? 0.001,
        "static":      isStatic,
        "radius":      0.0,
        "halfW":       0.0,
        "halfH":       0.0,
        "tag":         opts["tag"]         ?? "",
        "layer":       opts["layer"]       ?? 0,
        "color":       opts["color"]       ?? [255.0, 255.0, 255.0],
        "sleepTimer":  0.0,
        "sleeping":    false,
        "userData":    opts["userData"]    ?? nil,
        "_remove":     false
    };
}

function newton_circle(x, y, radius, opts) {
    opts = opts ?? {};
    var body = nw_make_body("circle", x, y, opts);
    body["radius"] = radius;
    if (body["mass"] > 0.0) {
        body["inertia"]   = 0.5 * body["mass"] * radius * radius;
        body["invInertia"]= 1.0 / body["inertia"];
    }
    return body;
}

function newton_box(x, y, width, height, opts) {
    opts = opts ?? {};
    var body = nw_make_body("box", x, y, opts);
    body["halfW"] = width  / 2.0;
    body["halfH"] = height / 2.0;
    if (body["mass"] > 0.0) {
        body["inertia"]   = (body["mass"] / 12.0) * (width * width + height * height);
        body["invInertia"]= 1.0 / body["inertia"];
    }
    return body;
}

function newton_static_circle(x, y, radius, opts) {
    opts = opts ?? {};
    opts["static"] = true;
    return newton_circle(x, y, radius, opts);
}

function newton_static_box(x, y, width, height, opts) {
    opts = opts ?? {};
    opts["static"] = true;
    return newton_box(x, y, width, height, opts);
}

// ===========================================================================
// 4. JOINTS
// ===========================================================================

// Spring joint: maintains target distance between two bodies
function newton_distance_joint(bodyA, bodyB, targetDist, stiffness, damping) {
    return {
        "type":       "distance",
        "bodyA":      bodyA,
        "bodyB":      bodyB,
        "targetDist": targetDist ?? nw_dist(bodyA["position"], bodyB["position"]),
        "stiffness":  stiffness  ?? 150.0,
        "damping":    damping    ?? 5.0
    };
}

// Pin joint: anchors a body to a fixed world position
function newton_pin_joint(body, worldX, worldY, stiffness, damping) {
    return {
        "type":      "pin",
        "body":      body,
        "anchor":    [worldX, worldY],
        "stiffness": stiffness ?? 300.0,
        "damping":   damping   ?? 10.0
    };
}

function nw_solve_distance_joint(j, delta) {
    var a        = j["bodyA"];
    var b        = j["bodyB"];
    var dp       = nw_sub(b["position"], a["position"]);
    var dist     = nw_len(dp);
    if (dist < 0.0000001) return;
    var dir      = nw_scale(dp, 1.0 / dist);
    var diff     = dist - j["targetDist"];
    var relVel   = nw_dot(nw_sub(b["velocity"], a["velocity"]), dir);
    var fmag     = j["stiffness"] * diff + j["damping"] * relVel;
    var f        = nw_scale(dir, fmag);
    var totalIM  = a["invMass"] + b["invMass"];
    if (totalIM <= 0.0) return;
    if (!a["static"]) a["velocity"] = nw_add(a["velocity"], nw_scale(f,  a["invMass"] * delta));
    if (!b["static"]) b["velocity"] = nw_sub(b["velocity"], nw_scale(f,  b["invMass"] * delta));
}

function nw_solve_pin_joint(j, delta) {
    var body = j["body"];
    if (body["static"]) return;
    var dp   = nw_sub(j["anchor"], body["position"]);
    var dist = nw_len(dp);
    if (dist < 0.0000001) return;
    var dir  = nw_scale(dp, 1.0 / dist);
    var velD = nw_dot(body["velocity"], dir);
    var fmag = j["stiffness"] * dist - j["damping"] * velD;
    body["velocity"] = nw_add(body["velocity"],
        nw_scale(dir, fmag * body["invMass"] * delta));
}

// ===========================================================================
// 5. WORLD
// ===========================================================================

function newton_world(opts) {
    opts = opts ?? {};
    return {
        "gravity":        opts["gravity"]        ?? [0.0, 980.0],
        "bodies":         [],
        "joints":         [],
        "contacts":       [],
        "iterations":     opts["iterations"]     ?? 6,
        "subSteps":       opts["subSteps"]       ?? 1,
        "sleepThreshold": opts["sleepThreshold"] ?? 5.0,
        "sleepTime":      opts["sleepTime"]      ?? 0.5,
        "onCollision":    opts["onCollision"]    ?? nil,
        "layerMask":      opts["layerMask"]      ?? nil,
        "cellSize":       opts["cellSize"]       ?? 64.0,
        "grid":           {},
        "frameCount":     0,
        "contactCount":   0
    };
}

function newton_add(world, body) {
    world["bodies"][len(world["bodies"])] = body;
    return body;
}

function newton_add_joint(world, joint) {
    world["joints"][len(world["joints"])] = joint;
    return joint;
}

function newton_remove(world, body) {
    body["_remove"] = true;
}

function newton_apply_force(body, fx, fy) {
    if (body["static"] || body["sleeping"]) return;
    body["force"] = nw_add(body["force"], [fx, fy]);
}

function newton_apply_impulse(body, ix, iy) {
    if (body["static"]) return;
    body["sleeping"]   = false;
    body["sleepTimer"] = 0.0;
    body["velocity"]   = nw_add(body["velocity"], nw_scale([ix, iy], body["invMass"]));
}

function newton_apply_torque(body, torque) {
    if (body["static"] || body["sleeping"]) return;
    body["torque"] = body["torque"] + torque;
}

// ===========================================================================
// 6. BROAD PHASE — SPATIAL HASHING
// ===========================================================================

function nw_grid_key(cx, cy) {
    return valueToString(floor(cx)) + "," + valueToString(floor(cy));
}

function nw_body_cells(body, cellSize) {
    var px = body["position"][0];
    var py = body["position"][1];
    var r  = 0.0;
    if (body["shape"] == "circle") {
        r = body["radius"];
    } else {
        r = nw_max2(body["halfW"], body["halfH"]);
    }
    return [
        floor((px - r) / cellSize),
        floor((py - r) / cellSize),
        floor((px + r) / cellSize),
        floor((py + r) / cellSize)
    ];
}

function nw_build_grid(world) {
    var grid     = {};
    var bodies   = world["bodies"];
    var cellSize = world["cellSize"];
    var n        = len(bodies);
    var i = 0;
    while (i < n) {
        var body = bodies[i];
        if (!body["sleeping"]) {
            var cells = nw_body_cells(body, cellSize);
            var cx = cells[0];
            while (cx <= cells[2]) {
                var cy = cells[1];
                while (cy <= cells[3]) {
                    var key = nw_grid_key(cx, cy);
                    if (grid[key] == nil) { grid[key] = []; }
                    grid[key][len(grid[key])] = i;
                    cy = cy + 1;
                }
                cx = cx + 1;
            }
        }
        i = i + 1;
    }
    world["grid"] = grid;
}

// Returns a flat list of unique [i, j] pairs that share at least one cell
function nw_broad_phase(world) {
    var result   = [];
    var seen     = {};
    var grid     = world["grid"];
    var bodies   = world["bodies"];
    var cellSize = world["cellSize"];
    var n        = len(bodies);
    var i = 0;

    while (i < n) {
        var body = bodies[i];
        if (body["sleeping"]) { i = i + 1; continue; }
        var cells = nw_body_cells(body, cellSize);
        var cx = cells[0];
        while (cx <= cells[2]) {
            var cy = cells[1];
            while (cy <= cells[3]) {
                var key  = nw_grid_key(cx, cy);
                var cell = grid[key];
                if (cell != nil) {
                    var k = 0;
                    while (k < len(cell)) {
                        var j = cell[k];
                        if (j > i) {
                            var pk = valueToString(i) + "_" + valueToString(j);
                            if (seen[pk] == nil) {
                                seen[pk] = true;
                                result[len(result)] = [i, j];
                            }
                        }
                        k = k + 1;
                    }
                }
                cy = cy + 1;
            }
            cx = cx + 1;
        }
        i = i + 1;
    }
    return result;
}

// ===========================================================================
// 7. NARROW PHASE COLLISION DETECTION
// ===========================================================================

function nw_contact(a, b, normal, pen, ptx, pty) {
    return { "a": a, "b": b, "normal": normal, "penetration": pen, "point": [ptx, pty] };
}

function nw_circle_circle(a, b) {
    var d  = nw_sub(b["position"], a["position"]);
    var r  = a["radius"] + b["radius"];
    var d2 = nw_len_sq(d);
    if (d2 >= r * r) return nil;
    var dist = sqrt(d2);
    var n    = (dist > 0.0000001) ? nw_scale(d, 1.0 / dist) : [1.0, 0.0];
    return nw_contact(a, b, n, r - dist,
        a["position"][0] + n[0] * a["radius"],
        a["position"][1] + n[1] * a["radius"]);
}

function nw_box_box(a, b) {
    var d  = nw_sub(b["position"], a["position"]);
    var ox = a["halfW"] + b["halfW"] - nw_abs(d[0]);
    var oy = a["halfH"] + b["halfH"] - nw_abs(d[1]);
    if (ox <= 0.0 || oy <= 0.0) return nil;
    if (ox < oy) {
        var nx = (d[0] < 0.0) ? -1.0 : 1.0;
        return nw_contact(a, b, [nx, 0.0], ox,
            a["position"][0] + nx * a["halfW"], b["position"][1]);
    }
    var ny = (d[1] < 0.0) ? -1.0 : 1.0;
    return nw_contact(a, b, [0.0, ny], oy,
        b["position"][0], a["position"][1] + ny * a["halfH"]);
}

function nw_circle_box(circle, box) {
    var bpos = box["position"];
    var cpos = circle["position"];
    var hw   = box["halfW"];
    var hh   = box["halfH"];
    var cl   = [
        nw_clamp(cpos[0], bpos[0] - hw, bpos[0] + hw),
        nw_clamp(cpos[1], bpos[1] - hh, bpos[1] + hh)
    ];
    var d  = nw_sub(cpos, cl);
    var d2 = nw_len_sq(d);
    var r  = circle["radius"];
    if (d2 >= r * r) return nil;

    var dist = sqrt(d2);
    var n    = [0.0, 0.0];

    if (dist < 0.0000001) {
        // Circle center inside box — find minimum exit axis
        var dx  = cpos[0] - bpos[0];
        var dy  = cpos[1] - bpos[1];
        var ovX = hw - nw_abs(dx);
        var ovY = hh - nw_abs(dy);
        if (ovX < ovY) {
            n = [(dx < 0.0) ? -1.0 : 1.0, 0.0];
            return nw_contact(circle, box, n, ovX + r, cl[0], cl[1]);
        }
        n = [0.0, (dy < 0.0) ? -1.0 : 1.0];
        return nw_contact(circle, box, n, ovY + r, cl[0], cl[1]);
    }
    n = nw_scale(d, 1.0 / dist);
    return nw_contact(circle, box, n, r - dist, cl[0], cl[1]);
}

function nw_detect(a, b) {
    if (a["shape"] == "circle" && b["shape"] == "circle") return nw_circle_circle(a, b);
    if (a["shape"] == "box"    && b["shape"] == "box")    return nw_box_box(a, b);
    if (a["shape"] == "circle" && b["shape"] == "box")    return nw_circle_box(a, b);
    if (a["shape"] == "box"    && b["shape"] == "circle") {
        var c = nw_circle_box(b, a);
        if (c == nil) return nil;
        return nw_contact(a, b, nw_neg(c["normal"]), c["penetration"], c["point"][0], c["point"][1]);
    }
    return nil;
}

function nw_can_collide(world, a, b) {
    if (a["static"] && b["static"]) return false;
    var mask = world["layerMask"];
    if (mask == nil) return true;
    var la = valueToString(a["layer"]);
    var lb = valueToString(b["layer"]);
    if (mask[la] == nil) return true;
    if (mask[la][lb] == nil) return true;
    return mask[la][lb];
}

// ===========================================================================
// 8. IMPULSE RESOLUTION  (Coulomb friction + Baumgarte positional correction)
// ===========================================================================

function nw_resolve(contact) {
    var a   = contact["a"];
    var b   = contact["b"];
    var n   = contact["normal"];
    var pen = contact["penetration"];
    var pt  = contact["point"];

    var totalIM = a["invMass"] + b["invMass"];
    if (totalIM <= 0.0) return;

    // Radius vectors from centers to contact point
    var rA = nw_sub(pt, a["position"]);
    var rB = nw_sub(pt, b["position"]);

    // Relative velocity at contact (includes angular contribution)
    var vA = nw_add(a["velocity"], nw_cross_sv(a["angularVel"], rA));
    var vB = nw_add(b["velocity"], nw_cross_sv(b["angularVel"], rB));
    var relVel = nw_sub(vB, vA);
    var velN   = nw_dot(relVel, n);

    // Baumgarte positional correction (always applied)
    var slop    = 0.005;
    var pct     = 0.4;
    var corrMag = nw_max2(pen - slop, 0.0) / totalIM * pct;
    var corr    = nw_scale(n, corrMag);
    if (!a["static"]) a["position"] = nw_sub(a["position"], nw_scale(corr, a["invMass"]));
    if (!b["static"]) b["position"] = nw_add(b["position"], nw_scale(corr, b["invMass"]));

    // Bodies already separating — no impulse
    if (velN > 0.0) return;

    var e = nw_min2(a["restitution"], b["restitution"]);

    // Effective mass for normal
    var rACN = nw_cross2d(rA, n);
    var rBCN = nw_cross2d(rB, n);
    var effM = totalIM
        + rACN * rACN * a["invInertia"]
        + rBCN * rBCN * b["invInertia"];
    if (effM <= 0.0) return;

    var jN  = -(1.0 + e) * velN / effM;
    var imp = nw_scale(n, jN);

    if (!a["static"]) {
        a["velocity"]   = nw_sub(a["velocity"],   nw_scale(imp, a["invMass"]));
        a["angularVel"] = a["angularVel"] - a["invInertia"] * nw_cross2d(rA, imp);
    }
    if (!b["static"]) {
        b["velocity"]   = nw_add(b["velocity"],   nw_scale(imp, b["invMass"]));
        b["angularVel"] = b["angularVel"] + b["invInertia"] * nw_cross2d(rB, imp);
    }

    // --- Coulomb Friction ---
    // Recompute relative velocity after normal impulse
    var vA2    = nw_add(a["velocity"], nw_cross_sv(a["angularVel"], rA));
    var vB2    = nw_add(b["velocity"], nw_cross_sv(b["angularVel"], rB));
    var rv2    = nw_sub(vB2, vA2);
    var velN2  = nw_dot(rv2, n);
    var velT   = nw_sub(rv2, nw_scale(n, velN2));
    var vtLen  = nw_len(velT);
    if (vtLen < 0.0001) return;

    var tangent = nw_scale(velT, 1.0 / vtLen);
    var mu      = (a["friction"] + b["friction"]) * 0.5;
    var rACT    = nw_cross2d(rA, tangent);
    var rBCT    = nw_cross2d(rB, tangent);
    var effMT   = totalIM
        + rACT * rACT * a["invInertia"]
        + rBCT * rBCT * b["invInertia"];
    if (effMT <= 0.0) return;

    var jT = -vtLen / effMT;
    // Clamp to Coulomb cone
    var maxF = mu * nw_abs(jN);
    if (jT < -maxF) jT = -maxF;
    if (jT >  maxF) jT =  maxF;

    var fImp = nw_scale(tangent, jT);
    if (!a["static"]) {
        a["velocity"]   = nw_sub(a["velocity"],   nw_scale(fImp, a["invMass"]));
        a["angularVel"] = a["angularVel"] - a["invInertia"] * nw_cross2d(rA, fImp);
    }
    if (!b["static"]) {
        b["velocity"]   = nw_add(b["velocity"],   nw_scale(fImp, b["invMass"]));
        b["angularVel"] = b["angularVel"] + b["invInertia"] * nw_cross2d(rB, fImp);
    }
}

// ===========================================================================
// 9. SLEEP SYSTEM
// ===========================================================================

function nw_update_sleep(body, threshold, sleepTime, delta) {
    if (body["static"]) return;
    var spd2  = nw_len_sq(body["velocity"]);
    var aSpd2 = body["angularVel"] * body["angularVel"];
    var t2    = threshold * threshold;

    if (spd2 < t2 && aSpd2 < t2 * 0.1) {
        body["sleepTimer"] = body["sleepTimer"] + delta;
        if (body["sleepTimer"] > sleepTime) {
            body["sleeping"]   = true;
            body["velocity"]   = [0.0, 0.0];
            body["angularVel"] = 0.0;
            body["force"]      = [0.0, 0.0];
            body["torque"]     = 0.0;
        }
    } else {
        body["sleepTimer"] = 0.0;
        body["sleeping"]   = false;
    }
}

function newton_wake_near(world, x, y, radius) {
    var bodies = world["bodies"];
    var i = 0;
    while (i < len(bodies)) {
        var body = bodies[i];
        if (body["sleeping"] && nw_dist(body["position"], [x, y]) < radius) {
            body["sleeping"]   = false;
            body["sleepTimer"] = 0.0;
        }
        i = i + 1;
    }
}

// ===========================================================================
// 10. INTEGRATION — Semi-implicit Euler (Velocity Verlet style)
// ===========================================================================

function nw_integrate(body, gravity, delta) {
    if (body["static"] || body["sleeping"]) return;

    // Damping factors (approximate exponential decay)
    var ldF = 1.0 - nw_min2(body["linearDamp"]  * delta, 0.999);
    var adF = 1.0 - nw_min2(body["angularDamp"] * delta, 0.999);

    // Compute acceleration: gravity + user forces
    var gravForce  = nw_scale(gravity, body["mass"]);
    var totalForce = nw_add(body["force"], gravForce);
    var accel      = nw_scale(totalForce, body["invMass"]);
    var alpha      = body["torque"] * body["invInertia"];

    // Integrate velocity then position
    body["velocity"]   = nw_scale(nw_add(body["velocity"],   nw_scale(accel, delta)), ldF);
    body["angularVel"] = (body["angularVel"] + alpha * delta) * adF;
    body["position"]   = nw_add(body["position"], nw_scale(body["velocity"], delta));
    body["angle"]      = body["angle"] + body["angularVel"] * delta;

    // Reset accumulators for next frame
    body["force"]  = [0.0, 0.0];
    body["torque"] = 0.0;
}

// ===========================================================================
// 11. WORLD STEP
// ===========================================================================

function newton_step(world, delta) {
    var bodies   = world["bodies"];
    var joints   = world["joints"];
    var iters    = world["iterations"];
    var subSteps = world["subSteps"];
    var subDT    = delta / subSteps;

    // --- Sweep removed bodies ---
    var alive = [];
    var bi = 0;
    while (bi < len(bodies)) {
        if (!bodies[bi]["_remove"]) alive[len(alive)] = bodies[bi];
        bi = bi + 1;
    }
    world["bodies"] = alive;
    bodies = alive;

    var sub = 0;
    while (sub < subSteps) {
        // Integration
        var i = 0;
        while (i < len(bodies)) { nw_integrate(bodies[i], world["gravity"], subDT); i = i + 1; }

        // Broad phase
        nw_build_grid(world);
        var pairs = nw_broad_phase(world);

        // Narrow phase
        var contacts = [];
        var pi = 0;
        while (pi < len(pairs)) {
            var pair = pairs[pi];
            var a    = bodies[pair[0]];
            var b    = bodies[pair[1]];
            if (nw_can_collide(world, a, b)) {
                var c = nw_detect(a, b);
                if (c != nil) {
                    contacts[len(contacts)] = c;
                    if (a["sleeping"]) { a["sleeping"] = false; a["sleepTimer"] = 0.0; }
                    if (b["sleeping"]) { b["sleeping"] = false; b["sleepTimer"] = 0.0; }
                }
            }
            pi = pi + 1;
        }
        world["contacts"]     = contacts;
        world["contactCount"] = len(contacts);

        // Iterative resolution
        var iter = 0;
        while (iter < iters) {
            var ci = 0;
            while (ci < len(contacts)) { nw_resolve(contacts[ci]); ci = ci + 1; }
            iter = iter + 1;
        }

        // Solve joints
        var ji = 0;
        while (ji < len(joints)) {
            var j = joints[ji];
            if (j["type"] == "distance") nw_solve_distance_joint(j, subDT);
            if (j["type"] == "pin")      nw_solve_pin_joint(j, subDT);
            ji = ji + 1;
        }

        sub = sub + 1;
    }

    // Sleep update (once per full step)
    var si = 0;
    while (si < len(bodies)) {
        nw_update_sleep(bodies[si], world["sleepThreshold"], world["sleepTime"], delta);
        si = si + 1;
    }

    // Collision callbacks
    if (world["onCollision"] != nil) {
        var ci = 0;
        while (ci < len(world["contacts"])) {
            var c = world["contacts"][ci];
            world["onCollision"](c["a"], c["b"], c);
            ci = ci + 1;
        }
    }

    world["frameCount"] = world["frameCount"] + 1;
}

// ===========================================================================
// 12. RENDERING  (uses Sapphire native graphics globals)
// ===========================================================================

// Draw a filled rectangle centered at (cx, cy)
function nw_draw_box(cx, cy, hw, hh) {
    drawRect(cx - hw, cy - hh, hw * 2.0, hh * 2.0);
}

// Approximate a circle using a square (no native drawCircle in Sapphire v1.0.x)
function nw_draw_circle(cx, cy, r) {
    drawRect(cx - r, cy - r, r * 2.0, r * 2.0);
}

// Full world render — call between clear() and display()
// opts keys: "showContacts" (bool), "showSleep" (bool)
function newton_render(world, opts) {
    opts = opts ?? {};
    var showContacts = opts["showContacts"] ?? false;
    var bodies       = world["bodies"];
    var i = 0;
    while (i < len(bodies)) {
        var body = bodies[i];
        var px   = body["position"][0];
        var py   = body["position"][1];
        if (body["shape"] == "circle") nw_draw_circle(px, py, body["radius"]);
        if (body["shape"] == "box")    nw_draw_box(px, py, body["halfW"], body["halfH"]);
        i = i + 1;
    }
    if (showContacts) {
        var contacts = world["contacts"];
        var ci = 0;
        while (ci < len(contacts)) {
            var c = contacts[ci];
            drawRect(c["point"][0] - 2.0, c["point"][1] - 2.0, 4.0, 4.0);
            ci = ci + 1;
        }
    }
}

// ===========================================================================
// 13. RAYCAST
// ===========================================================================

function newton_raycast(world, ox, oy, dx, dy, maxDist) {
    var bodies  = world["bodies"];
    var bestT   = maxDist;
    var bestHit = nil;
    var i = 0;
    while (i < len(bodies)) {
        var body = bodies[i];
        var hit  = false;
        var t    = maxDist;

        if (body["shape"] == "circle") {
            var fx   = ox - body["position"][0];
            var fy   = oy - body["position"][1];
            var a2   = dx * dx + dy * dy;
            var b2   = 2.0 * (fx * dx + fy * dy);
            var c2   = fx * fx + fy * fy - body["radius"] * body["radius"];
            var disc = b2 * b2 - 4.0 * a2 * c2;
            if (disc >= 0.0) {
                var sq = sqrt(disc);
                var t1 = (-b2 - sq) / (2.0 * a2);
                var t2 = (-b2 + sq) / (2.0 * a2);
                if (t1 > 0.0001 && t1 < bestT) { t = t1; hit = true; }
                else if (t2 > 0.0001 && t2 < bestT) { t = t2; hit = true; }
            }
        }

        if (body["shape"] == "box") {
            var idx = (nw_abs(dx) > 0.0001) ? (1.0 / dx) : 99999999.0;
            var idy = (nw_abs(dy) > 0.0001) ? (1.0 / dy) : 99999999.0;
            var tx1 = (body["position"][0] - body["halfW"] - ox) * idx;
            var tx2 = (body["position"][0] + body["halfW"] - ox) * idx;
            var ty1 = (body["position"][1] - body["halfH"] - oy) * idy;
            var ty2 = (body["position"][1] + body["halfH"] - oy) * idy;
            var tmin = nw_max2(nw_min2(tx1, tx2), nw_min2(ty1, ty2));
            var tmax = nw_min2(nw_max2(tx1, tx2), nw_max2(ty1, ty2));
            if (tmax >= tmin && tmin > 0.0001 && tmin < bestT) { t = tmin; hit = true; }
        }

        if (hit) {
            bestT   = t;
            bestHit = {
                "body":  body,
                "t":     t,
                "point": [ox + dx * t, oy + dy * t]
            };
        }
        i = i + 1;
    }
    return bestHit;
}

// ===========================================================================
// 14. DEBUG + STATS
// ===========================================================================

function newton_debug(world) {
    var bodies   = world["bodies"];
    var sleeping = 0;
    var i = 0;
    while (i < len(bodies)) {
        if (bodies[i]["sleeping"]) sleeping = sleeping + 1;
        i = i + 1;
    }
    print("[Newton] frame=" + valueToString(world["frameCount"])
        + " bodies=" + valueToString(len(bodies))
        + " sleeping=" + valueToString(sleeping)
        + " contacts=" + valueToString(world["contactCount"])
        + " joints=" + valueToString(len(world["joints"])) + "\n");
}

function newton_stats(world) {
    var bodies   = world["bodies"];
    var sleeping = 0;
    var dynamic  = 0;
    var staticN  = 0;
    var i = 0;
    while (i < len(bodies)) {
        if (bodies[i]["sleeping"]) sleeping = sleeping + 1;
        if (bodies[i]["static"])   staticN  = staticN  + 1;
        else                       dynamic  = dynamic  + 1;
        i = i + 1;
    }
    return {
        "frame":    world["frameCount"],
        "bodies":   len(bodies),
        "dynamic":  dynamic,
        "static":   staticN,
        "sleeping": sleeping,
        "contacts": world["contactCount"],
        "joints":   len(world["joints"])
    };
}

// ===========================================================================
// 15. PUBLIC API
// ===========================================================================

var Newton = {
    // --- Body constructors ---
    "circle":        newton_circle,
    "box":           newton_box,
    "staticCircle":  newton_static_circle,
    "staticBox":     newton_static_box,

    // --- Materials ---
    "RUBBER": NW_MAT_RUBBER,
    "WOOD":   NW_MAT_WOOD,
    "METAL":  NW_MAT_METAL,
    "ICE":    NW_MAT_ICE,
    "STONE":  NW_MAT_STONE,

    // --- World management ---
    "world":        newton_world,
    "add":          newton_add,
    "remove":       newton_remove,
    "addJoint":     newton_add_joint,
    "step":         newton_step,

    // --- Forces & impulses ---
    "applyForce":   newton_apply_force,
    "applyImpulse": newton_apply_impulse,
    "applyTorque":  newton_apply_torque,

    // --- Joints ---
    "distanceJoint": newton_distance_joint,
    "pinJoint":      newton_pin_joint,

    // --- Queries ---
    "raycast":  newton_raycast,
    "wakeNear": newton_wake_near,

    // --- Rendering ---
    "render":  newton_render,

    // --- Diagnostics ---
    "debug": newton_debug,
    "stats": newton_stats,

    // --- Math helpers (public) ---
    "vec2":      nw_add,
    "length":    nw_len,
    "normalize": nw_normalize,
    "dot":       nw_dot,
    "distance":  nw_dist
};
