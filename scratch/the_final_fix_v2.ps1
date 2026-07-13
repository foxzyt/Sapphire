$verbos1 = "eu gosto de ler", "eu quero viajar", "eu preciso estudar", "eu amo musica", "eu prefiro cafe", "eu adoro pizza", "eu vou trabalhar", "eu posso ajudar", "eu tenho tempo", "eu acho legal"
$verbos3 = "ela gosta de ler", "ele quer viajar", "o menino precisa estudar", "a menina ama musica", "o homem prefere cafe", "a mulher adora pizza", "o engenheiro vai trabalhar", "a medica pode ajudar", "o cachorro tem tempo", "o gato acha legal"
$verbos1_pass = "eu comprei um carro", "eu vendi a casa", "eu fiz um bolo", "eu encontrei o livro", "eu perdi as chaves", "eu assisti um filme", "eu li o relatorio", "eu escrevi uma carta", "eu bebi agua", "eu comi maca"
$verbos3_pass = "ela comprou um carro", "ele vendeu a casa", "o menino fez um bolo", "a menina encontrou o livro", "o homem perdeu as chaves", "a mulher assistiu um filme", "o engenheiro leu o relatorio", "a medica escreveu uma carta", "o cachorro bebeu agua", "o gato comeu maca"
$verbos5 = "eu jogo futebol", "ela joga tenis", "nos andamos muito", "o menino brinca na rua", "a menina canta alto", "o medico fala baixo", "o engenheiro corre rapido", "a professora ensina bem", "eu estudo muito", "ele dorme cedo"

$bases = $verbos1 + $verbos3 + $verbos1_pass + $verbos3_pass + $verbos5
$complementos = "hoje de manha .", "ontem a noite .", "agora mesmo .", "com muita alegria .", "rapidamente .", "bem devagar .", "na minha casa .", "no hospital central .", "na rua de baixo .", "no centro da cidade ."

$test_file = 'c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\test_nlp.sp'
$lines = Get-Content $test_file
$new_lines = @()

for ($i = 0; $i -lt $lines.Count; $i++) {
    $line = $lines[$i]
    if ($line.Contains("`$phrases_str +=")) { continue }
    
    $new_lines += $line
    
    if ($line.Contains("var dataset = listCreate();")) {
        foreach ($b in $bases) {
            foreach ($c in $complementos) {
                $p = "$b $c"
                $new_lines += "    listAppend(dataset, `"$p`");"
            }
        }
    }
}

$new_lines | Set-Content $test_file
