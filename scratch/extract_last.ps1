$classes = @("Tensor", "Init", "Neuron", "Linear", "ActivationLayer", "MLP", "Adam", "Softmax", "Tokenizer", "Embedding", "LayerNorm", "MultiHeadAttention", "TransformerBlock", "TransformerLanguageModel", "CrossEntropyLoss", "SequenceCrossEntropyLoss", "SelfAttention")

$lines = [System.IO.File]::ReadAllLines("c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\sapphire_grad.sp")

# We will read from bottom to top to get the LAST definition of each class
$out = @()
$extracted = @{}
$current_class_lines = @()
$in_class = $false
$brace_count = 0

for ($i = $lines.Count - 1; $i -ge 0; $i--) {
    $line = $lines[$i]
    if (-not $in_class) {
        if ($line -match "}") {
            # Could be the end of a class, let's scan backwards to find its start
            $brace_count = 1
            $in_class = $true
            $current_class_lines = @($line)
        }
    } else {
        # We are reading a class backwards
        $current_class_lines += $line
        $brace_count += ($line.Split("}").Count - 1)
        $brace_count -= ($line.Split("{").Count - 1)
        
        if ($brace_count -le 0 -and $line -match "^class ") {
            $in_class = $false
            
            # Check if this is one of our target classes
            foreach ($c in $classes) {
                if ($line -match "^class $c\b") {
                    if (-not $extracted.ContainsKey($c)) {
                        $extracted[$c] = $true
                        
                        # Reverse current_class_lines to forward order
                        [array]::Reverse($current_class_lines)
                        $out = $current_class_lines + $out
                    }
                    break
                }
            }
        }
    }
}

[System.IO.File]::WriteAllLines("c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\mini_sapphire_grad.sp", $out)
