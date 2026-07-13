$greetings = @(
    "ola , tudo bem ?",
    "bom dia !",
    "boa tarde .",
    "boa noite .",
    "como voce esta ?",
    "eu estou bem , obrigado .",
    "qual e o seu nome ?",
    "meu nome e sapphire .",
    "muito prazer em te conhecer .",
    "ate logo !",
    "tchau !",
    "te vejo amanha .",
    "como foi o seu dia ?",
    "meu dia foi otimo .",
    "eu estou cansado .",
    "eu preciso dormir ."
)

$subj_1ps = @("eu")
$verb_1ps = @("comprei", "vendi", "encontrei", "perdi", "escrevi", "li", "assisti", "fiz")
$obj_1ps = @("um carro", "um livro", "um computador", "um celular", "uma carta", "uma mensagem", "um filme", "um bolo")

$subj_3ps_pessoa = @("o homem", "a mulher", "o menino", "a menina", "o professor", "a medica", "ele", "ela", "o engenheiro", "a advogada")
$verb_3ps_acao = @("comprou", "vendeu", "encontrou", "perdi", "escreveu", "leu", "assistiu", "fez", "estudou", "trabalhou")
$obj_3ps_acao = @("um carro novo", "um livro interessante", "um relogio", "uma bolsa", "uma carta", "um relatorio", "um filme bom", "um bolo delicioso", "muito", "pouco")

$verb_3ps_sentimento = @("gosta de", "adora", "odeia", "ama", "prefere")
$obj_3ps_sentimento = @("cafe", "cha", "ler", "estudar", "viajar", "dormir", "trabalhar", "assistir tv", "jogar futebol", "ouvir musica")

$locais = @("em casa", "na escola", "no trabalho", "no parque", "na praia", "no cinema", "no shopping", "na rua")
$tempos = @("hoje", "ontem", "amanha", "sempre", "de manha", "de noite", "agora")

$phrases = New-Object System.Collections.Generic.HashSet[string]

foreach ($g in $greetings) {
    $phrases.Add($g) | Out-Null
}

$rand = New-Object System.Random

# 1st person
while ($phrases.Count -lt 150) {
    $s = "eu"
    $v = $verb_1ps[$rand.Next($verb_1ps.Count)]
    $o = $obj_1ps[$rand.Next($obj_1ps.Count)]
    $t = $tempos[$rand.Next($tempos.Count)]
    
    $type = $rand.Next(3)
    if ($type -eq 0) { $p = "$s $v $o ." }
    elseif ($type -eq 1) { $p = "$s $v $o $t ." }
    else { $p = "$t , $s $v $o ." }
    
    $p = $p -replace '\s+', ' '
    $phrases.Add($p) | Out-Null
}

# 3rd person acao
while ($phrases.Count -lt 350) {
    $s = $subj_3ps_pessoa[$rand.Next($subj_3ps_pessoa.Count)]
    $v = $verb_3ps_acao[$rand.Next($verb_3ps_acao.Count)]
    $o = $obj_3ps_acao[$rand.Next($obj_3ps_acao.Count)]
    $l = $locais[$rand.Next($locais.Count)]
    
    $type = $rand.Next(3)
    if ($type -eq 0) { $p = "$s $v $o ." }
    elseif ($type -eq 1) { $p = "$s $v $o $l ." }
    else { $p = "$s $v $o $l ontem ." }
    
    $p = $p -replace '\s+', ' '
    $phrases.Add($p) | Out-Null
}

# 3rd person sentimento
while ($phrases.Count -lt 500) {
    $s = $subj_3ps_pessoa[$rand.Next($subj_3ps_pessoa.Count)]
    $v = $verb_3ps_sentimento[$rand.Next($verb_3ps_sentimento.Count)]
    $o = $obj_3ps_sentimento[$rand.Next($obj_3ps_sentimento.Count)]
    
    $type = $rand.Next(2)
    if ($type -eq 0) { $p = "$s $v $o ." }
    else { $p = "$s $v muito $o ." }
    
    $p = $p -replace '\s+', ' '
    $phrases.Add($p) | Out-Null
}

$out = @()
foreach ($p in $phrases) {
    $out += "    listAppend(dataset, `"$p`");"
}
$out | Select-Object -First 500 | Set-Content -Path "c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\scratch\500_phrases_human.txt"
