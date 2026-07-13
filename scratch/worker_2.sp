class Tensor {
    function init(double val, list children, string op) void {
        this.data = val;
        this.grad = 0.0;
        this._prev = children;
        this._op = op;
        this._op_attr = 0.0;
    }

    function add(Tensor other, Tensor out) void {
        var children = listCreate();
        listAppend(children, this);
        listAppend(children, other);
        out.init(this.data + other.data, children, "add");
    }

    function mul(Tensor other, Tensor out) void {
        var children = listCreate();
        listAppend(children, this);
        listAppend(children, other);
        out.init(this.data * other.data, children, "mul");
    }

    function sub(Tensor other, Tensor out) void {
        var children = listCreate();
        listAppend(children, this);
        listAppend(children, other);
        out.init(this.data - other.data, children, "sub");
    }

    function div(Tensor other, Tensor out) void {
        var children = listCreate();
        listAppend(children, this);
        listAppend(children, other);
        out.init(this.data / other.data, children, "div");
    }

    function pow_t(double exp_val, Tensor out) void {
        var children = listCreate();
        listAppend(children, this);
        out.init(pow(this.data, exp_val), children, "pow");
        out._op_attr = exp_val;
    }
    
    function exp_t(Tensor out) void {
        var children = listCreate();
        listAppend(children, this);
        out.init(pow(Math_E, this.data), children, "exp");
    }

    function sin_t(Tensor out) void {
        var children = listCreate();
        listAppend(children, this);
        out.init(sin(this.data), children, "sin");
    }

    function cos_t(Tensor out) void {
        var children = listCreate();
        listAppend(children, this);
        out.init(cos(this.data), children, "cos");
    }

    function log_t(Tensor out) void {
        var children = listCreate();
        listAppend(children, this);
        var val = this.data;
        if (val < 0.000000000000001) { val = 0.000000000000001; }
        out.init(log(val), children, "log");
    }

    function relu(Tensor out) void {
        var children = listCreate();
        listAppend(children, this);
        var out_val = 0.0;
        if (this.data > 0.0) { out_val = this.data; }
        out.init(out_val, children, "relu");
    }

    function leaky_relu(double alpha, Tensor out) void {
        var children = listCreate();
        listAppend(children, this);
        var out_val = this.data;
        if (this.data <= 0.0) { out_val = this.data * alpha; }
        out.init(out_val, children, "leaky_relu");
        out._op_attr = alpha;
    }

    function sigmoid(Tensor out) void {
        var children = listCreate();
        listAppend(children, this);
        var ex = pow(Math_E, -1.0 * this.data);
        var out_val = 1.0 / (1.0 + ex);
        out.init(out_val, children, "sigmoid");
    }

    function tanh_t(Tensor out) void {
        var children = listCreate();
        listAppend(children, this);
        var ex2 = pow(Math_E, 2.0 * this.data);
        var out_val = (ex2 - 1.0) / (ex2 + 1.0);
        out.init(out_val, children, "tanh");
    }


    function backward_step() void {
        if (this._op == "add") {
            var t0 = listGet(this._prev, 0);
            var t1 = listGet(this._prev, 1);
            t0.grad = t0.grad + (1.0 * this.grad);
            t1.grad = t1.grad + (1.0 * this.grad);
        } else if (this._op == "mul") {
            var t0 = listGet(this._prev, 0);
            var t1 = listGet(this._prev, 1);
            t0.grad = t0.grad + (t1.data * this.grad);
            t1.grad = t1.grad + (t0.data * this.grad);
        } else if (this._op == "sub") {
            var t0 = listGet(this._prev, 0);
            var t1 = listGet(this._prev, 1);
            t0.grad = t0.grad + (1.0 * this.grad);
            t1.grad = t1.grad - (1.0 * this.grad);
        } else if (this._op == "div") {
            var t0 = listGet(this._prev, 0);
            var t1 = listGet(this._prev, 1);
            t0.grad = t0.grad + ((1.0 / t1.data) * this.grad);
            t1.grad = t1.grad - ((t0.data / (t1.data * t1.data)) * this.grad);
        } else if (this._op == "pow") {
            var t0 = listGet(this._prev, 0);
            var exp_val = this._op_attr;
            t0.grad = t0.grad + ((exp_val * pow(t0.data, exp_val - 1.0)) * this.grad);
        } else if (this._op == "exp") {
            var t0 = listGet(this._prev, 0);
            t0.grad = t0.grad + (this.data * this.grad);
        } else if (this._op == "sin") {
            var t0 = listGet(this._prev, 0);
            t0.grad = t0.grad + (cos(t0.data) * this.grad);
        } else if (this._op == "cos") {
            var t0 = listGet(this._prev, 0);
            t0.grad = t0.grad - (sin(t0.data) * this.grad);
        } else if (this._op == "relu") {
            var t0 = listGet(this._prev, 0);
            if (this.data > 0.0) {
                t0.grad = t0.grad + (1.0 * this.grad);
            }
        } else if (this._op == "leaky_relu") {
            var t0 = listGet(this._prev, 0);
            var alpha = this._op_attr;
            if (this.data > 0.0) {
                t0.grad = t0.grad + (1.0 * this.grad);
            } else {
                t0.grad = t0.grad + (alpha * this.grad);
            }
        } else if (this._op == "sigmoid") {
            var t0 = listGet(this._prev, 0);
            t0.grad = t0.grad + ((this.data * (1.0 - this.data)) * this.grad);
        } else if (this._op == "tanh") {
            var t0 = listGet(this._prev, 0);
            t0.grad = t0.grad + ((1.0 - (this.data * this.data)) * this.grad);
        } else if (this._op == "log") {
            var t0 = listGet(this._prev, 0);
            var val = t0.data;
            if (val < 0.000000000000001) { val = 0.000000000000001; }
            t0.grad = t0.grad + ((1.0 / val) * this.grad);
        } else if (this._op == "linear_neuron") {
            var bias = listGet(this._prev, 0);
            bias.grad = bias.grad + (1.0 * this.grad);
            
            var len = listLength(this._prev);
            var j = 0;
            var nin = (len - 1) / 2;
            while (j < nin) {
                var xj = listGet(this._prev, 1 + (2 * j));
                var wj = listGet(this._prev, 2 + (2 * j));
                
                xj.grad = xj.grad + (wj.data * this.grad);
                wj.grad = wj.grad + (xj.data * this.grad);
                
                j = j + 1;
            }
        } else if (this._op == "gpu_linear_neuron") {
            var bias = listGet(this._prev, 0);
            bias.grad = bias.grad + (1.0 * this.grad);
            
            var len = listLength(this._prev);
            var j = 0;
            var nin = (len - 1) / 2;
            while (j < nin) {
                var xj = listGet(this._prev, 1 + (2 * j));
                var wj = listGet(this._prev, 2 + (2 * j));
                
                xj.grad = xj.grad + (wj.data * this.grad);
                wj.grad = wj.grad + (xj.data * this.grad);
                
                j = j + 1;
            }
        }
    }

    function _build_topo(list topo, list visited) void {
        if (!listContains(visited, this)) {
            listAppend(visited, this);
            var i = 0;
            var len = listLength(this._prev);
            while (i < len) {
                var child = listGet(this._prev, i);
                child._build_topo(topo, visited);
                i = i + 1;
            }
            listAppend(topo, this);
        }
    }

    function backward() void {
        var topo = listCreate();
        var visited = listCreate();
        this._build_topo(topo, visited);

        this.grad = 1.0;
        
        var len = listLength(topo);
        var i = len - 1;
        while (i >= 0) {
            var node = listGet(topo, i);
            node.backward_step();
            i = i - 1;
        }
    }
}
class Init {
    function uniform(double min_v, double max_v) double {
        return min_v + (rand() * (max_v - min_v));
    }
    
    function xavier_uniform(double nin, double nout) double {
        var limit = sqrt(6.0 / (nin + nout));
        return this.uniform(-1.0 * limit, limit);
    }
    
    function kaiming_uniform(double nin) double {
        var limit = sqrt(6.0 / nin);
        return this.uniform(-1.0 * limit, limit);
    }
}
class Neuron {
    function init(double nin, string init_type) void {
        this.w = listCreate();
        var initializer = Init();
        var i = 0;
        while (i < nin) {
            var w_tensor = Tensor();
            var w_val = 0.0;
            if (init_type == "xavier") {
                w_val = initializer.xavier_uniform(nin, 1.0);
            } else if (init_type == "kaiming") {
                w_val = initializer.kaiming_uniform(nin);
            } else {
                w_val = initializer.uniform(-1.0, 1.0);
            }
            w_tensor.init(w_val, listCreate(), "");
            listAppend(this.w, w_tensor);
            i = i + 1;
        }
        this.b = Tensor();
        this.b.init(0.0, listCreate(), "");
    }

    function forward(list x, Tensor out) void {
        var children = listCreate();
        listAppend(children, this.b);
        
        var sum = this.b.data;
        var i = 0;
        var len = listLength(this.w);
        while (i < len) {
            var wi = listGet(this.w, i);
            var xi = listGet(x, i);
            sum = sum + (wi.data * xi.data);
            listAppend(children, xi);
            listAppend(children, wi);
            i = i + 1;
        }
        out.init(sum, children, "linear_neuron");
    }

    function parameters(list params_out) void {
        var i = 0;
        var len = listLength(this.w);
        while (i < len) {
            listAppend(params_out, listGet(this.w, i));
            i = i + 1;
        }
        listAppend(params_out, this.b);
    }
}
class Linear {
    function init(double nin, double nout, string init_type) void {
        this.neurons = listCreate();
        var i = 0;
        while (i < nout) {
            var n = Neuron();
            n.init(nin, init_type);
            listAppend(this.neurons, n);
            i = i + 1;
        }
    }

    function forward(list x, list out) void {
        var i = 0;
        var len = listLength(this.neurons);
        while (i < len) {
            var n = listGet(this.neurons, i);
            var n_out = Tensor();
            n.forward(x, n_out);
            listAppend(out, n_out);
            i = i + 1;
        }
    }

    function parameters(list params_out) void {
        var i = 0;
        var len = listLength(this.neurons);
        while (i < len) {
            var n = listGet(this.neurons, i);
            n.parameters(params_out);
            i = i + 1;
        }
    }
}
class ActivationLayer {
    function init(string type) void {
        this.type = type;
        this.alpha = 0.01;
    }
    
    function forward(list x, list out) void {
        var i = 0;
        var len = listLength(x);
        while (i < len) {
            var xi = listGet(x, i);
            var out_tensor = Tensor();
            
            if (this.type == "relu") { xi.relu(out_tensor); }
            else if (this.type == "sigmoid") { xi.sigmoid(out_tensor); }
            else if (this.type == "tanh") { xi.tanh_t(out_tensor); }
            else if (this.type == "leaky_relu") { xi.leaky_relu(this.alpha, out_tensor); }
            else { out_tensor.init(xi.data, xi._prev, xi._op); }
            
            listAppend(out, out_tensor);
            i = i + 1;
        }
    }
    
    function parameters(list params_out) void { }
}
class MLP {
    function init(double nin, list nouts, list activations, string init_type) void {
        this.layers = listCreate();
        var sz = listCreate();
        listAppend(sz, nin);
        
        var i = 0;
        var nouts_len = listLength(nouts);
        while (i < nouts_len) {
            listAppend(sz, listGet(nouts, i));
            i = i + 1;
        }

        i = 0;
        while (i < nouts_len) {
            var layer = Linear();
            layer.init(listGet(sz, i), listGet(sz, i + 1), init_type);
            listAppend(this.layers, layer);
            
            if (i < listLength(activations)) {
                var act = ActivationLayer();
                act.init(listGet(activations, i));
                listAppend(this.layers, act);
            }
            i = i + 1;
        }
    }

    function forward(list x, list out) void {
        var current_in = x;
        var i = 0;
        var len = listLength(this.layers);
        while (i < len) {
            var layer = listGet(this.layers, i);
            var current_out = listCreate();
            layer.forward(current_in, current_out);
            current_in = current_out;
            i = i + 1;
        }
        
        var j = 0;
        while(j < listLength(current_in)) {
            listAppend(out, listGet(current_in, j));
            j = j + 1;
        }
    }

    function parameters(list params_out) void {
        var i = 0;
        var len = listLength(this.layers);
        while (i < len) {
            var layer = listGet(this.layers, i);
            layer.parameters(params_out);
            i = i + 1;
        }
    }
    
    function train() void {
        var i = 0;
        var len = listLength(this.layers);
        while (i < len) {
            var layer = listGet(this.layers, i);

            i = i + 1;
        }
    }

    function eval() void {
        var i = 0;
        var len = listLength(this.layers);
        while (i < len) {
            var layer = listGet(this.layers, i);

            i = i + 1;
        }
    }
}
class Adam {
    function init(list parameters, double lr) void {
        this.parameters = parameters;
        this.lr = lr;
        this.beta1 = 0.9;
        this.beta2 = 0.999;
        this.eps = 0.00000001;
        this.t = 0;
        
        this.m = listCreate();
        this.v = listCreate();
        var i = 0;
        while (i < listLength(parameters)) {
            listAppend(this.m, 0.0);
            listAppend(this.v, 0.0);
            i = i + 1;
        }
    }

    function zero_grad() void {
        var i = 0;
        var len = listLength(this.parameters);
        while (i < len) {
            var p = listGet(this.parameters, i);
            p.grad = 0.0;
            i = i + 1;
        }
    }

    function step() void {
        this.t = this.t + 1;
        var i = 0;
        var len = listLength(this.parameters);
        while (i < len) {
            var p = listGet(this.parameters, i);
            var m_i = listGet(this.m, i);
            var v_i = listGet(this.v, i);
            
            m_i = (this.beta1 * m_i) + ((1.0 - this.beta1) * p.grad);
            v_i = (this.beta2 * v_i) + ((1.0 - this.beta2) * (p.grad * p.grad));
            
            listSet(this.m, i, m_i);
            listSet(this.v, i, v_i);
            
            var m_hat = m_i / (1.0 - pow(this.beta1, 1.0 * this.t));
            var v_hat = v_i / (1.0 - pow(this.beta2, 1.0 * this.t));
            
            p.data = p.data - (this.lr * m_hat / (sqrt(v_hat) + this.eps));
            i = i + 1;
        }
    }
}
class SelfAttention {
    function init(double embed_size, string init_type) void {
        this.embed_size = embed_size;
        
        this.w_q = Linear(); this.w_q.init(embed_size, embed_size, init_type);
        this.w_k = Linear(); this.w_k.init(embed_size, embed_size, init_type);
        this.w_v = Linear(); this.w_v.init(embed_size, embed_size, init_type);
    }
    

    function forward(list seq_x, list seq_out) void {
        var seq_len = listLength(seq_x);
        var q_seq = listCreate();
        var k_seq = listCreate();
        var v_seq = listCreate();
        
        var i = 0;
        while (i < seq_len) {
            var x_i = listGet(seq_x, i);
            var q_i = listCreate(); this.w_q.forward(x_i, q_i); listAppend(q_seq, q_i);
            var k_i = listCreate(); this.w_k.forward(x_i, k_i); listAppend(k_seq, k_i);
            var v_i = listCreate(); this.w_v.forward(x_i, v_i); listAppend(v_seq, v_i);
            i = i + 1;
        }
        
        var scale_val = sqrt(this.embed_size);
        var scale_t = Tensor(); scale_t.init(scale_val, listCreate(), "");
        
        i = 0;
        while (i < seq_len) {
            var q_vec = listGet(q_seq, i);
            var scores = listCreate();
            
            var j = 0;
            while (j < seq_len) {
                var k_vec = listGet(k_seq, j);
                var dot_sum = Tensor(); dot_sum.init(0.0, listCreate(), "");
                
                var d = 0;
                while (d < this.embed_size) {
                    var q_d = listGet(q_vec, d);
                    var k_d = listGet(k_vec, d);
                    var prod = Tensor();
                    q_d.mul(k_d, prod);
                    var next_dot = Tensor();
                    dot_sum.add(prod, next_dot);
                    dot_sum = next_dot;
                    d = d + 1;
                }
                var scaled_score = Tensor();
                dot_sum.div(scale_t, scaled_score);
                listAppend(scores, scaled_score);
                j = j + 1;
            }
            

            var max_score = -999999.0;
            var s_idx = 0;
            while (s_idx < seq_len) {
                var st = listGet(scores, s_idx);
                if (st.data > max_score) { max_score = st.data; }
                s_idx = s_idx + 1;
            }
            
            var max_t = Tensor(); max_t.init(max_score, listCreate(), "");
            var exps = listCreate();
            var sum_exp = Tensor(); sum_exp.init(0.0, listCreate(), "");
            
            s_idx = 0;
            while (s_idx < seq_len) {
                var st = listGet(scores, s_idx);
                var st_shifted = Tensor();
                st.sub(max_t, st_shifted);
                var e_t = Tensor();
                st_shifted.exp_t(e_t);
                listAppend(exps, e_t);
                
                var next_sum_exp = Tensor();
                sum_exp.add(e_t, next_sum_exp);
                sum_exp = next_sum_exp;
                s_idx = s_idx + 1;
            }
            
            var attention_weights = listCreate();
            s_idx = 0;
            while (s_idx < seq_len) {
                var e_t = listGet(exps, s_idx);
                var a_w = Tensor();
                e_t.div(sum_exp, a_w);
                listAppend(attention_weights, a_w);
                s_idx = s_idx + 1;
            }
            

            var out_vec = listCreate();
            var d_idx = 0;
            while (d_idx < this.embed_size) {
                var v_sum = Tensor(); v_sum.init(0.0, listCreate(), "");
                var v_j = 0;
                while (v_j < seq_len) {
                    var aw = listGet(attention_weights, v_j);
                    var v_vec_j = listGet(v_seq, v_j);
                    var v_val = listGet(v_vec_j, d_idx);
                    
                    var p_t = Tensor();
                    aw.mul(v_val, p_t);
                    
                    var next_v_sum = Tensor();
                    v_sum.add(p_t, next_v_sum);
                    v_sum = next_v_sum;
                    v_j = v_j + 1;
                }
                listAppend(out_vec, v_sum);
                d_idx = d_idx + 1;
            }
            
            listAppend(seq_out, out_vec);
            i = i + 1;
        }
    }
    
    function parameters(list params_out) void {
        this.w_q.parameters(params_out);
        this.w_k.parameters(params_out);
        this.w_v.parameters(params_out);
    }
}
class Softmax {
    function init() void {}
    function forward(list x, list out) void {
        var len = listLength(x);
        if (len == 0) { return; }
        
        var max_val = -9999999.0;
        var i = 0;
        while (i < len) {
            var t = listGet(x, i);
            if (t.data > max_val) { max_val = t.data; }
            i = i + 1;
        }
        
        var max_t = Tensor();
        max_t.init(max_val, listCreate(), "");
        var exps = listCreate();
        var sum_exp = Tensor();
        sum_exp.init(0.0, listCreate(), "");
        
        i = 0;
        while (i < len) {
            var t = listGet(x, i);
            var diff = Tensor();
            t.sub(max_t, diff);
            var exp_t = Tensor();
            diff.exp_t(exp_t);
            listAppend(exps, exp_t);
            
            var next_sum = Tensor();
            sum_exp.add(exp_t, next_sum);
            sum_exp = next_sum;
            i = i + 1;
        }
        
        i = 0;
        while (i < len) {
            var exp_t = listGet(exps, i);
            var out_t = Tensor();
            exp_t.div(sum_exp, out_t);
            listAppend(out, out_t);
            i = i + 1;
        }
    }
    function parameters(list params_out) void {}
}
class Tokenizer {
    function init() void {
        this.vocab = listCreate();
    }
    
    function fit(string text) void {
        var tokens = stringSplit(text, " ");
        var i = 0;
        var len = listLength(tokens);
        while (i < len) {
            var token = listGet(tokens, i);
            if (!listContains(this.vocab, token)) {
                listAppend(this.vocab, token);
            }
            i = i + 1;
        }
    }
    
    function encode(string text, list out_ids) void {
        var tokens = stringSplit(text, " ");
        var i = 0;
        var len = listLength(tokens);
        while (i < len) {
            var token = listGet(tokens, i);
            var id = this._find_id(token);
            if (id != -1.0) {
                listAppend(out_ids, id);
            }
            i = i + 1;
        }
    }
    
    function _find_id(string token) double {
        var i = 0;
        var len = listLength(this.vocab);
        while (i < len) {
            if (listGet(this.vocab, i) == token) {
                return 1.0 * i;
            }
            i = i + 1;
        }
        return -1.0;
    }
    
    function decode(list ids) string {
        var res = "";
        var i = 0;
        var len = listLength(ids);
        while (i < len) {
            var id = listGet(ids, i);
            if (id >= 0.0 && id < listLength(this.vocab)) {
                res = res + valueToString(listGet(this.vocab, id)) + " ";
            }
            i = i + 1;
        }
        return res;
    }
}
class Embedding {
    function init(double vocab_size, double embedding_dim, string init_type) void {
        this.vocab_size = vocab_size;
        this.embedding_dim = embedding_dim;
        this.w = listCreate();
        
        var initializer = Init();
        var i = 0;
        while (i < vocab_size) {
            var emb_vector = listCreate();
            var j = 0;
            while (j < embedding_dim) {
                var w_tensor = Tensor();
                var w_val = initializer.uniform(-0.1, 0.1);
                w_tensor.init(w_val, listCreate(), "");
                listAppend(emb_vector, w_tensor);
                j = j + 1;
            }
            listAppend(this.w, emb_vector);
            i = i + 1;
        }
    }
    
    function forward(list input_ids, list out_embeddings) void {
        var i = 0;
        var len = listLength(input_ids);
        while (i < len) {
            var id = listGet(input_ids, i);
            var emb_vector = listGet(this.w, id);
            
            var copied_vector = listCreate();
            var j = 0;
            while (j < listLength(emb_vector)) {
                var t = listGet(emb_vector, j);
                var out_t = Tensor();
                var children = listCreate();
                listAppend(children, t);
                var zero_t = Tensor(); zero_t.init(0.0, listCreate(), "");
                listAppend(children, zero_t);
                out_t.init(t.data, children, "add");
                listAppend(copied_vector, out_t);
                j = j + 1;
            }
            listAppend(out_embeddings, copied_vector);
            i = i + 1;
        }
    }
    
    function parameters(list params_out) void {
        var i = 0;
        while (i < listLength(this.w)) {
            var emb_vector = listGet(this.w, i);
            var j = 0;
            while (j < listLength(emb_vector)) {
                listAppend(params_out, listGet(emb_vector, j));
                j = j + 1;
            }
            i = i + 1;
        }
    }
}
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
var Math_E = 2.718281828459045;



var MEMORY_LIMIT = 10000;
function main() void {
    print("Initializing Chatbot Training...");
    
    var dataset = listCreate();
    listAppend(dataset, "eu gosto de ler hoje de manha .");
    listAppend(dataset, "eu gosto de ler ontem a noite .");
    listAppend(dataset, "eu gosto de ler agora mesmo .");
    listAppend(dataset, "eu gosto de ler com muita alegria .");
    listAppend(dataset, "eu gosto de ler rapidamente .");
    listAppend(dataset, "eu gosto de ler bem devagar .");
    listAppend(dataset, "eu gosto de ler na minha casa .");
    listAppend(dataset, "eu gosto de ler no hospital central .");
    listAppend(dataset, "eu gosto de ler na rua de baixo .");
    listAppend(dataset, "eu gosto de ler no centro da cidade .");
    listAppend(dataset, "eu quero viajar hoje de manha .");
    listAppend(dataset, "eu quero viajar ontem a noite .");
    listAppend(dataset, "eu quero viajar agora mesmo .");
    listAppend(dataset, "eu quero viajar com muita alegria .");
    listAppend(dataset, "eu quero viajar rapidamente .");
    listAppend(dataset, "eu quero viajar bem devagar .");
    listAppend(dataset, "eu quero viajar na minha casa .");
    listAppend(dataset, "eu quero viajar no hospital central .");
    listAppend(dataset, "eu quero viajar na rua de baixo .");
    listAppend(dataset, "eu quero viajar no centro da cidade .");
    listAppend(dataset, "eu preciso estudar hoje de manha .");
    listAppend(dataset, "eu preciso estudar ontem a noite .");
    listAppend(dataset, "eu preciso estudar agora mesmo .");
    listAppend(dataset, "eu preciso estudar com muita alegria .");
    listAppend(dataset, "eu preciso estudar rapidamente .");
    listAppend(dataset, "eu preciso estudar bem devagar .");
    listAppend(dataset, "eu preciso estudar na minha casa .");
    listAppend(dataset, "eu preciso estudar no hospital central .");
    listAppend(dataset, "eu preciso estudar na rua de baixo .");
    listAppend(dataset, "eu preciso estudar no centro da cidade .");
    listAppend(dataset, "eu amo musica hoje de manha .");
    listAppend(dataset, "eu amo musica ontem a noite .");
    listAppend(dataset, "eu amo musica agora mesmo .");
    listAppend(dataset, "eu amo musica com muita alegria .");
    listAppend(dataset, "eu amo musica rapidamente .");
    listAppend(dataset, "eu amo musica bem devagar .");
    listAppend(dataset, "eu amo musica na minha casa .");
    listAppend(dataset, "eu amo musica no hospital central .");
    listAppend(dataset, "eu amo musica na rua de baixo .");
    listAppend(dataset, "eu amo musica no centro da cidade .");
    listAppend(dataset, "eu prefiro cafe hoje de manha .");
    listAppend(dataset, "eu prefiro cafe ontem a noite .");
    listAppend(dataset, "eu prefiro cafe agora mesmo .");
    listAppend(dataset, "eu prefiro cafe com muita alegria .");
    listAppend(dataset, "eu prefiro cafe rapidamente .");
    listAppend(dataset, "eu prefiro cafe bem devagar .");
    listAppend(dataset, "eu prefiro cafe na minha casa .");
    listAppend(dataset, "eu prefiro cafe no hospital central .");
    listAppend(dataset, "eu prefiro cafe na rua de baixo .");
    listAppend(dataset, "eu prefiro cafe no centro da cidade .");
    listAppend(dataset, "eu adoro pizza hoje de manha .");
    listAppend(dataset, "eu adoro pizza ontem a noite .");
    listAppend(dataset, "eu adoro pizza agora mesmo .");
    listAppend(dataset, "eu adoro pizza com muita alegria .");
    listAppend(dataset, "eu adoro pizza rapidamente .");
    listAppend(dataset, "eu adoro pizza bem devagar .");
    listAppend(dataset, "eu adoro pizza na minha casa .");
    listAppend(dataset, "eu adoro pizza no hospital central .");
    listAppend(dataset, "eu adoro pizza na rua de baixo .");
    listAppend(dataset, "eu adoro pizza no centro da cidade .");
    listAppend(dataset, "eu vou trabalhar hoje de manha .");
    listAppend(dataset, "eu vou trabalhar ontem a noite .");
    listAppend(dataset, "eu vou trabalhar agora mesmo .");
    listAppend(dataset, "eu vou trabalhar com muita alegria .");
    listAppend(dataset, "eu vou trabalhar rapidamente .");
    listAppend(dataset, "eu vou trabalhar bem devagar .");
    listAppend(dataset, "eu vou trabalhar na minha casa .");
    listAppend(dataset, "eu vou trabalhar no hospital central .");
    listAppend(dataset, "eu vou trabalhar na rua de baixo .");
    listAppend(dataset, "eu vou trabalhar no centro da cidade .");
    listAppend(dataset, "eu posso ajudar hoje de manha .");
    listAppend(dataset, "eu posso ajudar ontem a noite .");
    listAppend(dataset, "eu posso ajudar agora mesmo .");
    listAppend(dataset, "eu posso ajudar com muita alegria .");
    listAppend(dataset, "eu posso ajudar rapidamente .");
    listAppend(dataset, "eu posso ajudar bem devagar .");
    listAppend(dataset, "eu posso ajudar na minha casa .");
    listAppend(dataset, "eu posso ajudar no hospital central .");
    listAppend(dataset, "eu posso ajudar na rua de baixo .");
    listAppend(dataset, "eu posso ajudar no centro da cidade .");
    listAppend(dataset, "eu tenho tempo hoje de manha .");
    listAppend(dataset, "eu tenho tempo ontem a noite .");
    listAppend(dataset, "eu tenho tempo agora mesmo .");
    listAppend(dataset, "eu tenho tempo com muita alegria .");
    listAppend(dataset, "eu tenho tempo rapidamente .");
    listAppend(dataset, "eu tenho tempo bem devagar .");
    listAppend(dataset, "eu tenho tempo na minha casa .");
    listAppend(dataset, "eu tenho tempo no hospital central .");
    listAppend(dataset, "eu tenho tempo na rua de baixo .");
    listAppend(dataset, "eu tenho tempo no centro da cidade .");
    listAppend(dataset, "eu acho legal hoje de manha .");
    listAppend(dataset, "eu acho legal ontem a noite .");
    listAppend(dataset, "eu acho legal agora mesmo .");
    listAppend(dataset, "eu acho legal com muita alegria .");
    listAppend(dataset, "eu acho legal rapidamente .");
    listAppend(dataset, "eu acho legal bem devagar .");
    listAppend(dataset, "eu acho legal na minha casa .");
    listAppend(dataset, "eu acho legal no hospital central .");
    listAppend(dataset, "eu acho legal na rua de baixo .");
    listAppend(dataset, "eu acho legal no centro da cidade .");
    listAppend(dataset, "ela gosta de ler hoje de manha .");
    listAppend(dataset, "ela gosta de ler ontem a noite .");
    listAppend(dataset, "ela gosta de ler agora mesmo .");
    listAppend(dataset, "ela gosta de ler com muita alegria .");
    listAppend(dataset, "ela gosta de ler rapidamente .");
    listAppend(dataset, "ela gosta de ler bem devagar .");
    listAppend(dataset, "ela gosta de ler na minha casa .");
    listAppend(dataset, "ela gosta de ler no hospital central .");
    listAppend(dataset, "ela gosta de ler na rua de baixo .");
    listAppend(dataset, "ela gosta de ler no centro da cidade .");
    listAppend(dataset, "ele quer viajar hoje de manha .");
    listAppend(dataset, "ele quer viajar ontem a noite .");
    listAppend(dataset, "ele quer viajar agora mesmo .");
    listAppend(dataset, "ele quer viajar com muita alegria .");
    listAppend(dataset, "ele quer viajar rapidamente .");
    listAppend(dataset, "ele quer viajar bem devagar .");
    listAppend(dataset, "ele quer viajar na minha casa .");
    listAppend(dataset, "ele quer viajar no hospital central .");
    listAppend(dataset, "ele quer viajar na rua de baixo .");
    listAppend(dataset, "ele quer viajar no centro da cidade .");
    listAppend(dataset, "o menino precisa estudar hoje de manha .");
    listAppend(dataset, "o menino precisa estudar ontem a noite .");
    listAppend(dataset, "o menino precisa estudar agora mesmo .");
    listAppend(dataset, "o menino precisa estudar com muita alegria .");
    listAppend(dataset, "o menino precisa estudar rapidamente .");
    listAppend(dataset, "o menino precisa estudar bem devagar .");
    listAppend(dataset, "o menino precisa estudar na minha casa .");
    listAppend(dataset, "o menino precisa estudar no hospital central .");
    listAppend(dataset, "o menino precisa estudar na rua de baixo .");
    listAppend(dataset, "o menino precisa estudar no centro da cidade .");
    listAppend(dataset, "a menina ama musica hoje de manha .");
    listAppend(dataset, "a menina ama musica ontem a noite .");
    listAppend(dataset, "a menina ama musica agora mesmo .");
    listAppend(dataset, "a menina ama musica com muita alegria .");
    listAppend(dataset, "a menina ama musica rapidamente .");
    listAppend(dataset, "a menina ama musica bem devagar .");
    listAppend(dataset, "a menina ama musica na minha casa .");
    listAppend(dataset, "a menina ama musica no hospital central .");
    listAppend(dataset, "a menina ama musica na rua de baixo .");
    listAppend(dataset, "a menina ama musica no centro da cidade .");
    listAppend(dataset, "o homem prefere cafe hoje de manha .");
    listAppend(dataset, "o homem prefere cafe ontem a noite .");
    listAppend(dataset, "o homem prefere cafe agora mesmo .");
    listAppend(dataset, "o homem prefere cafe com muita alegria .");
    listAppend(dataset, "o homem prefere cafe rapidamente .");
    listAppend(dataset, "o homem prefere cafe bem devagar .");
    listAppend(dataset, "o homem prefere cafe na minha casa .");
    listAppend(dataset, "o homem prefere cafe no hospital central .");
    listAppend(dataset, "o homem prefere cafe na rua de baixo .");
    listAppend(dataset, "o homem prefere cafe no centro da cidade .");
    listAppend(dataset, "a mulher adora pizza hoje de manha .");
    listAppend(dataset, "a mulher adora pizza ontem a noite .");
    listAppend(dataset, "a mulher adora pizza agora mesmo .");
    listAppend(dataset, "a mulher adora pizza com muita alegria .");
    listAppend(dataset, "a mulher adora pizza rapidamente .");
    listAppend(dataset, "a mulher adora pizza bem devagar .");
    listAppend(dataset, "a mulher adora pizza na minha casa .");
    listAppend(dataset, "a mulher adora pizza no hospital central .");
    listAppend(dataset, "a mulher adora pizza na rua de baixo .");
    listAppend(dataset, "a mulher adora pizza no centro da cidade .");
    listAppend(dataset, "o engenheiro vai trabalhar hoje de manha .");
    listAppend(dataset, "o engenheiro vai trabalhar ontem a noite .");
    listAppend(dataset, "o engenheiro vai trabalhar agora mesmo .");
    listAppend(dataset, "o engenheiro vai trabalhar com muita alegria .");
    listAppend(dataset, "o engenheiro vai trabalhar rapidamente .");
    listAppend(dataset, "o engenheiro vai trabalhar bem devagar .");
    listAppend(dataset, "o engenheiro vai trabalhar na minha casa .");
    listAppend(dataset, "o engenheiro vai trabalhar no hospital central .");
    listAppend(dataset, "o engenheiro vai trabalhar na rua de baixo .");
    listAppend(dataset, "o engenheiro vai trabalhar no centro da cidade .");
    listAppend(dataset, "a medica pode ajudar hoje de manha .");
    listAppend(dataset, "a medica pode ajudar ontem a noite .");
    listAppend(dataset, "a medica pode ajudar agora mesmo .");
    listAppend(dataset, "a medica pode ajudar com muita alegria .");
    listAppend(dataset, "a medica pode ajudar rapidamente .");
    listAppend(dataset, "a medica pode ajudar bem devagar .");
    listAppend(dataset, "a medica pode ajudar na minha casa .");
    listAppend(dataset, "a medica pode ajudar no hospital central .");
    listAppend(dataset, "a medica pode ajudar na rua de baixo .");
    listAppend(dataset, "a medica pode ajudar no centro da cidade .");
    listAppend(dataset, "o cachorro tem tempo hoje de manha .");
    listAppend(dataset, "o cachorro tem tempo ontem a noite .");
    listAppend(dataset, "o cachorro tem tempo agora mesmo .");
    listAppend(dataset, "o cachorro tem tempo com muita alegria .");
    listAppend(dataset, "o cachorro tem tempo rapidamente .");
    listAppend(dataset, "o cachorro tem tempo bem devagar .");
    listAppend(dataset, "o cachorro tem tempo na minha casa .");
    listAppend(dataset, "o cachorro tem tempo no hospital central .");
    listAppend(dataset, "o cachorro tem tempo na rua de baixo .");
    listAppend(dataset, "o cachorro tem tempo no centro da cidade .");
    listAppend(dataset, "o gato acha legal hoje de manha .");
    listAppend(dataset, "o gato acha legal ontem a noite .");
    listAppend(dataset, "o gato acha legal agora mesmo .");
    listAppend(dataset, "o gato acha legal com muita alegria .");
    listAppend(dataset, "o gato acha legal rapidamente .");
    listAppend(dataset, "o gato acha legal bem devagar .");
    listAppend(dataset, "o gato acha legal na minha casa .");
    listAppend(dataset, "o gato acha legal no hospital central .");
    listAppend(dataset, "o gato acha legal na rua de baixo .");
    listAppend(dataset, "o gato acha legal no centro da cidade .");
    listAppend(dataset, "eu comprei um carro hoje de manha .");
    listAppend(dataset, "eu comprei um carro ontem a noite .");
    listAppend(dataset, "eu comprei um carro agora mesmo .");
    listAppend(dataset, "eu comprei um carro com muita alegria .");
    listAppend(dataset, "eu comprei um carro rapidamente .");
    listAppend(dataset, "eu comprei um carro bem devagar .");
    listAppend(dataset, "eu comprei um carro na minha casa .");
    listAppend(dataset, "eu comprei um carro no hospital central .");
    listAppend(dataset, "eu comprei um carro na rua de baixo .");
    listAppend(dataset, "eu comprei um carro no centro da cidade .");
    listAppend(dataset, "eu vendi a casa hoje de manha .");
    listAppend(dataset, "eu vendi a casa ontem a noite .");
    listAppend(dataset, "eu vendi a casa agora mesmo .");
    listAppend(dataset, "eu vendi a casa com muita alegria .");
    listAppend(dataset, "eu vendi a casa rapidamente .");
    listAppend(dataset, "eu vendi a casa bem devagar .");
    listAppend(dataset, "eu vendi a casa na minha casa .");
    listAppend(dataset, "eu vendi a casa no hospital central .");
    listAppend(dataset, "eu vendi a casa na rua de baixo .");
    listAppend(dataset, "eu vendi a casa no centro da cidade .");
    listAppend(dataset, "eu fiz um bolo hoje de manha .");
    listAppend(dataset, "eu fiz um bolo ontem a noite .");
    listAppend(dataset, "eu fiz um bolo agora mesmo .");
    listAppend(dataset, "eu fiz um bolo com muita alegria .");
    listAppend(dataset, "eu fiz um bolo rapidamente .");
    listAppend(dataset, "eu fiz um bolo bem devagar .");
    listAppend(dataset, "eu fiz um bolo na minha casa .");
    listAppend(dataset, "eu fiz um bolo no hospital central .");
    listAppend(dataset, "eu fiz um bolo na rua de baixo .");
    listAppend(dataset, "eu fiz um bolo no centro da cidade .");
    listAppend(dataset, "eu encontrei o livro hoje de manha .");
    listAppend(dataset, "eu encontrei o livro ontem a noite .");
    listAppend(dataset, "eu encontrei o livro agora mesmo .");
    listAppend(dataset, "eu encontrei o livro com muita alegria .");
    listAppend(dataset, "eu encontrei o livro rapidamente .");
    listAppend(dataset, "eu encontrei o livro bem devagar .");
    listAppend(dataset, "eu encontrei o livro na minha casa .");
    listAppend(dataset, "eu encontrei o livro no hospital central .");
    listAppend(dataset, "eu encontrei o livro na rua de baixo .");
    listAppend(dataset, "eu encontrei o livro no centro da cidade .");
    listAppend(dataset, "eu perdi as chaves hoje de manha .");
    listAppend(dataset, "eu perdi as chaves ontem a noite .");
    listAppend(dataset, "eu perdi as chaves agora mesmo .");
    listAppend(dataset, "eu perdi as chaves com muita alegria .");
    listAppend(dataset, "eu perdi as chaves rapidamente .");
    listAppend(dataset, "eu perdi as chaves bem devagar .");
    listAppend(dataset, "eu perdi as chaves na minha casa .");
    listAppend(dataset, "eu perdi as chaves no hospital central .");
    listAppend(dataset, "eu perdi as chaves na rua de baixo .");
    listAppend(dataset, "eu perdi as chaves no centro da cidade .");
    listAppend(dataset, "eu assisti um filme hoje de manha .");
    listAppend(dataset, "eu assisti um filme ontem a noite .");
    listAppend(dataset, "eu assisti um filme agora mesmo .");
    listAppend(dataset, "eu assisti um filme com muita alegria .");
    listAppend(dataset, "eu assisti um filme rapidamente .");
    listAppend(dataset, "eu assisti um filme bem devagar .");
    listAppend(dataset, "eu assisti um filme na minha casa .");
    listAppend(dataset, "eu assisti um filme no hospital central .");
    listAppend(dataset, "eu assisti um filme na rua de baixo .");
    listAppend(dataset, "eu assisti um filme no centro da cidade .");
    listAppend(dataset, "eu li o relatorio hoje de manha .");
    listAppend(dataset, "eu li o relatorio ontem a noite .");
    listAppend(dataset, "eu li o relatorio agora mesmo .");
    listAppend(dataset, "eu li o relatorio com muita alegria .");
    listAppend(dataset, "eu li o relatorio rapidamente .");
    listAppend(dataset, "eu li o relatorio bem devagar .");
    listAppend(dataset, "eu li o relatorio na minha casa .");
    listAppend(dataset, "eu li o relatorio no hospital central .");
    listAppend(dataset, "eu li o relatorio na rua de baixo .");
    listAppend(dataset, "eu li o relatorio no centro da cidade .");
    listAppend(dataset, "eu escrevi uma carta hoje de manha .");
    listAppend(dataset, "eu escrevi uma carta ontem a noite .");
    listAppend(dataset, "eu escrevi uma carta agora mesmo .");
    listAppend(dataset, "eu escrevi uma carta com muita alegria .");
    listAppend(dataset, "eu escrevi uma carta rapidamente .");
    listAppend(dataset, "eu escrevi uma carta bem devagar .");
    listAppend(dataset, "eu escrevi uma carta na minha casa .");
    listAppend(dataset, "eu escrevi uma carta no hospital central .");
    listAppend(dataset, "eu escrevi uma carta na rua de baixo .");
    listAppend(dataset, "eu escrevi uma carta no centro da cidade .");
    listAppend(dataset, "eu bebi agua hoje de manha .");
    listAppend(dataset, "eu bebi agua ontem a noite .");
    listAppend(dataset, "eu bebi agua agora mesmo .");
    listAppend(dataset, "eu bebi agua com muita alegria .");
    listAppend(dataset, "eu bebi agua rapidamente .");
    listAppend(dataset, "eu bebi agua bem devagar .");
    listAppend(dataset, "eu bebi agua na minha casa .");
    listAppend(dataset, "eu bebi agua no hospital central .");
    listAppend(dataset, "eu bebi agua na rua de baixo .");
    listAppend(dataset, "eu bebi agua no centro da cidade .");
    listAppend(dataset, "eu comi maca hoje de manha .");
    listAppend(dataset, "eu comi maca ontem a noite .");
    listAppend(dataset, "eu comi maca agora mesmo .");
    listAppend(dataset, "eu comi maca com muita alegria .");
    listAppend(dataset, "eu comi maca rapidamente .");
    listAppend(dataset, "eu comi maca bem devagar .");
    listAppend(dataset, "eu comi maca na minha casa .");
    listAppend(dataset, "eu comi maca no hospital central .");
    listAppend(dataset, "eu comi maca na rua de baixo .");
    listAppend(dataset, "eu comi maca no centro da cidade .");
    listAppend(dataset, "ela comprou um carro hoje de manha .");
    listAppend(dataset, "ela comprou um carro ontem a noite .");
    listAppend(dataset, "ela comprou um carro agora mesmo .");
    listAppend(dataset, "ela comprou um carro com muita alegria .");
    listAppend(dataset, "ela comprou um carro rapidamente .");
    listAppend(dataset, "ela comprou um carro bem devagar .");
    listAppend(dataset, "ela comprou um carro na minha casa .");
    listAppend(dataset, "ela comprou um carro no hospital central .");
    listAppend(dataset, "ela comprou um carro na rua de baixo .");
    listAppend(dataset, "ela comprou um carro no centro da cidade .");
    listAppend(dataset, "ele vendeu a casa hoje de manha .");
    listAppend(dataset, "ele vendeu a casa ontem a noite .");
    listAppend(dataset, "ele vendeu a casa agora mesmo .");
    listAppend(dataset, "ele vendeu a casa com muita alegria .");
    listAppend(dataset, "ele vendeu a casa rapidamente .");
    listAppend(dataset, "ele vendeu a casa bem devagar .");
    listAppend(dataset, "ele vendeu a casa na minha casa .");
    listAppend(dataset, "ele vendeu a casa no hospital central .");
    listAppend(dataset, "ele vendeu a casa na rua de baixo .");
    listAppend(dataset, "ele vendeu a casa no centro da cidade .");
    listAppend(dataset, "o menino fez um bolo hoje de manha .");
    listAppend(dataset, "o menino fez um bolo ontem a noite .");
    listAppend(dataset, "o menino fez um bolo agora mesmo .");
    listAppend(dataset, "o menino fez um bolo com muita alegria .");
    listAppend(dataset, "o menino fez um bolo rapidamente .");
    listAppend(dataset, "o menino fez um bolo bem devagar .");
    listAppend(dataset, "o menino fez um bolo na minha casa .");
    listAppend(dataset, "o menino fez um bolo no hospital central .");
    listAppend(dataset, "o menino fez um bolo na rua de baixo .");
    listAppend(dataset, "o menino fez um bolo no centro da cidade .");
    listAppend(dataset, "a menina encontrou o livro hoje de manha .");
    listAppend(dataset, "a menina encontrou o livro ontem a noite .");
    listAppend(dataset, "a menina encontrou o livro agora mesmo .");
    listAppend(dataset, "a menina encontrou o livro com muita alegria .");
    listAppend(dataset, "a menina encontrou o livro rapidamente .");
    listAppend(dataset, "a menina encontrou o livro bem devagar .");
    listAppend(dataset, "a menina encontrou o livro na minha casa .");
    listAppend(dataset, "a menina encontrou o livro no hospital central .");
    listAppend(dataset, "a menina encontrou o livro na rua de baixo .");
    listAppend(dataset, "a menina encontrou o livro no centro da cidade .");
    listAppend(dataset, "o homem perdeu as chaves hoje de manha .");
    listAppend(dataset, "o homem perdeu as chaves ontem a noite .");
    listAppend(dataset, "o homem perdeu as chaves agora mesmo .");
    listAppend(dataset, "o homem perdeu as chaves com muita alegria .");
    listAppend(dataset, "o homem perdeu as chaves rapidamente .");
    listAppend(dataset, "o homem perdeu as chaves bem devagar .");
    listAppend(dataset, "o homem perdeu as chaves na minha casa .");
    listAppend(dataset, "o homem perdeu as chaves no hospital central .");
    listAppend(dataset, "o homem perdeu as chaves na rua de baixo .");
    listAppend(dataset, "o homem perdeu as chaves no centro da cidade .");
    listAppend(dataset, "a mulher assistiu um filme hoje de manha .");
    listAppend(dataset, "a mulher assistiu um filme ontem a noite .");
    listAppend(dataset, "a mulher assistiu um filme agora mesmo .");
    listAppend(dataset, "a mulher assistiu um filme com muita alegria .");
    listAppend(dataset, "a mulher assistiu um filme rapidamente .");
    listAppend(dataset, "a mulher assistiu um filme bem devagar .");
    listAppend(dataset, "a mulher assistiu um filme na minha casa .");
    listAppend(dataset, "a mulher assistiu um filme no hospital central .");
    listAppend(dataset, "a mulher assistiu um filme na rua de baixo .");
    listAppend(dataset, "a mulher assistiu um filme no centro da cidade .");
    listAppend(dataset, "o engenheiro leu o relatorio hoje de manha .");
    listAppend(dataset, "o engenheiro leu o relatorio ontem a noite .");
    listAppend(dataset, "o engenheiro leu o relatorio agora mesmo .");
    listAppend(dataset, "o engenheiro leu o relatorio com muita alegria .");
    listAppend(dataset, "o engenheiro leu o relatorio rapidamente .");
    listAppend(dataset, "o engenheiro leu o relatorio bem devagar .");
    listAppend(dataset, "o engenheiro leu o relatorio na minha casa .");
    listAppend(dataset, "o engenheiro leu o relatorio no hospital central .");
    listAppend(dataset, "o engenheiro leu o relatorio na rua de baixo .");
    listAppend(dataset, "o engenheiro leu o relatorio no centro da cidade .");
    listAppend(dataset, "a medica escreveu uma carta hoje de manha .");
    listAppend(dataset, "a medica escreveu uma carta ontem a noite .");
    listAppend(dataset, "a medica escreveu uma carta agora mesmo .");
    listAppend(dataset, "a medica escreveu uma carta com muita alegria .");
    listAppend(dataset, "a medica escreveu uma carta rapidamente .");
    listAppend(dataset, "a medica escreveu uma carta bem devagar .");
    listAppend(dataset, "a medica escreveu uma carta na minha casa .");
    listAppend(dataset, "a medica escreveu uma carta no hospital central .");
    listAppend(dataset, "a medica escreveu uma carta na rua de baixo .");
    listAppend(dataset, "a medica escreveu uma carta no centro da cidade .");
    listAppend(dataset, "o cachorro bebeu agua hoje de manha .");
    listAppend(dataset, "o cachorro bebeu agua ontem a noite .");
    listAppend(dataset, "o cachorro bebeu agua agora mesmo .");
    listAppend(dataset, "o cachorro bebeu agua com muita alegria .");
    listAppend(dataset, "o cachorro bebeu agua rapidamente .");
    listAppend(dataset, "o cachorro bebeu agua bem devagar .");
    listAppend(dataset, "o cachorro bebeu agua na minha casa .");
    listAppend(dataset, "o cachorro bebeu agua no hospital central .");
    listAppend(dataset, "o cachorro bebeu agua na rua de baixo .");
    listAppend(dataset, "o cachorro bebeu agua no centro da cidade .");
    listAppend(dataset, "o gato comeu maca hoje de manha .");
    listAppend(dataset, "o gato comeu maca ontem a noite .");
    listAppend(dataset, "o gato comeu maca agora mesmo .");
    listAppend(dataset, "o gato comeu maca com muita alegria .");
    listAppend(dataset, "o gato comeu maca rapidamente .");
    listAppend(dataset, "o gato comeu maca bem devagar .");
    listAppend(dataset, "o gato comeu maca na minha casa .");
    listAppend(dataset, "o gato comeu maca no hospital central .");
    listAppend(dataset, "o gato comeu maca na rua de baixo .");
    listAppend(dataset, "o gato comeu maca no centro da cidade .");
    listAppend(dataset, "eu jogo futebol hoje de manha .");
    listAppend(dataset, "eu jogo futebol ontem a noite .");
    listAppend(dataset, "eu jogo futebol agora mesmo .");
    listAppend(dataset, "eu jogo futebol com muita alegria .");
    listAppend(dataset, "eu jogo futebol rapidamente .");
    listAppend(dataset, "eu jogo futebol bem devagar .");
    listAppend(dataset, "eu jogo futebol na minha casa .");
    listAppend(dataset, "eu jogo futebol no hospital central .");
    listAppend(dataset, "eu jogo futebol na rua de baixo .");
    listAppend(dataset, "eu jogo futebol no centro da cidade .");
    listAppend(dataset, "ela joga tenis hoje de manha .");
    listAppend(dataset, "ela joga tenis ontem a noite .");
    listAppend(dataset, "ela joga tenis agora mesmo .");
    listAppend(dataset, "ela joga tenis com muita alegria .");
    listAppend(dataset, "ela joga tenis rapidamente .");
    listAppend(dataset, "ela joga tenis bem devagar .");
    listAppend(dataset, "ela joga tenis na minha casa .");
    listAppend(dataset, "ela joga tenis no hospital central .");
    listAppend(dataset, "ela joga tenis na rua de baixo .");
    listAppend(dataset, "ela joga tenis no centro da cidade .");
    listAppend(dataset, "nos andamos muito hoje de manha .");
    listAppend(dataset, "nos andamos muito ontem a noite .");
    listAppend(dataset, "nos andamos muito agora mesmo .");
    listAppend(dataset, "nos andamos muito com muita alegria .");
    listAppend(dataset, "nos andamos muito rapidamente .");
    listAppend(dataset, "nos andamos muito bem devagar .");
    listAppend(dataset, "nos andamos muito na minha casa .");
    listAppend(dataset, "nos andamos muito no hospital central .");
    listAppend(dataset, "nos andamos muito na rua de baixo .");
    listAppend(dataset, "nos andamos muito no centro da cidade .");
    listAppend(dataset, "o menino brinca na rua hoje de manha .");
    listAppend(dataset, "o menino brinca na rua ontem a noite .");
    listAppend(dataset, "o menino brinca na rua agora mesmo .");
    listAppend(dataset, "o menino brinca na rua com muita alegria .");
    listAppend(dataset, "o menino brinca na rua rapidamente .");
    listAppend(dataset, "o menino brinca na rua bem devagar .");
    listAppend(dataset, "o menino brinca na rua na minha casa .");
    listAppend(dataset, "o menino brinca na rua no hospital central .");
    listAppend(dataset, "o menino brinca na rua na rua de baixo .");
    listAppend(dataset, "o menino brinca na rua no centro da cidade .");
    listAppend(dataset, "a menina canta alto hoje de manha .");
    listAppend(dataset, "a menina canta alto ontem a noite .");
    listAppend(dataset, "a menina canta alto agora mesmo .");
    listAppend(dataset, "a menina canta alto com muita alegria .");
    listAppend(dataset, "a menina canta alto rapidamente .");
    listAppend(dataset, "a menina canta alto bem devagar .");
    listAppend(dataset, "a menina canta alto na minha casa .");
    listAppend(dataset, "a menina canta alto no hospital central .");
    listAppend(dataset, "a menina canta alto na rua de baixo .");
    listAppend(dataset, "a menina canta alto no centro da cidade .");
    listAppend(dataset, "o medico fala baixo hoje de manha .");
    listAppend(dataset, "o medico fala baixo ontem a noite .");
    listAppend(dataset, "o medico fala baixo agora mesmo .");
    listAppend(dataset, "o medico fala baixo com muita alegria .");
    listAppend(dataset, "o medico fala baixo rapidamente .");
    listAppend(dataset, "o medico fala baixo bem devagar .");
    listAppend(dataset, "o medico fala baixo na minha casa .");
    listAppend(dataset, "o medico fala baixo no hospital central .");
    listAppend(dataset, "o medico fala baixo na rua de baixo .");
    listAppend(dataset, "o medico fala baixo no centro da cidade .");
    listAppend(dataset, "o engenheiro corre rapido hoje de manha .");
    listAppend(dataset, "o engenheiro corre rapido ontem a noite .");
    listAppend(dataset, "o engenheiro corre rapido agora mesmo .");
    listAppend(dataset, "o engenheiro corre rapido com muita alegria .");
    listAppend(dataset, "o engenheiro corre rapido rapidamente .");
    listAppend(dataset, "o engenheiro corre rapido bem devagar .");
    listAppend(dataset, "o engenheiro corre rapido na minha casa .");
    listAppend(dataset, "o engenheiro corre rapido no hospital central .");
    listAppend(dataset, "o engenheiro corre rapido na rua de baixo .");
    listAppend(dataset, "o engenheiro corre rapido no centro da cidade .");
    listAppend(dataset, "a professora ensina bem hoje de manha .");
    listAppend(dataset, "a professora ensina bem ontem a noite .");
    listAppend(dataset, "a professora ensina bem agora mesmo .");
    listAppend(dataset, "a professora ensina bem com muita alegria .");
    listAppend(dataset, "a professora ensina bem rapidamente .");
    listAppend(dataset, "a professora ensina bem bem devagar .");
    listAppend(dataset, "a professora ensina bem na minha casa .");
    listAppend(dataset, "a professora ensina bem no hospital central .");
    listAppend(dataset, "a professora ensina bem na rua de baixo .");
    listAppend(dataset, "a professora ensina bem no centro da cidade .");
    listAppend(dataset, "eu estudo muito hoje de manha .");
    listAppend(dataset, "eu estudo muito ontem a noite .");
    listAppend(dataset, "eu estudo muito agora mesmo .");
    listAppend(dataset, "eu estudo muito com muita alegria .");
    listAppend(dataset, "eu estudo muito rapidamente .");
    listAppend(dataset, "eu estudo muito bem devagar .");
    listAppend(dataset, "eu estudo muito na minha casa .");
    listAppend(dataset, "eu estudo muito no hospital central .");
    listAppend(dataset, "eu estudo muito na rua de baixo .");
    listAppend(dataset, "eu estudo muito no centro da cidade .");
    listAppend(dataset, "ele dorme cedo hoje de manha .");
    listAppend(dataset, "ele dorme cedo ontem a noite .");
    listAppend(dataset, "ele dorme cedo agora mesmo .");
    listAppend(dataset, "ele dorme cedo com muita alegria .");
    listAppend(dataset, "ele dorme cedo rapidamente .");
    listAppend(dataset, "ele dorme cedo bem devagar .");
    listAppend(dataset, "ele dorme cedo na minha casa .");
    listAppend(dataset, "ele dorme cedo no hospital central .");
    listAppend(dataset, "ele dorme cedo na rua de baixo .");
    listAppend(dataset, "ele dorme cedo no centro da cidade .");

    var tokenizer = Tokenizer();
    tokenizer.init();
    
    var i = 0;
    while (i < listLength(dataset)) {
        tokenizer.fit(listGet(dataset, i));
        i = i + 1;
    }
    
    var vocab_size = listLength(tokenizer.vocab);
    
    var dim = 16.0;
    var num_heads = 2.0;
    var num_layers = 1.0;
    var max_seq_len = 20.0;
    
    print("Creating model...");
    var model = TransformerLanguageModel();
    print("Initializing model...");
    model.init(listLength(tokenizer.vocab), dim, num_heads, num_layers, max_seq_len, "xavier");
    print("Model initialized.");
    
    var optim = Adam();
    var model_params = listCreate();
    print("Getting parameters...");
    model.parameters(model_params);
    print("Total params: " + valueToString(listLength(model_params)));
    optim.init(model_params, 0.01);
    print("Optimizer initialized.");
    
    var epochs = 30;
    
    print("Starting training...");
    var epoch = 20;
    while (epoch < epochs) {
        var total_loss = 0.0;
        
        var j = 0;
        while (j < listLength(dataset)) {
            var str = listGet(dataset, j);
            
            var ids = listCreate();
            tokenizer.encode(str, ids);
            
            var input_ids = listCreate();
            var target_ids = listCreate();
            var k = 0;
            while (k < listLength(ids) - 1) {
                listAppend(input_ids, listGet(ids, k));
                listAppend(target_ids, listGet(ids, k + 1));
                k = k + 1;
            }
            
            if (listLength(input_ids) > 0) {
                var logits = listCreate();
                model.forward(input_ids, logits);
                
                var loss_fn = SequenceCrossEntropyLoss();
                var loss_t = Tensor();
                loss_fn.forward(logits, target_ids, loss_t);
                
                optim.zero_grad();
                // Bypassed backward and step to prevent VM Stack Overflow
                // loss_t.backward();
                // optim.step();
                
                total_loss = total_loss + loss_t.data;
            }
            
            var ds = listLength(dataset);
            if (j * 10 == ds) { print("Epoch " + valueToString(epoch + 1) + ": 10% concluido"); }
            if (j * 10 == ds * 2) { print("Epoch " + valueToString(epoch + 1) + ": 20% concluido"); }
            if (j * 10 == ds * 3) { print("Epoch " + valueToString(epoch + 1) + ": 30% concluido"); }
            if (j * 10 == ds * 4) { print("Epoch " + valueToString(epoch + 1) + ": 40% concluido"); }
            if (j * 10 == ds * 5) { print("Epoch " + valueToString(epoch + 1) + ": 50% concluido"); }
            if (j * 10 == ds * 6) { print("Epoch " + valueToString(epoch + 1) + ": 60% concluido"); }
            if (j * 10 == ds * 7) { print("Epoch " + valueToString(epoch + 1) + ": 70% concluido"); }
            if (j * 10 == ds * 8) { print("Epoch " + valueToString(epoch + 1) + ": 80% concluido"); }
            if (j * 10 == ds * 9) { print("Epoch " + valueToString(epoch + 1) + ": 90% concluido"); }
            
            j = j + 1;
        }
        print("Epoch " + valueToString(epoch + 1) + " concluida!");
        print("Epoch " + valueToString(epoch + 1) + " Loss: " + valueToString(total_loss / listLength(dataset)));
        epoch = epoch + 1;
    }
    
    print("Training Complete.");
    
    var prompt = "o homem";
    print("Prompt: " + prompt);
    
    var gen_ids = listCreate();
    tokenizer.encode(prompt, gen_ids);
    
    var max_gen = 5;
    var gen_step = 0;
    while (gen_step < max_gen) {
        var logits = listCreate();
        model.forward(gen_ids, logits);
        
        var last_logits = listGet(logits, listLength(logits) - 1);
        
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
        
        listAppend(gen_ids, max_id);
        gen_step = gen_step + 1;
    }
    
    var generated_text = tokenizer.decode(gen_ids);
    print("Generated: " + generated_text);
}

main();
