$classes = @("Tensor", "Init", "Neuron", "Linear", "ActivationLayer", "MLP", "Adam", "Softmax", "Tokenizer", "Embedding", "LayerNorm", "SelfAttention", "MultiHeadAttention", "TransformerBlock", "TransformerLanguageModel", "CrossEntropyLoss", "SequenceCrossEntropyLoss")

$lines = [System.IO.File]::ReadAllLines("c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\sapphire_grad.sp")
$out = @()
$in_class = $false
$brace_count = 0
$extracted = @{}
$current_class = ""

foreach ($line in $lines) {
    if (-not $in_class) {
        foreach ($c in $classes) {
            if ($line -match "^class $c\b") {
                if (-not $extracted.ContainsKey($c)) {
                    $in_class = $true
                    $brace_count = 0
                    $current_class = $c
                    $extracted[$c] = $true
                    break
                }
            }
        }
    }
    
    if ($in_class) {
        $out += $line
        $brace_count += ($line.Split("{").Count - 1)
        $brace_count -= ($line.Split("}").Count - 1)
        
        # Check if we reached the end of the class
        if ($brace_count -le 0 -and $line -match "}") {
            $in_class = $false
        }
    }
}

[System.IO.File]::WriteAllLines("c:\Users\berna\Downloads\Sapphire-v1.0.6\Sapphire-v1.0.5\mini_sapphire_grad.sp", $out)
