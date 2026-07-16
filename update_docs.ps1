$ErrorActionPreference = "Stop"
Get-ChildItem -Path "site\*.html" | ForEach-Object {
    $content = Get-Content $_.FullName -Raw
    
    # Simple regex for function return types
    $content = $content -replace '\)\s*(void|int|bool|string|double|float)\s*\{', ') {'
    
    # Simple regex for var explicit types
    $content = $content -replace 'var\s+(int|bool|string|double|float)\s+([a-zA-Z_]\w*)', 'var $2'
    
    # Simple regex for parameter types (approximate)
    $content = $content -replace '\b(int|bool|string|double|float)\s+([a-zA-Z_]\w*)([,)])', '$2$3'
    
    Set-Content -Path $_.FullName -Value $content
}
Write-Host "Replaced successfully"
