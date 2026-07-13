$text = [System.IO.File]::ReadAllText("sapphire_grad.sp")
$text = [System.Text.RegularExpressions.Regex]::Replace($text, "(?m)[ \t]*//.*$", "")
[System.IO.File]::WriteAllText("sapphire_grad.sp", $text)
