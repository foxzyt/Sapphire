$test_file = 'c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\test_nlp.sp'
$phrases_file = 'c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\scratch\final_inject.ps1'

$phrases = Get-Content $phrases_file | Select-String 'listAppend'

$lines = Get-Content $test_file
$new_lines = @()

for ($i = 0; $i -lt $lines.Count; $i++) {
    $line = $lines[$i]
    $new_lines += $line
    
    if ($line.Contains("var dataset = listCreate();")) {
        foreach ($p in $phrases) {
            $new_lines += $p.Line.Trim()
        }
    }
}

$new_lines | Set-Content $test_file
