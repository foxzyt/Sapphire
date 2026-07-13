$test_file = "c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\test_nlp.sp"
$phrases = Get-Content "c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\scratch\500_phrases.txt"
$content = [System.IO.File]::ReadAllText($test_file)

$target = "    listAppend(dataset, `"hello , how are you ?`");`r`n    listAppend(dataset, `"i am fine , thank you !`");`r`n    listAppend(dataset, `"what is your name ?`");`r`n    listAppend(dataset, `"my name is sapphire .`");`r`n    listAppend(dataset, `"tell me a joke .`");`r`n    listAppend(dataset, `"why did the chicken cross the road ?`");`r`n    listAppend(dataset, `"to get to the other side !`");`r`n    listAppend(dataset, `"good morning !`");`r`n    listAppend(dataset, `"good night .`");`r`n    listAppend(dataset, `"see you later .`");"

$replacement = $phrases -join "`r`n"
$new_content = $content.Replace($target, $replacement)
[System.IO.File]::WriteAllText($test_file, $new_content)
