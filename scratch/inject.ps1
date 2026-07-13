$test_file = "c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\test_nlp.sp"
$phrases_file = "c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\scratch\500_phrases_perfect.txt"

$phrases = Get-Content $phrases_file -Raw
$content = [System.IO.File]::ReadAllText($test_file)

$target = "    var dataset = listCreate();`r`n    `r`n    var tokenizer = Tokenizer();"
$replacement = "    var dataset = listCreate();`r`n" + $phrases + "`r`n    var tokenizer = Tokenizer();"

$content = $content.Replace($target, $replacement)

[System.IO.File]::WriteAllText($test_file, $content)
