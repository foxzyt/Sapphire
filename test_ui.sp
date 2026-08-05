// test_ui.sp
// Um script de teste de UI impressionante e completo para a engine nativa do Sapphire

var UI = 1;

// --- Configurações da Janela ---
var config_window_width = 900;
var config_window_height = 700;
var config_window_title = "Sapphire UI Engine Demo";

// --- Definição de Estilos Visuais ---
Style("AppBackground", bgColor="#121214", textColor="#ffffff");
Style("CardBg", bgColor="#202024", borderRadius=16.0);
Style("TitleStyle", color="#00e88f", fontSize=42);
Style("PrimaryBtn", bgColor="#00875f", hoverColor="#015f43", borderRadius=8.0, fontSize=24, color="#ffffff");
Style("DangerBtn", bgColor="#f75a68", hoverColor="#aa2834", borderRadius=8.0, fontSize=24, color="#ffffff");
Style("InputStyle", bgColor="#121214", borderRadius=8.0, color="#ffffff");

// --- Estado Global ---
var volume = 0.5;
var termsAccepted = false;
var loadingProgress = 0.2;

// --- Callbacks ---
function handleSubmit() {
    if (termsAccepted) {
        print("Submit clicado! Termos aceitos: true");
        loadingProgress = 1.0;
    } else {
        print("Submit clicado! Termos aceitos: false");
    }
}

function handleReset() {
    print("Reset clicado!");
    volume = 0.5;
    termsAccepted = false;
    loadingProgress = 0.0;
}

function handleVolumeChange(val) {
    volume = val; 
    loadingProgress = val;
}

function handleTermsChange(state) {
    termsAccepted = state;
}

// --- Loop Principal da Interface ---
function updateUI() {
    
    // A árvore UI é recriada a cada frame (Retained Mode com regeneração)
    var ui_root = Flex(
        direction="column",
        justify="center",
        align="center",
        gap=25.0,
        style="AppBackground",
        children=[
            Flex(
                direction="column",
                align="center",
                gap=10.0,
                children=[
                    Text("💎 Sapphire UI", style="TitleStyle", weight="black", shadow={"offsetX": 2.0, "offsetY": 4.0, "blur": 8.0}),
                    Text("Construindo interfaces incriveis nativamente", size=20, color="#a9a9b2", weight="light")
                ]
            ),
            Flex(
                direction="column",
                align="center",
                gap=20.0,
                style="CardBg",
                children=[
                    Input(
                        placeholder="Digite seu e-mail corporativo...", 
                        passwordMode=false,
                        width=400.0, 
                        height=45.0,
                        style="InputStyle"
                    ),
                    Input(
                        placeholder="Sua senha secreta...", 
                        passwordMode=true,
                        width=400.0, 
                        height=45.0,
                        style="InputStyle"
                    ),
                    Flex(
                        direction="column",
                        align="center",
                        gap=5.0,
                        children=[
                            Text("Volume do Sistema", size=18, color="#e1e1e6"),
                            Slider(
                                min=0.0, 
                                max=1.0, 
                                initialValue=volume, 
                                step=0.05,
                                width=350.0, 
                                onChange=handleVolumeChange
                            )
                        ]
                    ),
                    Checkbox(
                        label="Aceito os Termos de Servico do Sapphire", 
                        checked=termsAccepted, 
                        onChange=handleTermsChange
                    ),
                    Flex(
                        direction="row",
                        justify="center",
                        gap=20.0,
                        children=[
                            Button(
                                label="Cancelar", 
                                width=150.0, 
                                height=50.0, 
                                style="DangerBtn", 
                                onClick=handleReset
                            ),
                            Button(
                                label="Confirmar e Salvar", 
                                width=230.0, 
                                height=50.0, 
                                style="PrimaryBtn", 
                                onClick=handleSubmit
                            )
                        ]
                    ),
                    ProgressBar(
                        progress=loadingProgress, 
                        color="#00e88f", 
                        trackColor="#121214",
                        width=400.0,
                        height=15.0,
                        animated=true
                    )
                ]
            )
        ]
    );

    render(ui_root);
    return true;
}
