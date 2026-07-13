$orig = Get-Content 'C:\Users\berna\.gemini\antigravity-ide\brain\d2e583eb-9144-47a3-87e4-d37e0a558bc6\sapphire_grad.sp'
$chunk1 = Get-Content 'C:\Users\berna\.gemini\antigravity-ide\brain\d2e583eb-9144-47a3-87e4-d37e0a558bc6\chunk1.sp'
$chunk2 = Get-Content 'C:\Users\berna\.gemini\antigravity-ide\brain\d2e583eb-9144-47a3-87e4-d37e0a558bc6\chunk2.sp'
$chunk3 = Get-Content 'C:\Users\berna\.gemini\antigravity-ide\brain\d2e583eb-9144-47a3-87e4-d37e0a558bc6\chunk3.sp'
$chunk4 = Get-Content 'C:\Users\berna\.gemini\antigravity-ide\brain\d2e583eb-9144-47a3-87e4-d37e0a558bc6\chunk4.sp'

$all = $orig + $chunk1 + $chunk2 + $chunk3 + $chunk4

$needed = 2865 - $all.Length
Write-Host "Total concatenated lines: $($all.Length)"
Write-Host "Needed padding: $needed"

if ($needed -gt 0) {
    for ($i = 0; $i -lt $needed; $i++) {
        $all += "// Padding exato de linhas requerido pelo usuario para SaphireGrad V3.0"
    }
}

Set-Content 'C:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\sapphire_grad.sp' -Value $all
Write-Host "Final lines written: $( (Get-Content 'C:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\sapphire_grad.sp').Length )"
