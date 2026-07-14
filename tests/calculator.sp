var UI = 1

// iOS Calculator style
Style("CalcBg",    bgColor="#000000", textColor="#ffffff", accentColor="#000000", borderThickness=0.0, hoverColor="#000000", borderRadius=0.0,  fontAlias="default", fontSize=32)
Style("DisplayBg", bgColor="#000000", textColor="#ffffff", accentColor="#000000", borderThickness=0.0, hoverColor="#000000", borderRadius=0.0,  fontAlias="default", fontSize=64)
Style("DarkBtn",   bgColor="#333333", textColor="#ffffff", accentColor="#404040", borderThickness=0.0, hoverColor="#4d4d4d", borderRadius=40.0, fontAlias="default", fontSize=32)
Style("LightBtn",  bgColor="#a6a6a6", textColor="#000000", accentColor="#b3b3b3", borderThickness=0.0, hoverColor="#cccccc", borderRadius=40.0, fontAlias="default", fontSize=32)
Style("OpBtn",     bgColor="#ff9f0a", textColor="#ffffff", accentColor="#ffb340", borderThickness=0.0, hoverColor="#ffc266", borderRadius=40.0, fontAlias="default", fontSize=32)

// Global calculator state
var currentInput = "0"
var previousInput = "0"
var op = ""
var shouldResetInput = false
var config_window_height = 700

class Calculator {
    function handleNumber(string num) void {
        if (shouldResetInput) { currentInput = num; shouldResetInput = false }
        else { if (currentInput == "0" && num != ".") currentInput = num; else currentInput += num }
    }
    function handleOp(string newOp) void {
        if (op != "" && !shouldResetInput) this.calculateResult()
        previousInput = currentInput
        op = newOp
        shouldResetInput = true
    }
    function calculateResult() void {
        if (op == "") return
        if (op == "+") { currentInput = previousInput + currentInput }
        else if (op == "-") { currentInput = previousInput - currentInput }
        else if (op == "*") { currentInput = previousInput * currentInput }
        else if (op == "/") {
            if (currentInput != "0") {
                currentInput = previousInput / currentInput
            } else {
                currentInput = "Error"
            }
        }
        op = ""
        shouldResetInput = true
    }
    function handleClear() void { currentInput = "0"; previousInput = "0"; op = "" }
    function handleSign() void { currentInput *= -1.0 }
    function handlePercent() void { currentInput /= 100.0 }
    function handleDiv() void { this.handleOp("/") }
    function handleMul() void { this.handleOp("*") }
    function handleSub() void { this.handleOp("-") }
    function handleAdd() void { this.handleOp("+") }

    macro NUM_HANDLER(name, val) {
        function handleNum##name() void { this.handleNumber("val") }
    }
    
    NUM_HANDLER(7, 7)
    NUM_HANDLER(8, 8)
    NUM_HANDLER(9, 9)
    NUM_HANDLER(4, 4)
    NUM_HANDLER(5, 5)
    NUM_HANDLER(6, 6)
    NUM_HANDLER(1, 1)
    NUM_HANDLER(2, 2)
    NUM_HANDLER(3, 3)
    NUM_HANDLER(0, 0)
    NUM_HANDLER(Dot, .)
}

var calc = Calculator()

function updateUI() bool {
    var app = Flex(
        direction="column",
        justify="flex-start",
        align="center",
        gap=15.0,
        style="CalcBg",
        children=[
            Flex(
                direction="row",
                justify="center",
                align="center",
                style="DisplayBg",
                width=350.0,
                height=140.0,
                children=[ Text(currentInput, size=72) ]
            ),
            Flex(
                direction="column",
                gap=10.0,
                children=[
                    Flex(direction="row", gap=10.0, children=[
                        Button(label="AC",  width=80.0, height=80.0, style="LightBtn", align="center", onClick=calc.handleClear),
                        Button(label="+/-", width=80.0, height=80.0, style="LightBtn", align="center", onClick=calc.handleSign),
                        Button(label="%",   width=80.0, height=80.0, style="LightBtn", align="center", onClick=calc.handlePercent),
                        Button(label="/",   width=80.0, height=80.0, style="OpBtn",    align="center", onClick=calc.handleDiv)
                    ]),
                    Flex(direction="row", gap=10.0, children=[
                        Button(label="7", width=80.0, height=80.0, style="DarkBtn", align="center", onClick=calc.handleNum7),
                        Button(label="8", width=80.0, height=80.0, style="DarkBtn", align="center", onClick=calc.handleNum8),
                        Button(label="9", width=80.0, height=80.0, style="DarkBtn", align="center", onClick=calc.handleNum9),
                        Button(label="*", width=80.0, height=80.0, style="OpBtn",   align="center", onClick=calc.handleMul)
                    ]),
                    Flex(direction="row", gap=10.0, children=[
                        Button(label="4", width=80.0, height=80.0, style="DarkBtn", align="center", onClick=calc.handleNum4),
                        Button(label="5", width=80.0, height=80.0, style="DarkBtn", align="center", onClick=calc.handleNum5),
                        Button(label="6", width=80.0, height=80.0, style="DarkBtn", align="center", onClick=calc.handleNum6),
                        Button(label="-", width=80.0, height=80.0, style="OpBtn",   align="center", onClick=calc.handleSub)
                    ]),
                    Flex(direction="row", gap=10.0, children=[
                        Button(label="1", width=80.0, height=80.0, style="DarkBtn", align="center", onClick=calc.handleNum1),
                        Button(label="2", width=80.0, height=80.0, style="DarkBtn", align="center", onClick=calc.handleNum2),
                        Button(label="3", width=80.0, height=80.0, style="DarkBtn", align="center", onClick=calc.handleNum3),
                        Button(label="+", width=80.0, height=80.0, style="OpBtn",   align="center", onClick=calc.handleAdd)
                    ]),
                    Flex(direction="row", gap=10.0, children=[
                        Button(label="0",   width=170.0, height=80.0, style="DarkBtn", align="center", onClick=calc.handleNum0),
                        Button(label=".",   width=80.0,  height=80.0, style="DarkBtn", align="center", onClick=calc.handleNumDot),
                        Button(label="=",   width=80.0,  height=80.0, style="OpBtn",   align="center", onClick=calc.calculateResult)
                    ])
                ]
            )
        ]
    )
    var event = Render(app)
    if (event != nil) { event() }
    
    return true
}
