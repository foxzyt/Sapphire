class LayerNorm {
    function init(double dim, double eps) void {
        this.dim = dim;
        this.eps = eps;
        
        this.gamma = listCreate();
        this.beta = listCreate();
        
        var i = 0;
        while (i < dim) {
            var g = Tensor(); g.init(1.0, listCreate(), "");
            var b = Tensor(); b.init(0.0, listCreate(), "");
            listAppend(this.gamma, g);
            listAppend(this.beta, b);
            i = i + 1;
        }
    }
    
    function forward(list x, list out) void {
        var i = 0;
        var seq_len = listLength(x);
        while (i < seq_len) {
            var xi = listGet(x, i);
            
            var mean_t = Tensor(); mean_t.init(0.0, listCreate(), "");
            var j = 0;
            while (j < this.dim) {
                var next_mean = Tensor();
                mean_t.add(listGet(xi, j), next_mean);
                mean_t = next_mean;
                j = j + 1;
            }
            var div_t = Tensor(); div_t.init(this.dim, listCreate(), "");
            var mean_final = Tensor(); mean_t.div(div_t, mean_final);
            
            var var_t = Tensor(); var_t.init(0.0, listCreate(), "");
            j = 0;
            while (j < this.dim) {
                var diff = Tensor();
                var elem = listGet(xi, j);
                elem.sub(mean_final, diff);
                var sq = Tensor();
                diff.pow_t(2.0, sq);
                var next_var = Tensor();
                var_t.add(sq, next_var);
                var_t = next_var;
                j = j + 1;
            }
            var var_final = Tensor(); var_t.div(div_t, var_final);
            
            var eps_t = Tensor(); eps_t.init(this.eps, listCreate(), "");
            var var_eps = Tensor(); var_final.add(eps_t, var_eps);
            var std_dev = Tensor(); var_eps.pow_t(0.5, std_dev);
            
            var out_xi = listCreate();
            j = 0;
            while (j < this.dim) {
                var diff = Tensor();
                var elem = listGet(xi, j);
                elem.sub(mean_final, diff);
                
                var norm = Tensor();
                diff.div(std_dev, norm);
                
                var scaled = Tensor();
                norm.mul(listGet(this.gamma, j), scaled);
                
                var shifted = Tensor();
                scaled.add(listGet(this.beta, j), shifted);
                
                listAppend(out_xi, shifted);
                j = j + 1;
            }
            listAppend(out, out_xi);
            i = i + 1;
        }
    }
    
    function parameters(list params_out) void {
        var j = 0;
        while (j < this.dim) {
            listAppend(params_out, listGet(this.gamma, j));
            listAppend(params_out, listGet(this.beta, j));
            j = j + 1;
        }
    }
}

class Softmax {
    function forward(list x, list out) void {
        var len = listLength(x);
        var i = 0;
        var sum_exp = Tensor(); sum_exp.init(0.0, listCreate(), "");
        var exps = listCreate();
        
        while (i < len) {
            var exp_t = Tensor();
            var xi = listGet(x, i);
            xi.exp_t(exp_t);
            listAppend(exps, exp_t);
            
            var next_sum = Tensor();
            sum_exp.add(exp_t, next_sum);
            sum_exp = next_sum;
            
            i = i + 1;
        }
        
        i = 0;
        while (i < len) {
            var ex = listGet(exps, i);
            var prob = Tensor();
            ex.div(sum_exp, prob);
            listAppend(out, prob);
            i = i + 1;
        }
    }
}

class MultiHeadAttention {
    function init(double dim, double num_heads, string init_type) void {
        this.dim = dim;
        this.num_heads = num_heads;
        this.head_dim = dim / num_heads;
        
        this.q_proj = Linear(); this.q_proj.init(dim, dim, init_type);
        this.k_proj = Linear(); this.k_proj.init(dim, dim, init_type);
        this.v_proj = Linear(); this.v_proj.init(dim, dim, init_type);
        this.out_proj = Linear(); this.out_proj.init(dim, dim, init_type);
    }
    
    function parameters(list params_out) void {
        this.q_proj.parameters(params_out);
        this.k_proj.parameters(params_out);
        this.v_proj.parameters(params_out);
        this.out_proj.parameters(params_out);
    }
    
    function forward(list x, list out) void {
        var seq_len = listLength(x);
        var Q = listCreate();
        var K = listCreate();
        var V = listCreate();
        
        var i = 0;
        while (i < seq_len) {
            var xi = listGet(x, i);
            var qi = listCreate(); this.q_proj.forward(xi, qi); listAppend(Q, qi);
            var ki = listCreate(); this.k_proj.forward(xi, ki); listAppend(K, ki);
            var vi = listCreate(); this.v_proj.forward(xi, vi); listAppend(V, vi);
            i = i + 1;
        }
        
        var scale = Tensor();
        scale.init(sqrt(this.head_dim), listCreate(), "");
        
        i = 0;
        while (i < seq_len) {
            var qi = listGet(Q, i);
            var token_heads_out = listCreate();
            
            var h = 0;
            while (h < this.num_heads) {
                var head_start = h * this.head_dim;
                
                var scores = listCreate();
                var j = 0;
                while (j < seq_len) {
                    var kj = listGet(K, j);
                    var dot_t = Tensor(); dot_t.init(0.0, listCreate(), "");
                    var d = 0;
                    while (d < this.head_dim) {
                        var q_val = listGet(qi, head_start + d);
                        var k_val = listGet(kj, head_start + d);
                        var prod = Tensor(); q_val.mul(k_val, prod);
                        var next_dot = Tensor(); dot_t.add(prod, next_dot);
                        dot_t = next_dot;
                        d = d + 1;
                    }
                    
                    var scaled_dot = Tensor();
                    dot_t.div(scale, scaled_dot);
                    
                    if (j > i) {
                        var neg_inf = Tensor();
                        neg_inf.init(-10000.0, listCreate(), "");
                        var next_scaled = Tensor();
                        scaled_dot.add(neg_inf, next_scaled);
                        scaled_dot = next_scaled;
                    }
                    
                    listAppend(scores, scaled_dot);
                    j = j + 1;
                }
                
                var softmax_layer = Softmax();
                var probs = listCreate();
                softmax_layer.forward(scores, probs);
                
                var d = 0;
                while (d < this.head_dim) {
                    var sum_v = Tensor(); sum_v.init(0.0, listCreate(), "");
                    var j = 0;
                    while (j < seq_len) {
                        var vj = listGet(V, j);
                        var v_val = listGet(vj, head_start + d);
                        var prob = listGet(probs, j);
                        
                        var prod = Tensor();
                        prob.mul(v_val, prod);
                        
                        var next_sum = Tensor();
                        sum_v.add(prod, next_sum);
                        sum_v = next_sum;
                        
                        j = j + 1;
                    }
                    listAppend(token_heads_out, sum_v);
                    d = d + 1;
                }
                h = h + 1;
            }
            
            var final_out = listCreate();
            this.out_proj.forward(token_heads_out, final_out);
            listAppend(out, final_out);
            
            i = i + 1;
        }
    }
}

class TransformerBlock {
    function init(double dim, double num_heads, double ffn_dim, string init_type) void {
        this.ln1 = LayerNorm(); this.ln1.init(dim, 0.00001);
        this.attn = MultiHeadAttention(); this.attn.init(dim, num_heads, init_type);
        this.ln2 = LayerNorm(); this.ln2.init(dim, 0.00001);
        
        var nouts = listCreate();
        listAppend(nouts, ffn_dim);
        listAppend(nouts, dim);
        var acts = listCreate();
        listAppend(acts, "relu");
        
        this.mlp = MLP(); this.mlp.init(dim, nouts, acts, init_type);
    }
    
    function parameters(list params_out) void {
        this.ln1.parameters(params_out);
        this.attn.parameters(params_out);
        this.ln2.parameters(params_out);
        this.mlp.parameters(params_out);
    }
    
    function forward(list x, list out) void {
        var ln1_out = listCreate();
        this.ln1.forward(x, ln1_out);
        
        var attn_out = listCreate();
        this.attn.forward(ln1_out, attn_out);
        
        var res1_out = listCreate();
        var i = 0;
        var seq_len = listLength(x);
        while (i < seq_len) {
            var xi = listGet(x, i);
            var ai = listGet(attn_out, i);
            var sum_i = listCreate();
            var j = 0;
            while (j < listLength(xi)) {
                var sum_t = Tensor();
                var t1 = listGet(xi, j);
                var t2 = listGet(ai, j);
                t1.add(t2, sum_t);
                listAppend(sum_i, sum_t);
                j = j + 1;
            }
            listAppend(res1_out, sum_i);
            i = i + 1;
        }
        
        var ln2_out = listCreate();
        this.ln2.forward(res1_out, ln2_out);
        
        var mlp_out = listCreate();
        i = 0;
        while (i < seq_len) {
            var ln2_i = listGet(ln2_out, i);
            var mlp_i = listCreate();
            this.mlp.forward(ln2_i, mlp_i);
            listAppend(mlp_out, mlp_i);
            i = i + 1;
        }
        
        i = 0;
        while (i < seq_len) {
            var r1_i = listGet(res1_out, i);
            var m_i = listGet(mlp_out, i);
            var sum_i = listCreate();
            var j = 0;
            while (j < listLength(r1_i)) {
                var sum_t = Tensor();
                var t1 = listGet(r1_i, j);
                var t2 = listGet(m_i, j);
                t1.add(t2, sum_t);
                listAppend(sum_i, sum_t);
                j = j + 1;
            }
            listAppend(out, sum_i);
            i = i + 1;
        }
    }
}

class TransformerLanguageModel {
    function init(double vocab_size, double dim, double num_heads, double num_layers, double max_seq_len, string init_type) void {
        this.vocab_size = vocab_size;
        this.token_emb = Embedding(); this.token_emb.init(vocab_size, dim, init_type);
        this.pos_emb = Embedding(); this.pos_emb.init(max_seq_len, dim, init_type);
        
        this.blocks = listCreate();
        var i = 0;
        while (i < num_layers) {
            var block = TransformerBlock();
            block.init(dim, num_heads, dim * 4.0, init_type);
            listAppend(this.blocks, block);
            i = i + 1;
        }
        
        this.ln_f = LayerNorm(); this.ln_f.init(dim, 0.00001);
        this.lm_head = Linear(); this.lm_head.init(dim, vocab_size, init_type);
    }
    
    function parameters(list params_out) void {
        this.token_emb.parameters(params_out);
        this.pos_emb.parameters(params_out);
        
        var i = 0;
        while (i < listLength(this.blocks)) {
            var block = listGet(this.blocks, i);
            block.parameters(params_out);
            i = i + 1;
        }
        
        this.ln_f.parameters(params_out);
        this.lm_head.parameters(params_out);
    }
    
    function forward(list input_ids, list out_logits) void {
        var seq_len = listLength(input_ids);
        var tok_emb = listCreate();
        this.token_emb.forward(input_ids, tok_emb);
        
        var pos_ids = listCreate();
        var i = 0;
        while (i < seq_len) {
            listAppend(pos_ids, i);
            i = i + 1;
        }
        
        var p_emb = listCreate();
        this.pos_emb.forward(pos_ids, p_emb);
        
        var x = listCreate();
        i = 0;
        while (i < seq_len) {
            var t_i = listGet(tok_emb, i);
            var p_i = listGet(p_emb, i);
            var sum_i = listCreate();
            var j = 0;
            while (j < listLength(t_i)) {
                var sum_t = Tensor();
                var t1 = listGet(t_i, j);
                var t2 = listGet(p_i, j);
                t1.add(t2, sum_t);
                listAppend(sum_i, sum_t);
                j = j + 1;
            }
            listAppend(x, sum_i);
            i = i + 1;
        }
        
        i = 0;
        while (i < listLength(this.blocks)) {
            var block = listGet(this.blocks, i);
            var next_x = listCreate();
            block.forward(x, next_x);
            x = next_x;
            i = i + 1;
        }
        
        var ln_out = listCreate();
        this.ln_f.forward(x, ln_out);
        
        i = 0;
        while (i < seq_len) {
            var l_i = listGet(ln_out, i);
            var logits_i = listCreate();
            this.lm_head.forward(l_i, logits_i);
            listAppend(out_logits, logits_i);
            i = i + 1;
        }
    }
}

class CrossEntropyLoss {
    function forward(list logits, double target_id, Tensor out) void {
        var softmax_layer = Softmax();
        var probs = listCreate();
        softmax_layer.forward(logits, probs);
        
        var prob = listGet(probs, target_id);
        
        var log_prob = Tensor();
        prob.log_t(log_prob);
        
        var neg_one = Tensor();
        neg_one.init(-1.0, listCreate(), "");
        
        log_prob.mul(neg_one, out);
    }
}

class SequenceCrossEntropyLoss {
    function forward(list seq_logits, list target_ids, Tensor out) void {
        var seq_len = listLength(seq_logits);
        var i = 0;
        var total_loss = Tensor();
        total_loss.init(0.0, listCreate(), "");
        
        var ce = CrossEntropyLoss();
        
        while (i < seq_len) {
            var logits_i = listGet(seq_logits, i);
            var target_i = listGet(target_ids, i);
            
            var loss_i = Tensor();
            ce.forward(logits_i, target_i, loss_i);
            
            var next_tot = Tensor();
            total_loss.add(loss_i, next_tot);
            total_loss = next_tot;
            
            i = i + 1;
        }
        
        var div_t = Tensor();
        div_t.init(1.0 * seq_len, listCreate(), "");
        
        total_loss.div(div_t, out);
    }
}
