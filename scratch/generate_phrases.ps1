$subjects = @("eu", "voce", "ele", "ela", "nos", "eles", "elas", "o cachorro", "o gato", "o passaro", "o menino", "a menina", "o homem", "a mulher", "o professor", "a medica", "o engenheiro", "a cantora", "o motorista", "a advogada")
$verbs = @("gosta de", "adora", "odeia", "ama", "prefere", "quer", "precisa de", "tem", "comprou", "vendeu", "fez", "criou", "descobriu", "olhou para", "encontrou", "perdeu", "escondeu", "mostrou", "entregou", "recebeu")
$objects = @("macas", "bananas", "carros", "livros", "computadores", "celulares", "filmes", "musicas", "jogos", "viagens", "dinheiro", "tempo", "amigos", "problemas", "solucoes", "historias", "segredos", "mentiras", "verdades", "sonhos")
$adverbs = @("hoje", "ontem", "amanha", "sempre", "nunca", "as vezes", "frequentemente", "raramente", "rapido", "devagar", "bem", "mal", "muito", "pouco", "bastante", "demais", "agora", "depois", "cedo", "tarde")
$places = @("na escola", "em casa", "no trabalho", "na rua", "no parque", "na praia", "no cinema", "no teatro", "no restaurante", "no hospital", "no banco", "na loja", "no mercado", "no shopping", "na fazenda", "na cidade", "no campo", "no pais", "no exterior", "no mundo")

$phrases = New-Object System.Collections.Generic.HashSet[string]
$rand = New-Object System.Random
while ($phrases.Count -lt 500) {
    $s = $subjects[$rand.Next($subjects.Count)]
    $v = $verbs[$rand.Next($verbs.Count)]
    $o = $objects[$rand.Next($objects.Count)]
    $a = $adverbs[$rand.Next($adverbs.Count)]
    $p = $places[$rand.Next($places.Count)]
    
    $struct = $rand.Next(1, 6)
    if ($struct -eq 1) { $phrase = "$s $v $o ." }
    elseif ($struct -eq 2) { $phrase = "$s $v $o $a ." }
    elseif ($struct -eq 3) { $phrase = "$s $v $o $p ." }
    elseif ($struct -eq 4) { $phrase = "$a , $s $v $o ." }
    else { $phrase = "$s $v $o $p $a ." }
    
    $phrase = $phrase -replace '\s+', ' '
    $phrases.Add($phrase) | Out-Null
}

$out = @()
foreach ($p in $phrases) {
    $out += "    listAppend(dataset, `"$p`");"
}
$out | Set-Content -Path "c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\scratch\500_phrases.txt"
