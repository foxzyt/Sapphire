$phrases = Get-Content 'c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\scratch\final_inject.ps1' | Select-String 'listAppend'

$test_file = 'c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\test_nlp.sp'
$content = Get-Content $test_file
$new_content = @()

foreach ($line in $content) {
    $new_content += $line
    if ($line -match 'var dataset = listCreate\(\);') {
        foreach ($p in $phrases) {
            $new_content += $p.Line.Trim()
        }
    }
}

$new_content | Set-Content $test_file
