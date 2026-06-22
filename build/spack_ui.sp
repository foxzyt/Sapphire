var UI = 1

// Spack GUI - Built with Sapphire Script!
const config_window_width = 2000
const config_window_height = 1600

Style("MainBg", bgColor="#1e1e1e", textColor="#c8c8c8", fontAlias="default", fontSize=36)
Style("HeaderBg", bgColor="#1e1e1e", textColor="#ffffff", fontAlias="default", fontSize=56)
Style("InputStyle", bgColor="#323232", textColor="#ffffff", hoverColor="#323232", borderColor="#646464", accentColor="#646464", borderThickness=2.0, borderRadius=0.0, fontAlias="default", fontSize=36)
Style("BtnStyle", bgColor="#464646", textColor="#ffffff", hoverColor="#555555", borderThickness=0.0, borderRadius=0.0, fontAlias="default", fontSize=32)
Style("PackBtnStyle", bgColor="#007acc", textColor="#ffffff", hoverColor="#0098ff", borderThickness=0.0, borderRadius=0.0, fontAlias="default", fontSize=40)
Style("StatusStyle", bgColor="#1e1e1e", textColor="#969696", fontAlias="default", fontSize=36)
Style("StatusSuccess", bgColor="#1e1e1e", textColor="#00ff00", fontAlias="default", fontSize=36)
Style("StatusError", bgColor="#1e1e1e", textColor="#ff0000", fontAlias="default", fontSize=36)
Style("CheckStyle", bgColor="#323232", textColor="#c8c8c8", hoverColor="#464646", accentColor="#007acc", borderThickness=2.0, borderRadius=0.0, fontAlias="default", fontSize=36)

var entryFile = "main.sp"
var outputFile = "app.exe"
var author = "Spack User"
var version = "1.0.0"
var iconPath = ""
var noConsole = false
var optimize = true
var softMode = false
var requireAdmin = false
var statusMessage = "Ready."
var currentStatusStyle = "StatusStyle"

class SpackApp {
    function pack() void {
        if (entryFile == "") {
            statusMessage = "Error: Entry File is empty."
            currentStatusStyle = "StatusError"
            return
        }

        var nl = "
"
        var conf = "EntryFile=" + entryFile + nl
        conf += "OutputFile=" + outputFile + nl
        conf += "Author=" + author + nl
        conf += "Version=" + version + nl
        conf += "IconPath=" + iconPath + nl
        if (noConsole) { conf += "NoConsole=true" + nl } else { conf += "NoConsole=false" + nl }
        if (optimize) { conf += "Optimize=true" + nl } else { conf += "Optimize=false" + nl }
        if (softMode) { conf += "SoftMode=true" + nl } else { conf += "SoftMode=false" + nl }
        if (requireAdmin) { conf += "RequireAdmin=true" + nl } else { conf += "RequireAdmin=false" + nl }

        writeFile("SpackConfig.txt", conf)
        
        var result = exec("spack")
        
        if (result == 0) {
            statusMessage = "Success! Created " + outputFile
            currentStatusStyle = "StatusSuccess"
        } else {
            statusMessage = "Error: spack failed."
            currentStatusStyle = "StatusError"
        }
    }

    function onEntryChanged() void { entryFile = GetInputText("entryInput") }
    function onOutputChanged() void { outputFile = GetInputText("outputInput") }
    function onAuthorChanged() void { author = GetInputText("authorInput") }
    function onVersionChanged() void { version = GetInputText("versionInput") }
    function onIconChanged() void { iconPath = GetInputText("iconInput") }

    function onEntryBrowse() void { entryFile = openFileDialog() }
    function onOutputBrowse() void { outputFile = openFileDialog() }
    function onIconBrowse() void { iconPath = openFileDialog() }

    function onNoConsoleToggle() void { noConsole = !noConsole }
    function onOptimizeToggle() void { optimize = !optimize }
    function onSoftModeToggle() void { softMode = !softMode }
    function onAdminToggle() void { requireAdmin = !requireAdmin }
}

var app = SpackApp()

function updateUI() bool {
    var layout = Flex(direction="column", gap=0.0, style="MainBg", children=[
        Flex(height=40.0),
        
        Flex(direction="row", gap=0.0, children=[
            Flex(width=100.0),
            
            Flex(direction="column", gap=0.0, align="flex-start", children=[
                Text(text="Spack Native Packager", style="HeaderBg", width=800.0, height=60.0),
                Flex(height=20.0),
                Separator(width=1580.0, thickness=2.0, margin=0.0),
                Flex(height=22.0),
                
                Text(text="Entry File (.sp):", style="MainBg", width=800.0, height=40.0),
                Flex(height=14.0),
                Flex(direction="row", gap=20.0, children=[
                    Input(id="entryInput", placeholder="main.sp", text=entryFile, width=1400.0, height=60.0, style="InputStyle", onChange=app.onEntryChanged),
                    Button(label="Browse", width=160.0, height=60.0, style="BtnStyle", onClick=app.onEntryBrowse)
                ]),
                Flex(height=14.0),
                Separator(width=1580.0, thickness=2.0, margin=0.0),
                Flex(height=14.0),
                
                Text(text="Output File (.exe):", style="MainBg", width=800.0, height=40.0),
                Flex(height=14.0),
                Flex(direction="row", gap=20.0, children=[
                    Input(id="outputInput", placeholder="app.exe", text=outputFile, width=1400.0, height=60.0, style="InputStyle", onChange=app.onOutputChanged),
                    Button(label="Browse", width=160.0, height=60.0, style="BtnStyle", onClick=app.onOutputBrowse)
                ]),
                Flex(height=14.0),
                Separator(width=1580.0, thickness=2.0, margin=0.0),
                Flex(height=14.0),
                
                Text(text="Author:", style="MainBg", width=800.0, height=40.0),
                Flex(height=14.0),
                Input(id="authorInput", placeholder="Your Name", text=author, width=1400.0, height=60.0, style="InputStyle", onChange=app.onAuthorChanged),
                Flex(height=14.0),
                Separator(width=1580.0, thickness=2.0, margin=0.0),
                Flex(height=14.0),
                
                Text(text="Version:", style="MainBg", width=800.0, height=40.0),
                Flex(height=14.0),
                Input(id="versionInput", placeholder="1.0.0", text=version, width=1400.0, height=60.0, style="InputStyle", onChange=app.onVersionChanged),
                Flex(height=14.0),
                Separator(width=1580.0, thickness=2.0, margin=0.0),
                Flex(height=14.0),
                
                Text(text="Icon Path (.ico) (optional):", style="MainBg", width=800.0, height=40.0),
                Flex(height=14.0),
                Flex(direction="row", gap=20.0, children=[
                    Input(id="iconInput", placeholder="", text=iconPath, width=1400.0, height=60.0, style="InputStyle", onChange=app.onIconChanged),
                    Button(label="Browse", width=160.0, height=60.0, style="BtnStyle", onClick=app.onIconBrowse)
                ]),
                Flex(height=24.0),
                Separator(width=1580.0, thickness=2.0, margin=0.0),
                Flex(height=24.0),
                
                Checkbox(label="Hide Console Window (-noconsole)", checked=noConsole, width=40.0, height=40.0, style="CheckStyle", onClick=app.onNoConsoleToggle),
                Flex(height=40.0),
                Checkbox(label="Optimize Bytecode", checked=optimize, width=40.0, height=40.0, style="CheckStyle", onClick=app.onOptimizeToggle),
                Flex(height=40.0),
                Checkbox(label="Soft Mode (Disable Type Checking)", checked=softMode, width=40.0, height=40.0, style="CheckStyle", onClick=app.onSoftModeToggle),
                Flex(height=40.0),
                Checkbox(label="Require Admin Privileges (Manifest)", checked=requireAdmin, width=40.0, height=40.0, style="CheckStyle", onClick=app.onAdminToggle),
                Flex(height=50.0),
                Separator(width=1580.0, thickness=2.0, margin=0.0),
                Flex(height=48.0),
                
                Flex(direction="row", gap=40.0, align="center", children=[
                    Button(label="GENERATE & PACK", width=500.0, height=100.0, style="PackBtnStyle", align="center", onClick=app.pack),
                    Text(text=statusMessage, style=currentStatusStyle, width=800.0, height=60.0)
                ])
            ])
        ])
    ])
    
    var event = Render(layout)
    if (event != nil) { event() }
    
    return true
}
