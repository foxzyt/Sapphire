$test_file = "c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\test_nlp.sp"
$phrases = Get-Content "c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\scratch\500_phrases.txt"
$content = Get-Content $test_file
$new_content = @()
$skip = $false
foreach ($line in $content) {
    if ($line -match 'listAppend\(dataset, "hello , how are you \?"\);') {
        $skip = $true
        $new_content += $phrases
    }
    elseif ($line -match 'listAppend\(dataset, "see you later ."\);') {
        $skip = $false
        continue
    }
    
    if (-not $skip) {
        $new_content += $line
    }
}
$new_content | Set-Content -Path $test_file
