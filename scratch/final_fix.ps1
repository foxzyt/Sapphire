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

$phrases_arr = @()
foreach ($p in $phrases) {
    $phrases_arr += "    listAppend(dataset, `"$p`");"
}

$test_file = "c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\test_nlp.sp"
$content = [System.IO.File]::ReadAllText($test_file)

if (-not ($content -match "var MEMORY_LIMIT = 10000;")) {
    $content = "var MEMORY_LIMIT = 10000;`r`n" + $content
}

$content = $content.Replace("    var dim = 16.0;", "    var dim = 64.0;")
$content = $content.Replace("    var num_heads = 2.0;", "    var num_heads = 4.0;")
$content = $content.Replace("    var num_layers = 2.0;", "    var num_layers = 4.0;")
$content = $content.Replace("    var max_seq_len = 20.0;", "    var max_seq_len = 30.0;")
$content = $content.Replace("    var epochs = 10;", "    var epochs = 50;")

$argmax_target = @"
        var max_val = -1000000.0;
        var max_id = 0.0;
        var v = 0;
        while (v < listLength(last_logits)) {
            var prob = listGet(last_logits, v);
            if (prob.data > max_val) {
                max_val = prob.data;
                max_id = 1.0 * v;
            }
            v = v + 1;
        }
"@

$argmax_replacement = @"
        var max_val = -1000000.0;
        var max_id = 0.0;
        var v = 0;
        while (v < listLength(last_logits)) {
            var prob = listGet(last_logits, v);
            var val = prob.data;
            
            if (listContains(gen_ids, 1.0 * v)) {
                val = val - 2.0;
            }
            
            if (val > max_val) {
                max_val = val;
                max_id = 1.0 * v;
            }
            v = v + 1;
        }
"@
$content = $content.Replace($argmax_target, $argmax_replacement)

$lines = $content -split "`r`n"
$new_lines = @()
$inserted = $false
foreach ($line in $lines) {
    if ($line -match 'listAppend\(dataset, ".*"\);') {
        if (-not $inserted) {
            $new_lines += $phrases_arr
            $inserted = $true
        }
    }
    else {
        $new_lines += $line
    }
}

$final_content = $new_lines -join "`r`n"
[System.IO.File]::WriteAllText($test_file, $final_content)
