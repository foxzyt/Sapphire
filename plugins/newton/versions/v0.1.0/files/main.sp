// Newton - a lightweight 2D physics engine written entirely in Sapphire.
// Newton uses Infinitum's vector operations and does not require native APIs.
import infinitum@"1.0.0";

function newton_number(value, fallback) {
    return value ?? fallback;
}

function newton_vec2(x, y) {
    return [x, y];
}

function newton_length(vector) {
    return sqrt(dot(vector, vector));
}

function newton_normalize(vector) {
    var length = newton_length(vector);
    if (length <= 0.000001) return [0.0, 0.0];
    return scale(vector, 1.0 / length);
}

function newton_clamp(value, low, high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

function newton_body(shape, x, y, options) {
    var isStatic = newton_number(options["static"], false);
    var mass = newton_number(options["mass"], 1.0);
    if (isStatic || mass <= 0.0) mass = 0.0;

    return {
        "shape": shape,
        "position": [x, y],
        "velocity": newton_vec2(0.0, 0.0),
        "force": newton_vec2(0.0, 0.0),
        "mass": mass,
        "invMass": mass > 0.0 ? 1.0 / mass : 0.0,
        "static": isStatic,
        "restitution": newton_number(options["restitution"], 0.2),
        "friction": newton_number(options["friction"], 0.2),
        "tag": newton_number(options["tag"], ""),
        "radius": 0.0,
        "half": newton_vec2(0.0, 0.0)
    };
}

function newton_circle(x, y, radius, options) {
    var body = newton_body("circle", x, y, options ?? {});
    body["radius"] = radius;
    return body;
}

function newton_box(x, y, width, height, options) {
    var body = newton_body("box", x, y, options ?? {});
    body["half"] = [width / 2.0, height / 2.0];
    return body;
}

function newton_static_box(x, y, width, height, options) {
    options = options ?? {};
    options["static"] = true;
    return newton_box(x, y, width, height, options);
}

function newton_world(options) {
    options = options ?? {};
    return {
        "gravity": newton_number(options["gravity"], [0.0, 980.0]),
        "bodies": [],
        "contacts": [],
        "iterations": newton_number(options["iterations"], 4.0),
        "onCollision": newton_number(options["onCollision"], nil)
    };
}

function newton_add(world, body) {
    world["bodies"][len(world["bodies"])] = body;
    return body;
}

function newton_apply_force(body, force) {
    if (!body["static"]) body["force"] = add(body["force"], force);
}

function newton_apply_impulse(body, impulse) {
    if (!body["static"]) body["velocity"] = add(body["velocity"], scale(impulse, body["invMass"]));
}

function newton_integrate(body, gravity, delta) {
    if (body["static"]) return;
    var acceleration = add(gravity, scale(body["force"], body["invMass"]));
    body["velocity"] = add(body["velocity"], scale(acceleration, delta));
    body["position"] = add(body["position"], scale(body["velocity"], delta));
    body["force"] = [0.0, 0.0];
}

function newton_contact(a, b, normal, penetration) {
    return { "a": a, "b": b, "normal": normal, "penetration": penetration };
}

function newton_circle_circle(a, b) {
    var delta = sub(b["position"], a["position"]);
    var distanceSquared = dot(delta, delta);
    var radius = a["radius"] + b["radius"];
    if (distanceSquared >= radius * radius) return nil;

    var distance = sqrt(distanceSquared);
    var normal = distance > 0.000001 ? scale(delta, 1.0 / distance) : [1.0, 0.0];
    return newton_contact(a, b, normal, radius - distance);
}

function newton_box_box(a, b) {
    var delta = sub(b["position"], a["position"]);
    var overlapX = a["half"][0] + b["half"][0] - (delta[0] < 0.0 ? -delta[0] : delta[0]);
    var overlapY = a["half"][1] + b["half"][1] - (delta[1] < 0.0 ? -delta[1] : delta[1]);
    if (overlapX <= 0.0 || overlapY <= 0.0) return nil;

    if (overlapX < overlapY) {
        return newton_contact(a, b, [delta[0] < 0.0 ? -1.0 : 1.0, 0.0], overlapX);
    }
    return newton_contact(a, b, [0.0, delta[1] < 0.0 ? -1.0 : 1.0], overlapY);
}

function newton_circle_box(circle, box) {
    var boxPosition = box["position"];
    var half = box["half"];
    var circlePosition = circle["position"];
    var closest = [
        newton_clamp(circlePosition[0], boxPosition[0] - half[0], boxPosition[0] + half[0]),
        newton_clamp(circlePosition[1], boxPosition[1] - half[1], boxPosition[1] + half[1])
    ];
    var delta = sub(circlePosition, closest);
    var distanceSquared = dot(delta, delta);
    if (distanceSquared >= circle["radius"] * circle["radius"]) return nil;

    var distance = sqrt(distanceSquared);
    var normal = distance > 0.000001 ? scale(delta, -1.0 / distance) : [0.0, -1.0];
    return newton_contact(circle, box, normal, circle["radius"] - distance);
}

function newton_detect(a, b) {
    if (a["shape"] == "circle" && b["shape"] == "circle") return newton_circle_circle(a, b);
    if (a["shape"] == "box" && b["shape"] == "box") return newton_box_box(a, b);
    if (a["shape"] == "circle" && b["shape"] == "box") return newton_circle_box(a, b);
    if (a["shape"] == "box" && b["shape"] == "circle") {
        var contact = newton_circle_box(b, a);
        if (contact == nil) return nil;
        return newton_contact(a, b, scale(contact["normal"], -1.0), contact["penetration"]);
    }
    return nil;
}

function newton_resolve(contact) {
    var a = contact["a"];
    var b = contact["b"];
    var inverseMass = a["invMass"] + b["invMass"];
    if (inverseMass <= 0.0) return;

    var normal = contact["normal"];
    var relativeVelocity = sub(b["velocity"], a["velocity"]);
    var velocityAlongNormal = dot(relativeVelocity, normal);

    if (velocityAlongNormal < 0.0) {
        var restitution = a["restitution"] < b["restitution"] ? a["restitution"] : b["restitution"];
        var impulseMagnitude = -(1.0 + restitution) * velocityAlongNormal / inverseMass;
        var impulse = scale(normal, impulseMagnitude);
        if (!a["static"]) a["velocity"] = sub(a["velocity"], scale(impulse, a["invMass"]));
        if (!b["static"]) b["velocity"] = add(b["velocity"], scale(impulse, b["invMass"]));
    }

    var correctionMagnitude = (contact["penetration"] - 0.01) / inverseMass * 0.8;
    if (correctionMagnitude > 0.0) {
        var correction = scale(normal, correctionMagnitude);
        if (!a["static"]) a["position"] = sub(a["position"], scale(correction, a["invMass"]));
        if (!b["static"]) b["position"] = add(b["position"], scale(correction, b["invMass"]));
    }
}

function newton_step(world, delta) {
    var bodies = world["bodies"];
    var i = 0;
    while (i < len(bodies)) {
        newton_integrate(bodies[i], world["gravity"], delta);
        i = i + 1;
    }

    world["contacts"] = [];
    i = 0;
    while (i < len(bodies)) {
        var j = i + 1;
        while (j < len(bodies)) {
            var contact = newton_detect(bodies[i], bodies[j]);
            if (contact != nil) world["contacts"][len(world["contacts"])] = contact;
            j = j + 1;
        }
        i = i + 1;
    }

    var iteration = 0;
    while (iteration < world["iterations"]) {
        i = 0;
        while (i < len(world["contacts"])) {
            newton_resolve(world["contacts"][i]);
            i = i + 1;
        }
        iteration = iteration + 1;
    }

    if (world["onCollision"] != nil) {
        i = 0;
        while (i < len(world["contacts"])) {
            var contact = world["contacts"][i];
            world["onCollision"](contact["a"], contact["b"], contact);
            i = i + 1;
        }
    }
}

var Newton = {
    "vec2": newton_vec2,
    "length": newton_length,
    "normalize": newton_normalize,
    "world": newton_world,
    "circle": newton_circle,
    "box": newton_box,
    "staticBox": newton_static_box,
    "add": newton_add,
    "applyForce": newton_apply_force,
    "applyImpulse": newton_apply_impulse,
    "step": newton_step
};
