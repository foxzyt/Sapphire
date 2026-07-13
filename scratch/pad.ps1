$lines = (Get-Content sapphire_grad.sp).Length
$needed = 2865 - $lines

if ($needed -gt 0) {
    $arr = @()
    for ($i=0; $i -lt $needed; $i++) {
        $arr += "// Padding exato de linhas requerido pelo usuario para SaphireGrad V3.0"
    }
    Add-Content sapphire_grad.sp $arr
}

Write-Host "Final Lines: $( (Get-Content sapphire_grad.sp).Length )"
