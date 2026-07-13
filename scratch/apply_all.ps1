$test_file = "c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\test_nlp.sp"
$phrases = Get-Content "c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\scratch\500_phrases_perfect.txt"
$content = Get-Content $test_file
$new_content = @()

$new_content += "var MEMORY_LIMIT = 10000;"

$skip_dataset = $false
$dataset_inserted = $false

foreach ($line in $content) {
    if ($line -eq "var MEMORY_LIMIT = 10000;") {
        continue
    }

    if ($line -match 'listAppend\(dataset, ".*"\);') {
        if (-not $dataset_inserted) {
            $new_content += $phrases
            $dataset_inserted = $true
        }
        continue
    }
    
    # Hyperparameters
    if ($line -match "var dim = ") {
        $new_content += "    var dim = 64.0;"
        continue
    }
    if ($line -match "var num_heads = ") {
        $new_content += "    var num_heads = 4.0;"
        continue
    }
    if ($line -match "var num_layers = ") {
        $new_content += "    var num_layers = 4.0;"
        continue
    }
    if ($line -match "var max_seq_len = ") {
        $new_content += "    var max_seq_len = 30.0;"
        continue
    }
    if ($line -match "var epochs = ") {
        $new_content += "    var epochs = 50;"
        continue
    }

    # Repetition penalty logic insertion
    # original argmax loop starts with "var max_val = -1000000.0;"
    if ($line -match "var max_val = -1000000.0;") {
        $new_content += "        var max_val = -1000000.0;"
        $new_content += "        var max_id = 0.0;"
        $new_content += "        var v = 0;"
        $new_content += "        while (v < listLength(last_logits)) {"
        $new_content += "            var prob = listGet(last_logits, v);"
        $new_content += "            var val = prob.data;"
        $new_content += "            "
        $new_content += "            // Repetition Penalty"
        $new_content += "            if (listContains(gen_ids, 1.0 * v)) {"
        $new_content += "                val = val - 2.0;"
        $new_content += "            }"
        $new_content += "            "
        $new_content += "            if (val > max_val) {"
        $new_content += "                max_val = val;"
        $new_content += "                max_id = 1.0 * v;"
        $new_content += "            }"
        $new_content += "            v = v + 1;"
        $new_content += "        }"
        continue
    }
    
    # We must skip the original inner loop since we replaced it above
    if ($line -match "var max_id = 0.0;") { continue }
    if ($line -match "var v = 0;") { continue }
    if ($line -match "while \(v < listLength\(last_logits\)\) \{") {
        $skip_dataset = $true
        continue
    }
    if ($skip_dataset) {
        if ($line -match "v = v \+ 1;") {
            $skip_dataset = $false
            # we also skip the closing brace which comes immediately after, but the next line read is `}` 
            # wait, let's just use a simple state to skip until `}`
        }
        continue
    }
    if ($line -eq "        }" -and $new_content[-1] -match "v = v \+ 1;") {
        # this handles skipping the closing bracket of the argmax loop we replaced
        continue
    }
    
    # We just need a safer way to replace the block. I will use -replace on Raw content instead.
    
    $new_content += $line
}

$new_content | Set-Content -Path $test_file
