$test_file = "c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\test_nlp.sp"
$phrases = Get-Content "c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\scratch\500_phrases_perfect.txt"
$phrases_str = $phrases -join "`r`n"
$content = [System.IO.File]::ReadAllText($test_file)

# 1. Add MEMORY_LIMIT
if (-not ($content -match "var MEMORY_LIMIT = 10000;")) {
    $content = "var MEMORY_LIMIT = 10000;`r`n" + $content
}

# 2. Replace Hyperparameters
$content = $content.Replace("    var dim = 16.0;", "    var dim = 64.0;")
$content = $content.Replace("    var num_heads = 2.0;", "    var num_heads = 4.0;")
$content = $content.Replace("    var num_layers = 2.0;", "    var num_layers = 4.0;")
$content = $content.Replace("    var max_seq_len = 20.0;", "    var max_seq_len = 30.0;")
$content = $content.Replace("    var epochs = 10;", "    var epochs = 50;")

# 3. Replace argmax with repetition penalty
$argmax_target = @"
        var max_val = -1000000.0;
        var max_id = 0.0;
        var v = 0;
        while (v < listLength(last_logits)) {
            var prob = listGet(last_logits, v);
            if (prob.data > max_val) {
                max_val = prob.data;
                max_id = 1.0 * v;
            }
            v = v + 1;
        }
"@

$argmax_replacement = @"
        var max_val = -1000000.0;
        var max_id = 0.0;
        var v = 0;
        while (v < listLength(last_logits)) {
            var prob = listGet(last_logits, v);
            var val = prob.data;
            
            if (listContains(gen_ids, 1.0 * v)) {
                val = val - 2.0;
            }
            
            if (val > max_val) {
                max_val = val;
                max_id = 1.0 * v;
            }
            v = v + 1;
        }
"@
$content = $content.Replace($argmax_target, $argmax_replacement)

# 4. Remove all listAppend(dataset, ...) lines, then insert the new ones
$lines = $content -split "`r`n"
$new_lines = @()
$inserted = $false
foreach ($line in $lines) {
    if ($line -match 'listAppend\(dataset, ".*"\);') {
        if (-not $inserted) {
            $new_lines += $phrases
            $inserted = $true
        }
    }
    else {
        $new_lines += $line
    }
}

$final_content = $new_lines -join "`r`n"
[System.IO.File]::WriteAllText($test_file, $final_content)
