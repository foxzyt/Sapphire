$verbos1 = @(
    "eu gosto de ler", "eu quero viajar", "eu preciso estudar",
    "eu amo musica", "eu prefiro cafe", "eu adoro pizza",
    "eu vou trabalhar", "eu posso ajudar", "eu tenho tempo", "eu acho legal"
)
$verbos3 = @(
    "ela gosta de ler", "ele quer viajar", "o menino precisa estudar",
    "a menina ama musica", "o homem prefere cafe", "a mulher adora pizza",
    "o engenheiro vai trabalhar", "a medica pode ajudar", "o cachorro tem tempo", "o gato acha legal"
)
$verbos1_pass = @(
    "eu comprei um carro", "eu vendi a casa", "eu fiz um bolo",
    "eu encontrei o livro", "eu perdi as chaves", "eu assisti um filme",
    "eu li o relatorio", "eu escrevi uma carta", "eu bebi agua", "eu comi maca"
)
$verbos3_pass = @(
    "ela comprou um carro", "ele vendeu a casa", "o menino fez um bolo",
    "a menina encontrou o livro", "o homem perdeu as chaves", "a mulher assistiu um filme",
    "o engenheiro leu o relatorio", "a medica escreveu uma carta", "o cachorro bebeu agua", "o gato comeu maca"
)
$complementos = @(
    "hoje de manha .", "ontem a noite .", "agora mesmo .", "com muita alegria .",
    "rapidamente .", "bem devagar .", "na minha casa .", "no hospital central .",
    "na rua de baixo .", "no centro da cidade ."
)

$phrases = New-Object System.Collections.Generic.HashSet[string]

$phrases.Add("ola , tudo bem ?") | Out-Null
$phrases.Add("bom dia , como voce esta ?") | Out-Null
$phrases.Add("boa tarde , qual e o seu nome ?") | Out-Null
$phrases.Add("boa noite , meu nome e sapphire .") | Out-Null
$phrases.Add("muito prazer em conhecer voce .") | Out-Null
$phrases.Add("eu sou uma inteligencia artificial .") | Out-Null
$phrases.Add("estou aqui para ajudar .") | Out-Null

$rand = New-Object System.Random

while ($phrases.Count -lt 500) {
    $type = $rand.Next(4)
    if ($type -eq 0) { $base = $verbos1[$rand.Next($verbos1.Count)] }
    elseif ($type -eq 1) { $base = $verbos3[$rand.Next($verbos3.Count)] }
    elseif ($type -eq 2) { $base = $verbos1_pass[$rand.Next($verbos1_pass.Count)] }
    else { $base = $verbos3_pass[$rand.Next($verbos3_pass.Count)] }
    
    $comp = $complementos[$rand.Next($complementos.Count)]
    
    $p = "$base $comp"
    $phrases.Add($p) | Out-Null
}

$out = @()
foreach ($p in $phrases) {
    $out += "    listAppend(dataset, `"$p`");"
}
$out | Set-Content -Path "c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\scratch\500_phrases_perfect.txt"
