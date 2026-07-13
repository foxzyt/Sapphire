$test_file = "c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\test_nlp.sp"
$phrases = Get-Content "c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\scratch\500_phrases_human.txt"
$content = Get-Content $test_file
$new_content = @()
$inserted = $false
foreach ($line in $content) {
    if ($line -match 'listAppend\(dataset, ".*"\);') {
        if (-not $inserted) {
            $new_content += $phrases
            $inserted = $true
        }
    }
    else {
        $new_content += $line
    }
}
$new_content | Set-Content -Path $test_file
