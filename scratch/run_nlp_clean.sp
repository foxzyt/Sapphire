







var Math_E = 2.718281828459045;

function util_list_sum(list l, Tensor out) void {
    var sum = 0.0;
    var i = 0;
    var len = listLength(l);
    while (i < len) {
        var t = listGet(l, i);
        sum = sum + t.data;
        i = i + 1;
    }


    var current = listGet(l, 0);
    i = 1;
    while (i < len) {
        var next_val = listGet(l, i);
        var temp = Tensor();
        current.add(next_val, temp);
        current = temp;
        i = i + 1;
    }
    out.init(current.data, current._prev, current._op);
    out._op_attr = current._op_attr;
}




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

class GPULinear {
    function init(double nin, double nout, string init_type) void {
        this.nin = nin;
        this.nout = nout;
        this.neurons = listCreate();
        var i = 0;
        while (i < nout) {
            var n = Neuron();
            n.init(nin, init_type);
            listAppend(this.neurons, n);
            i = i + 1;
        }
        
        this.gpu_active = false;
        var has_ocl = OpenCL.init();
        if (has_ocl == true) {
            var forward_src = "
            #define NIN " + valueToString(nin) + "
            #define NOUT " + valueToString(nout) + "
            __kernel void forward_kernel(__global const double *x, __global const double *w, __global const double *b, __global double *y) {
                int i = get_global_id(0);
                if (i < NOUT) {
                    double sum = b[i];
                    for (int j = 0; j < NIN; j = j + 1) {
                        sum = sum + (w[i * NIN + j] * x[j]);
                    }
                    y[i] = sum;
                }
            }
            ";
            
            var k_id = OpenCL.compile(forward_src, "forward_kernel");
            if (k_id != -1.0) {
                this.forward_kernel = k_id;
                this.x_buf = OpenCL.createBuffer(nin);
                this.w_buf = OpenCL.createBuffer(nout * nin);
                this.b_buf = OpenCL.createBuffer(nout);
                this.y_buf = OpenCL.createBuffer(nout);
                if (this.x_buf != -1.0) {
                    if (this.w_buf != -1.0) {
                        if (this.b_buf != -1.0) {
                            if (this.y_buf != -1.0) {
                                this.gpu_active = true;
                            }
                        }
                    }
                }
            }
        }
    }

    function forward(list x, list out) void {
        if (this.gpu_active == true) {
            var w_arr = listCreate();
            var i = 0;
            while (i < this.nout) {
                var n = listGet(this.neurons, i);
                var j = 0;
                while (j < this.nin) {
                    var w_tensor = listGet(n.w, j);
                    listAppend(w_arr, w_tensor.data);
                    j = j + 1;
                }
                i = i + 1;
            }
            OpenCL.writeBuffer(this.w_buf, w_arr);

            var b_arr = listCreate();
            i = 0;
            while (i < this.nout) {
                var n = listGet(this.neurons, i);
                listAppend(b_arr, n.b.data);
                i = i + 1;
            }
            OpenCL.writeBuffer(this.b_buf, b_arr);

            var x_arr = listCreate();
            i = 0;
            while (i < this.nin) {
                var t = listGet(x, i);
                listAppend(x_arr, t.data);
                i = i + 1;
            }
            OpenCL.writeBuffer(this.x_buf, x_arr);

            OpenCL.execute(this.forward_kernel, this.nout, this.x_buf, this.w_buf, this.b_buf, this.y_buf);

            var y_arr = listCreate();
            i = 0;
            while (i < this.nout) {
                listAppend(y_arr, 0.0);
                i = i + 1;
            }
            OpenCL.readBuffer(this.y_buf, y_arr);

            i = 0;
            while (i < this.nout) {
                var n = listGet(this.neurons, i);
                var children = listCreate();
                listAppend(children, n.b);
                
                var j = 0;
                while (j < this.nin) {
                    listAppend(children, listGet(x, j));
                    listAppend(children, listGet(n.w, j));
                    j = j + 1;
                }
                
                var out_t = Tensor();
                out_t.init(listGet(y_arr, i), children, "gpu_linear_neuron");
                listAppend(out, out_t);
                i = i + 1;
            }
        } else {
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


class Dropout {
    function init(double p) void {
        this.p = p;
        this.is_training = true;
    }
    
    function forward(list x, list out) void {
        var i = 0;
        var len = listLength(x);
        while (i < len) {
            var xi = listGet(x, i);
            var out_tensor = Tensor();
            
            if (this.is_training) {
                if (rand() < this.p) {
                    out_tensor.init(0.0, listCreate(), "");
                } else {
                    var scale = Tensor();
                    scale.init(1.0 / (1.0 - this.p), listCreate(), "");
                    xi.mul(scale, out_tensor);
                }
            } else {
                out_tensor.init(xi.data, xi._prev, xi._op);
            }
            
            listAppend(out, out_tensor);
            i = i + 1;
        }
    }
    
    function parameters(list params_out) void { }
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





class LossAPI {
    function MSELoss(list preds, list targets, Tensor out) void {
        var total_loss = Tensor();
        total_loss.init(0.0, listCreate(), "");
        
        var i = 0;
        var len = listLength(preds);
        while (i < len) {
            var p = listGet(preds, i);
            var t = listGet(targets, i);
            var diff = Tensor();
            p.sub(t, diff);
            var sq = Tensor();
            diff.pow_t(2.0, sq);
            var next_tot = Tensor();
            total_loss.add(sq, next_tot);
            total_loss = next_tot;
            i = i + 1;
        }
        var div_t = Tensor();
        div_t.init(1.0 * len, listCreate(), "");
        total_loss.div(div_t, out);
    }
    
    function L1Loss(list preds, list targets, Tensor out) void {
        var total_loss = Tensor();
        total_loss.init(0.0, listCreate(), "");
        
        var i = 0;
        var len = listLength(preds);
        while (i < len) {
            var p = listGet(preds, i);
            var t = listGet(targets, i);
            var diff = Tensor();
            p.sub(t, diff);

            var rel1 = Tensor(); diff.relu(rel1);
            var neg_one = Tensor(); neg_one.init(-1.0, listCreate(), "");
            var neg_diff = Tensor(); diff.mul(neg_one, neg_diff);
            var rel2 = Tensor(); neg_diff.relu(rel2);
            var abs_val = Tensor(); rel1.add(rel2, abs_val);
            
            var next_tot = Tensor();
            total_loss.add(abs_val, next_tot);
            total_loss = next_tot;
            i = i + 1;
        }
        var div_t = Tensor();
        div_t.init(1.0 * len, listCreate(), "");
        total_loss.div(div_t, out);
    }
}




class SGD {
    function init(list parameters, double lr, double momentum) void {
        this.parameters = parameters;
        this.lr = lr;
        this.momentum = momentum;
        this.velocities = listCreate();
        var i = 0;
        while (i < listLength(parameters)) {
            listAppend(this.velocities, 0.0);
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
        var i = 0;
        var len = listLength(this.parameters);
        while (i < len) {
            var p = listGet(this.parameters, i);
            var v = listGet(this.velocities, i);
            
            v = (this.momentum * v) + p.grad;
            listSet(this.velocities, i, v);
            
            p.data = p.data - (this.lr * v);
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

class RMSprop {
    function init(list parameters, double lr, double alpha) void {
        this.parameters = parameters;
        this.lr = lr;
        this.alpha = alpha;
        this.eps = 0.00000001;
        
        this.v = listCreate();
        var i = 0;
        while (i < listLength(parameters)) {
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
        var i = 0;
        var len = listLength(this.parameters);
        while (i < len) {
            var p = listGet(this.parameters, i);
            var v_i = listGet(this.v, i);
            
            v_i = (this.alpha * v_i) + ((1.0 - this.alpha) * (p.grad * p.grad));
            listSet(this.v, i, v_i);
            
            p.data = p.data - (this.lr * p.grad / (sqrt(v_i) + this.eps));
            i = i + 1;
        }
    }
}




class StepLR {
    function init(Adam opt, double step_size, double gamma) void {
        this.opt = opt;
        this.step_size = step_size;
        this.gamma = gamma;
        this.epoch = 0;
    }
    
    function step() void {
        this.epoch = this.epoch + 1;

        var ratio = (1.0 * this.epoch) / this.step_size;
        if (ratio == floor(ratio)) {
            this.opt.lr = this.opt.lr * this.gamma;
        }
    }
}

class ExponentialLR {
    function init(Adam opt, double gamma) void {
        this.opt = opt;
        this.gamma = gamma;
    }
    
    function step() void {
        this.opt.lr = this.opt.lr * this.gamma;
    }
}




class DataLoader {
    function init(list xs, list ys, double batch_size, bool shuffle) void {
        this.xs = xs;
        this.ys = ys;
        this.batch_size = batch_size;
        this.shuffle = shuffle;
    }
    
    function get_batches(list batches_out) void {
        var len = listLength(this.xs);
        
        var indices = listCreate();
        var i = 0;
        while (i < len) {
            listAppend(indices, i);
            i = i + 1;
        }
        
        if (this.shuffle) {

            i = len - 1;
            while (i > 0) {
                var j = floor(rand() * (i + 1));
                var temp = listGet(indices, i);
                listSet(indices, i, listGet(indices, j));
                listSet(indices, j, temp);
                i = i - 1;
            }
        }
        
        i = 0;
        while (i < len) {
            var b_x = listCreate();
            var b_y = listCreate();
            
            var b_idx = 0;
            while (b_idx < this.batch_size) {
                if (i + b_idx < len) {
                    var actual_idx = listGet(indices, i + b_idx);
                    listAppend(b_x, listGet(this.xs, actual_idx));
                    listAppend(b_y, listGet(this.ys, actual_idx));
                }
                b_idx = b_idx + 1;
            }
            
            var batch_pair = listCreate();
            listAppend(batch_pair, b_x);
            listAppend(batch_pair, b_y);
            listAppend(batches_out, batch_pair);
            
            i = i + this.batch_size;
        }
    }
}




class MetricsLogger {
    function init() void {
        this.history_loss = listCreate();
        this.history_acc = listCreate();
    }
    
    function log(double epoch, double loss, double acc) void {
        listAppend(this.history_loss, loss);
        listAppend(this.history_acc, acc);
        printColor("cyan", "[Epoch " + epoch + "] Loss: " + loss + " | Accuracy: " + acc);
    }
    
    function calculate_accuracy(list preds, list targets, double threshold) double {
        var correct = 0.0;
        var i = 0;
        var len = listLength(preds);
        while (i < len) {
            var p = listGet(preds, i);
            var t = listGet(targets, i);
            var pred_class = 0.0;
            if (p.data >= threshold) { pred_class = 1.0; }
            if (pred_class == t.data) { correct = correct + 1.0; }
            i = i + 1;
        }
        return correct / len;
    }
}




class Trainer {
    function init(MLP model, Adam optimizer, LossAPI loss_fn, DataLoader loader, double epochs) void {
        this.model = model;
        this.optimizer = optimizer;
        this.loss_fn = loss_fn;
        this.loader = loader;
        this.epochs = epochs;
        this.logger = MetricsLogger();
        this.logger.init();
    }
    
    function fit() void {
        printColor("yellow", "========== Iniciando Treinamento ==========");
        var e = 0;
        while (e < this.epochs) {
            var batches = listCreate();
            this.loader.get_batches(batches);
            var b = 0;
            
            var epoch_loss_sum = 0.0;
            var total_samples = 0.0;
            
            while (b < listLength(batches)) {
                var batch_pair = listGet(batches, b);
                var b_x = listGet(batch_pair, 0);
                var b_y = listGet(batch_pair, 1);
                
                var iter_loss = Tensor();
                iter_loss.init(0.0, listCreate(), "");
                
                var s = 0;
                var b_size = listLength(b_x);
                var all_preds = listCreate();
                
                while (s < b_size) {
                    var x_in = listGet(b_x, s);
                    var y_targ = listGet(b_y, s);
                    
                    var out_list = listCreate();
                    this.model.forward(x_in, out_list);
                    
                    var target_t = Tensor();
                    target_t.init(y_targ, listCreate(), "");
                    
                    var p = listGet(out_list, 0);
                    listAppend(all_preds, p);
                    
                    var diff = Tensor(); p.sub(target_t, diff);
                    var sq = Tensor(); diff.pow_t(2.0, sq);
                    var next_loss = Tensor(); iter_loss.add(sq, next_loss);
                    iter_loss = next_loss;
                    
                    s = s + 1;
                }
                
                var div_t = Tensor(); div_t.init(1.0 * b_size, listCreate(), "");
                var final_loss = Tensor();
                iter_loss.div(div_t, final_loss);
                
                this.optimizer.zero_grad();
                final_loss.backward();
                this.optimizer.step();
                
                epoch_loss_sum = epoch_loss_sum + final_loss.data;
                total_samples = total_samples + 1.0;
                b = b + 1;
            }
            
            var avg_loss = epoch_loss_sum / total_samples;
            this.logger.log(e, avg_loss, 0.0);
            e = e + 1;
        }
        printColor("green", "========== Treinamento Finalizado ==========");
    }
}






class RNNCell {
    function init(double nin, double hidden_size, string init_type) void {
        this.nin = nin;
        this.hidden_size = hidden_size;
        
        this.w_ih = Linear();
        this.w_ih.init(nin, hidden_size, init_type);
        
        this.w_hh = Linear();
        this.w_hh.init(hidden_size, hidden_size, init_type);
    }

    function forward(list x, list h_prev, list out_h) void {
        var ih_out = listCreate();
        this.w_ih.forward(x, ih_out);
        
        var hh_out = listCreate();
        this.w_hh.forward(h_prev, hh_out);
        
        var i = 0;
        var len = listLength(ih_out);
        while (i < len) {
            var val_ih = listGet(ih_out, i);
            var val_hh = listGet(hh_out, i);
            
            var sum_val = Tensor();
            val_ih.add(val_hh, sum_val);
            
            var h_t_i = Tensor();
            sum_val.tanh_t(h_t_i);
            
            listAppend(out_h, h_t_i);
            i = i + 1;
        }
    }

    function parameters(list params_out) void {
        this.w_ih.parameters(params_out);
        this.w_hh.parameters(params_out);
    }
}

class LSTMCell {
    function init(double nin, double hidden_size, string init_type) void {
        this.nin = nin;
        this.hidden_size = hidden_size;
        
        var combined_size = nin + hidden_size;
        
        this.w_f = Linear();
        this.w_f.init(combined_size, hidden_size, init_type);
        
        this.w_i = Linear();
        this.w_i.init(combined_size, hidden_size, init_type);
        
        this.w_c = Linear();
        this.w_c.init(combined_size, hidden_size, init_type);
        
        this.w_o = Linear();
        this.w_o.init(combined_size, hidden_size, init_type);
    }

    function forward(list x, list h_prev, list c_prev, list out_h, list out_c) void {
        var combined = listCreate();
        var i = 0;
        var len_h = listLength(h_prev);
        while (i < len_h) {
            listAppend(combined, listGet(h_prev, i));
            i = i + 1;
        }
        i = 0;
        var len_x = listLength(x);
        while (i < len_x) {
            listAppend(combined, listGet(x, i));
            i = i + 1;
        }
        
        var f_raw = listCreate(); this.w_f.forward(combined, f_raw);
        var i_raw = listCreate(); this.w_i.forward(combined, i_raw);
        var c_raw = listCreate(); this.w_c.forward(combined, c_raw);
        var o_raw = listCreate(); this.w_o.forward(combined, o_raw);
        
        i = 0;
        while (i < this.hidden_size) {
            var f_t_raw = listGet(f_raw, i);
            var f_t = Tensor();
            f_t_raw.sigmoid(f_t);
            
            var i_t_raw = listGet(i_raw, i);
            var i_t = Tensor();
            i_t_raw.sigmoid(i_t);
            
            var c_tilde_raw = listGet(c_raw, i);
            var c_tilde_t = Tensor();
            c_tilde_raw.tanh_t(c_tilde_t);
            
            var o_t_raw = listGet(o_raw, i);
            var o_t = Tensor();
            o_t_raw.sigmoid(o_t);
            
            var prev_c_val = listGet(c_prev, i);
            
            var forget_term = Tensor();
            f_t.mul(prev_c_val, forget_term);
            
            var input_term = Tensor();
            i_t.mul(c_tilde_t, input_term);
            
            var c_t = Tensor();
            forget_term.add(input_term, c_t);
            listAppend(out_c, c_t);
            
            var c_t_tanh = Tensor();
            c_t.tanh_t(c_t_tanh);
            
            var h_t = Tensor();
            o_t.mul(c_t_tanh, h_t);
            listAppend(out_h, h_t);
            
            i = i + 1;
        }
    }

    function parameters(list params_out) void {
        this.w_f.parameters(params_out);
        this.w_i.parameters(params_out);
        this.w_c.parameters(params_out);
        this.w_o.parameters(params_out);
    }
}

class GRUCell {
    function init(double nin, double hidden_size, string init_type) void {
        this.nin = nin;
        this.hidden_size = hidden_size;
        
        var combined_size = nin + hidden_size;
        
        this.w_z = Linear(); this.w_z.init(combined_size, hidden_size, init_type);
        this.w_r = Linear(); this.w_r.init(combined_size, hidden_size, init_type);
        this.w_h = Linear(); this.w_h.init(combined_size, hidden_size, init_type);
    }

    function forward(list x, list h_prev, list out_h) void {
        var combined = listCreate();
        var i = 0;
        var len_h = listLength(h_prev);
        while (i < len_h) { listAppend(combined, listGet(h_prev, i)); i = i + 1; }
        i = 0;
        var len_x = listLength(x);
        while (i < len_x) { listAppend(combined, listGet(x, i)); i = i + 1; }
        
        var z_raw = listCreate(); this.w_z.forward(combined, z_raw);
        var r_raw = listCreate(); this.w_r.forward(combined, r_raw);
        
        var combined_r = listCreate();
        i = 0;
        while (i < this.hidden_size) {
            var r_t = Tensor();
            var r_raw_t = listGet(r_raw, i);
            r_raw_t.sigmoid(r_t);
            
            var h_prev_t = listGet(h_prev, i);
            var r_h_prev = Tensor();
            r_t.mul(h_prev_t, r_h_prev);
            
            listAppend(combined_r, r_h_prev);
            i = i + 1;
        }
        
        i = 0;
        while (i < len_x) { listAppend(combined_r, listGet(x, i)); i = i + 1; }
        
        var h_tilde_raw = listCreate(); this.w_h.forward(combined_r, h_tilde_raw);
        
        var one_t = Tensor(); one_t.init(1.0, listCreate(), "");
        
        i = 0;
        while (i < this.hidden_size) {
            var z_t = Tensor();
            var z_raw_t = listGet(z_raw, i);
            z_raw_t.sigmoid(z_t);
            
            var h_tilde_t = Tensor();
            var h_tilde_raw_t = listGet(h_tilde_raw, i);
            h_tilde_raw_t.tanh_t(h_tilde_t);
            
            var neg_z_t = Tensor();
            var neg_one = Tensor(); neg_one.init(-1.0, listCreate(), "");
            z_t.mul(neg_one, neg_z_t);
            
            var one_minus_z = Tensor();
            one_t.add(neg_z_t, one_minus_z);
            
            var term1 = Tensor();
            one_minus_z.mul(listGet(h_prev, i), term1);
            
            var term2 = Tensor();
            z_t.mul(h_tilde_t, term2);
            
            var h_t = Tensor();
            term1.add(term2, h_t);
            listAppend(out_h, h_t);
            
            i = i + 1;
        }
    }

    function parameters(list params_out) void {
        this.w_z.parameters(params_out);
        this.w_r.parameters(params_out);
        this.w_h.parameters(params_out);
    }
}

class StackedRNN {
    function init(double nin, double hidden_size, double num_layers, string cell_type, string init_type) void {
        this.num_layers = num_layers;
        this.cell_type = cell_type;
        this.cells = listCreate();
        
        var i = 0;
        while (i < num_layers) {
            var current_nin = nin;
            if (i > 0) { current_nin = hidden_size; }
            
            if (cell_type == "lstm") {
                var cell = LSTMCell();
                cell.init(current_nin, hidden_size, init_type);
                listAppend(this.cells, cell);
            } else if (cell_type == "gru") {
                var cell = GRUCell();
                cell.init(current_nin, hidden_size, init_type);
                listAppend(this.cells, cell);
            } else {
                var cell = RNNCell();
                cell.init(current_nin, hidden_size, init_type);
                listAppend(this.cells, cell);
            }
            i = i + 1;
        }
    }


    function forward_sequence(list sequence, list init_h_states, list init_c_states, list out_sequence) void {
        var t = 0;
        var seq_len = listLength(sequence);
        var current_h_states = listCreate();
        var current_c_states = listCreate();
        
        var l = 0;
        while (l < this.num_layers) {
            listAppend(current_h_states, listGet(init_h_states, l));
            if (this.cell_type == "lstm") {
                listAppend(current_c_states, listGet(init_c_states, l));
            }
            l = l + 1;
        }
        
        while (t < seq_len) {
            var xt = listGet(sequence, t);
            var layer_input = xt;
            
            l = 0;
            while (l < this.num_layers) {
                var cell = listGet(this.cells, l);
                var h_prev = listGet(current_h_states, l);
                
                var out_h = listCreate();
                if (this.cell_type == "lstm") {
                    var c_prev = listGet(current_c_states, l);
                    var out_c = listCreate();
                    cell.forward(layer_input, h_prev, c_prev, out_h, out_c);
                    listSet(current_c_states, l, out_c);
                } else if (this.cell_type == "gru") {
                    cell.forward(layer_input, h_prev, out_h);
                } else {
                    cell.forward(layer_input, h_prev, out_h);
                }
                listSet(current_h_states, l, out_h);
                layer_input = out_h;
                
                l = l + 1;
            }
            listAppend(out_sequence, layer_input);
            t = t + 1;
        }
    }
    
    function parameters(list params_out) void {
        var l = 0;
        while (l < this.num_layers) {
            var cell = listGet(this.cells, l);
            cell.parameters(params_out);
            l = l + 1;
        }
    }
}






class Conv1D {
    function init(double in_channels, double out_channels, double kernel_size, double stride, string init_type) void {
        this.in_channels = in_channels;
        this.out_channels = out_channels;
        this.kernel_size = kernel_size;
        this.stride = stride;
        
        this.weights = listCreate();
        var initializer = Init();
        
        var i = 0;
        var total_w = out_channels * in_channels * kernel_size;
        while (i < total_w) {
            var w_tensor = Tensor();
            var w_val = 0.0;
            if (init_type == "xavier") {
                w_val = initializer.xavier_uniform(in_channels * kernel_size, out_channels);
            } else {
                w_val = initializer.kaiming_uniform(in_channels * kernel_size);
            }
            w_tensor.init(w_val, listCreate(), "");
            listAppend(this.weights, w_tensor);
            i = i + 1;
        }
        
        this.biases = listCreate();
        i = 0;
        while (i < out_channels) {
            var b = Tensor();
            b.init(0.0, listCreate(), "");
            listAppend(this.biases, b);
            i = i + 1;
        }
    }
    
    function forward(list x, double width, list out) void {
        var out_width = floor(((width - this.kernel_size) / this.stride) + 1.0);
        
        var oc = 0;
        while (oc < this.out_channels) {
            var b_c = listGet(this.biases, oc);
            var ow = 0;
            while (ow < out_width) {
                var sum_act = Tensor();
                sum_act.init(b_c.data, b_c._prev, b_c._op);
                
                var ic = 0;
                while (ic < this.in_channels) {
                    var kw = 0;
                    while (kw < this.kernel_size) {
                        var in_idx = (ic * width) + (ow * this.stride) + kw;
                        var w_idx = (oc * this.in_channels * this.kernel_size) + (ic * this.kernel_size) + kw;
                        
                        var w_t = listGet(this.weights, w_idx);
                        var x_t = listGet(x, in_idx);
                        
                        var prod = Tensor();
                        w_t.mul(x_t, prod);
                        
                        var next_sum = Tensor();
                        sum_act.add(prod, next_sum);
                        sum_act = next_sum;
                        
                        kw = kw + 1;
                    }
                    ic = ic + 1;
                }
                listAppend(out, sum_act);
                ow = ow + 1;
            }
            oc = oc + 1;
        }
    }
    
    function parameters(list params_out) void {
        var i = 0;
        var len = listLength(this.weights);
        while (i < len) {
            listAppend(params_out, listGet(this.weights, i));
            i = i + 1;
        }
        i = 0;
        len = listLength(this.biases);
        while (i < len) {
            listAppend(params_out, listGet(this.biases, i));
            i = i + 1;
        }
    }
}

class MaxPool1D {
    function init(double kernel_size, double stride) void {
        this.kernel_size = kernel_size;
        this.stride = stride;
    }
    
    function forward(list x, double channels, double width, list out) void {
        var out_width = floor(((width - this.kernel_size) / this.stride) + 1.0);
        
        var c = 0;
        while (c < channels) {
            var ow = 0;
            while (ow < out_width) {
                var max_val = -999999.0;
                var max_t = listGet(x, 0); 
                
                var kw = 0;
                while (kw < this.kernel_size) {
                    var in_idx = (c * width) + (ow * this.stride) + kw;
                    var curr_t = listGet(x, in_idx);
                    if (curr_t.data > max_val) {
                        max_val = curr_t.data;
                        max_t = curr_t;
                    }
                    kw = kw + 1;
                }
                
                var clone_t = Tensor();
                var clone_children = listCreate();
                listAppend(clone_children, max_t);

                clone_t.init(max_t.data, clone_children, "relu");

                listAppend(out, max_t);
                
                ow = ow + 1;
            }
            c = c + 1;
        }
    }
    
    function parameters(list params_out) void { }
}

class FlattenLayer {
    function init() void { }
    
    function forward(list x, list out) void {
        var i = 0;
        var len = listLength(x);
        while (i < len) {
            listAppend(out, listGet(x, i));
            i = i + 1;
        }
    }
    
    function parameters(list params_out) void { }
}

class Conv2D {
    function init(double in_channels, double out_channels, double kernel_size, double stride, double padding, string init_type) void {
        this.in_channels = in_channels;
        this.out_channels = out_channels;
        this.kernel_size = kernel_size;
        this.stride = stride;
        this.padding = padding;
        
        this.weights = listCreate();
        var initializer = Init();
        
        var i = 0;
        var total_w = out_channels * in_channels * kernel_size * kernel_size;
        while (i < total_w) {
            var w_tensor = Tensor();
            var w_val = initializer.kaiming_uniform(in_channels * kernel_size * kernel_size);
            w_tensor.init(w_val, listCreate(), "");
            listAppend(this.weights, w_tensor);
            i = i + 1;
        }
        
        this.biases = listCreate();
        i = 0;
        while (i < out_channels) {
            var b = Tensor();
            b.init(0.0, listCreate(), "");
            listAppend(this.biases, b);
            i = i + 1;
        }
    }
    
    function forward(list x, double height, double width, list out) void {
        var padded_h = height + (2.0 * this.padding);
        var padded_w = width + (2.0 * this.padding);
        
        var out_h = floor(((padded_h - this.kernel_size) / this.stride) + 1.0);
        var out_w = floor(((padded_w - this.kernel_size) / this.stride) + 1.0);
        
        var oc = 0;
        while (oc < this.out_channels) {
            var b_c = listGet(this.biases, oc);
            var oh = 0;
            while (oh < out_h) {
                var ow = 0;
                while (ow < out_w) {
                    var sum_act = Tensor();
                    sum_act.init(b_c.data, b_c._prev, b_c._op);
                    
                    var ic = 0;
                    while (ic < this.in_channels) {
                        var kh = 0;
                        while (kh < this.kernel_size) {
                            var kw = 0;
                            while (kw < this.kernel_size) {
                                
                                var h_in = (oh * this.stride) + kh - this.padding;
                                var w_in = (ow * this.stride) + kw - this.padding;
                                
                                var w_idx = (oc * this.in_channels * this.kernel_size * this.kernel_size) + (ic * this.kernel_size * this.kernel_size) + (kh * this.kernel_size) + kw;
                                var w_t = listGet(this.weights, w_idx);
                                
                                if (h_in >= 0.0) {
                                    if (h_in < height) {
                                        if (w_in >= 0.0) {
                                            if (w_in < width) {
                                                var in_idx = (ic * height * width) + (h_in * width) + w_in;
                                                var x_t = listGet(x, in_idx);
                                                
                                                var prod = Tensor();
                                                w_t.mul(x_t, prod);
                                                
                                                var next_sum = Tensor();
                                                sum_act.add(prod, next_sum);
                                                sum_act = next_sum;
                                            }
                                        }
                                    }
                                }
                                kw = kw + 1;
                            }
                            kh = kh + 1;
                        }
                        ic = ic + 1;
                    }
                    listAppend(out, sum_act);
                    ow = ow + 1;
                }
                oh = oh + 1;
            }
            oc = oc + 1;
        }
    }
    
    function parameters(list params_out) void {
        var i = 0;
        var len = listLength(this.weights);
        while (i < len) { listAppend(params_out, listGet(this.weights, i)); i = i + 1; }
        i = 0;
        len = listLength(this.biases);
        while (i < len) { listAppend(params_out, listGet(this.biases, i)); i = i + 1; }
    }
}

class MaxPool2D {
    function init(double kernel_size, double stride) void {
        this.kernel_size = kernel_size;
        this.stride = stride;
    }
    
    function forward(list x, double channels, double height, double width, list out) void {
        var out_h = floor(((height - this.kernel_size) / this.stride) + 1.0);
        var out_w = floor(((width - this.kernel_size) / this.stride) + 1.0);
        
        var c = 0;
        while (c < channels) {
            var oh = 0;
            while (oh < out_h) {
                var ow = 0;
                while (ow < out_w) {
                    var max_val = -999999.0;
                    var max_t = listGet(x, 0); 
                    
                    var kh = 0;
                    while (kh < this.kernel_size) {
                        var kw = 0;
                        while (kw < this.kernel_size) {
                            var h_in = (oh * this.stride) + kh;
                            var w_in = (ow * this.stride) + kw;
                            var in_idx = (c * height * width) + (h_in * width) + w_in;
                            
                            var curr_t = listGet(x, in_idx);
                            if (curr_t.data > max_val) {
                                max_val = curr_t.data;
                                max_t = curr_t;
                            }
                            kw = kw + 1;
                        }
                        kh = kh + 1;
                    }
                    listAppend(out, max_t);
                    ow = ow + 1;
                }
                oh = oh + 1;
            }
            c = c + 1;
        }
    }
    
    function parameters(list params_out) void { }
}






class LayerNorm {
    function init(double features_size, double eps, string init_type) void {
        this.features_size = features_size;
        this.eps = eps;
        

        this.gamma = listCreate();
        this.beta = listCreate();
        
        var i = 0;
        while (i < features_size) {
            var g = Tensor(); g.init(1.0, listCreate(), "");
            var b = Tensor(); b.init(0.0, listCreate(), "");
            listAppend(this.gamma, g);
            listAppend(this.beta, b);
            i = i + 1;
        }
    }

    function forward(list x, list out) void {
        var i = 0;
        var len = listLength(x);
        
        var sum_t = Tensor(); sum_t.init(0.0, listCreate(), "");
        
        while (i < len) {
            var val_t = listGet(x, i);
            var next_sum = Tensor();
            sum_t.add(val_t, next_sum);
            sum_t = next_sum;
            i = i + 1;
        }
        
        var len_t = Tensor(); len_t.init(1.0 * len, listCreate(), "");
        var mean_t = Tensor();
        sum_t.div(len_t, mean_t);
        
        var var_sum_t = Tensor(); var_sum_t.init(0.0, listCreate(), "");
        i = 0;
        while (i < len) {
            var val_t = listGet(x, i);
            var diff_t = Tensor();
            val_t.sub(mean_t, diff_t);
            var sq_t = Tensor();
            diff_t.pow_t(2.0, sq_t);
            var next_vsum = Tensor();
            var_sum_t.add(sq_t, next_vsum);
            var_sum_t = next_vsum;
            i = i + 1;
        }
        
        var variance_t = Tensor();
        var_sum_t.div(len_t, variance_t);
        
        var eps_t = Tensor(); eps_t.init(this.eps, listCreate(), "");
        var var_plus_eps = Tensor();
        variance_t.add(eps_t, var_plus_eps);
        var std_t = Tensor();
        var_plus_eps.pow_t(0.5, std_t);
        
        i = 0;
        while (i < len) {
            var val_t = listGet(x, i);
            var diff_t = Tensor();
            val_t.sub(mean_t, diff_t);
            
            var norm_val = Tensor();
            diff_t.div(std_t, norm_val);
            
            var g_t = listGet(this.gamma, i);
            var b_t = listGet(this.beta, i);
            
            var scaled = Tensor();
            norm_val.mul(g_t, scaled);
            
            var shifted = Tensor();
            scaled.add(b_t, shifted);
            
            listAppend(out, shifted);
            i = i + 1;
        }
    }
    
    function parameters(list params_out) void {
        var i = 0;
        var len = listLength(this.gamma);
        while (i < len) {
            listAppend(params_out, listGet(this.gamma, i));
            listAppend(params_out, listGet(this.beta, i));
            i = i + 1;
        }
    }
}

class BatchNorm1D {
    function init(double features_size, double eps, double momentum) void {
        this.features_size = features_size;
        this.eps = eps;
        this.momentum = momentum;
        this.is_training = true;
        
        this.gamma = listCreate();
        this.beta = listCreate();
        this.running_mean = listCreate();
        this.running_var = listCreate();
        
        var i = 0;
        while (i < features_size) {
            var g = Tensor(); g.init(1.0, listCreate(), "");
            var b = Tensor(); b.init(0.0, listCreate(), "");
            listAppend(this.gamma, g);
            listAppend(this.beta, b);
            listAppend(this.running_mean, 0.0);
            listAppend(this.running_var, 1.0);
            i = i + 1;
        }
    }
    


    function forward_batch(list x_batch, list out_batch) void {
        var batch_size = listLength(x_batch);
        if (batch_size == 0) { return; }
        
        var i = 0;
        while (i < batch_size) {
            listAppend(out_batch, listCreate());
            i = i + 1;
        }
        
        var f = 0;
        while (f < this.features_size) {
            if (this.is_training) {
                var sum_val = 0.0;
                var b = 0;
                while (b < batch_size) {
                    var sample = listGet(x_batch, b);
                    var t = listGet(sample, f);
                    sum_val = sum_val + t.data;
                    b = b + 1;
                }
                var mean = sum_val / batch_size;
                
                var sq_sum = 0.0;
                b = 0;
                while (b < batch_size) {
                    var sample = listGet(x_batch, b);
                    var t = listGet(sample, f);
                    var diff = t.data - mean;
                    sq_sum = sq_sum + (diff * diff);
                    b = b + 1;
                }
                var variance = sq_sum / batch_size;
                var std_dev = sqrt(variance + this.eps);
                
                var old_rm = listGet(this.running_mean, f);
                listSet(this.running_mean, f, (this.momentum * old_rm) + ((1.0 - this.momentum) * mean));
                
                var old_rv = listGet(this.running_var, f);
                listSet(this.running_var, f, (this.momentum * old_rv) + ((1.0 - this.momentum) * variance));
                
                var std_t = Tensor(); std_t.init(std_dev, listCreate(), "");
                var mean_t = Tensor(); mean_t.init(mean, listCreate(), "");
                
                b = 0;
                while (b < batch_size) {
                    var sample = listGet(x_batch, b);
                    var t = listGet(sample, f);
                    
                    var diff_t = Tensor();
                    t.sub(mean_t, diff_t);
                    
                    var norm_val = Tensor();
                    diff_t.div(std_t, norm_val);
                    
                    var g_t = listGet(this.gamma, f);
                    var b_t = listGet(this.beta, f);
                    
                    var scaled = Tensor(); norm_val.mul(g_t, scaled);
                    var shifted = Tensor(); scaled.add(b_t, shifted);
                    
                    var out_sample = listGet(out_batch, b);
                    listAppend(out_sample, shifted);
                    b = b + 1;
                }
            } else {
                var mean = listGet(this.running_mean, f);
                var variance = listGet(this.running_var, f);
                var std_dev = sqrt(variance + this.eps);
                
                var std_t = Tensor(); std_t.init(std_dev, listCreate(), "");
                var mean_t = Tensor(); mean_t.init(mean, listCreate(), "");
                
                var b = 0;
                while (b < batch_size) {
                    var sample = listGet(x_batch, b);
                    var t = listGet(sample, f);
                    
                    var diff_t = Tensor(); t.sub(mean_t, diff_t);
                    var norm_val = Tensor(); diff_t.div(std_t, norm_val);
                    
                    var g_t = listGet(this.gamma, f);
                    var b_t = listGet(this.beta, f);
                    var scaled = Tensor(); norm_val.mul(g_t, scaled);
                    var shifted = Tensor(); scaled.add(b_t, shifted);
                    
                    var out_sample = listGet(out_batch, b);
                    listAppend(out_sample, shifted);
                    b = b + 1;
                }
            }
            f = f + 1;
        }
    }
    
    function parameters(list params_out) void {
        var i = 0;
        while (i < listLength(this.gamma)) {
            listAppend(params_out, listGet(this.gamma, i));
            listAppend(params_out, listGet(this.beta, i));
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

class MultiHeadAttention {
    function init(double embed_size, double num_heads, string init_type) void {
        this.embed_size = embed_size;
        this.num_heads = num_heads;
        this.head_dim = embed_size / num_heads;
        
        this.heads = listCreate();
        var i = 0;
        while (i < num_heads) {
            var head = SelfAttention();
            head.init(embed_size, init_type);
            listAppend(this.heads, head);
            i = i + 1;
        }
        
        this.w_o = Linear();
        this.w_o.init(num_heads * embed_size, embed_size, init_type);
    }
    
    function forward(list seq_x, list seq_out) void {
        var seq_len = listLength(seq_x);
        var heads_outputs = listCreate();
        
        var i = 0;
        while (i < this.num_heads) {
            var head = listGet(this.heads, i);
            var h_out = listCreate();
            head.forward(seq_x, h_out);
            listAppend(heads_outputs, h_out);
            i = i + 1;
        }
        
        var t = 0;
        while (t < seq_len) {
            var concat_t = listCreate();
            var h = 0;
            while (h < this.num_heads) {
                var h_seq = listGet(heads_outputs, h);
                var h_vec_t = listGet(h_seq, t);
                var d = 0;
                while (d < this.embed_size) {
                    listAppend(concat_t, listGet(h_vec_t, d));
                    d = d + 1;
                }
                h = h + 1;
            }
            var out_t = listCreate();
            this.w_o.forward(concat_t, out_t);
            listAppend(seq_out, out_t);
            t = t + 1;
        }
    }
    
    function parameters(list params_out) void {
        var i = 0;
        while (i < this.num_heads) {
            var head = listGet(this.heads, i);
            head.parameters(params_out);
            i = i + 1;
        }
        this.w_o.parameters(params_out);
    }
}






class VAE {
    function init(double input_dim, double hidden_dim, double latent_dim, string init_type) void {
        this.input_dim = input_dim;
        this.hidden_dim = hidden_dim;
        this.latent_dim = latent_dim;
        

        this.enc_fc1 = Linear(); this.enc_fc1.init(input_dim, hidden_dim, init_type);
        this.enc_fc_mu = Linear(); this.enc_fc_mu.init(hidden_dim, latent_dim, init_type);
        this.enc_fc_logvar = Linear(); this.enc_fc_logvar.init(hidden_dim, latent_dim, init_type);
        

        this.dec_fc1 = Linear(); this.dec_fc1.init(latent_dim, hidden_dim, init_type);
        this.dec_fc2 = Linear(); this.dec_fc2.init(hidden_dim, input_dim, init_type);
    }
    
    function reparameterize(list mu, list logvar, list z_out) void {
        var i = 0;
        var len = listLength(mu);
        while (i < len) {
            var m_i = listGet(mu, i);
            var lv_i = listGet(logvar, i);
            
            var std_t = Tensor();
            var half_t = Tensor(); half_t.init(0.5, listCreate(), "");
            var lv_half = Tensor(); lv_i.mul(half_t, lv_half);
            lv_half.exp_t(std_t);
            


            var epsilon = 0.0;
            var u = 0;
            while (u < 12) {
                epsilon = epsilon + rand();
                u = u + 1;
            }
            epsilon = epsilon - 6.0;
            
            var eps_t = Tensor(); eps_t.init(epsilon, listCreate(), "");
            
            var eps_std = Tensor();
            std_t.mul(eps_t, eps_std);
            
            var z_i = Tensor();
            m_i.add(eps_std, z_i);
            listAppend(z_out, z_i);
            
            i = i + 1;
        }
    }
    

    function forward(list x, list params_out) void {
        var h1 = listCreate();
        this.enc_fc1.forward(x, h1);
        
        var h1_act = listCreate();
        var i = 0;
        while (i < listLength(h1)) {
            var t_act = Tensor();
            var t_raw = listGet(h1, i);
            t_raw.relu(t_act);
            listAppend(h1_act, t_act);
            i = i + 1;
        }
        
        var mu = listCreate();
        this.enc_fc_mu.forward(h1_act, mu);
        
        var logvar = listCreate();
        this.enc_fc_logvar.forward(h1_act, logvar);
        
        var z = listCreate();
        this.reparameterize(mu, logvar, z);
        
        var h3 = listCreate();
        this.dec_fc1.forward(z, h3);
        
        var h3_act = listCreate();
        i = 0;
        while (i < listLength(h3)) {
            var t_act = Tensor();
            var t_raw = listGet(h3, i);
            t_raw.relu(t_act);
            listAppend(h3_act, t_act);
            i = i + 1;
        }
        
        var recon_x = listCreate();
        this.dec_fc2.forward(h3_act, recon_x);
        
        var recon_x_sig = listCreate();
        i = 0;
        while (i < listLength(recon_x)) {
            var t_act = Tensor();
            var t_raw = listGet(recon_x, i);
            t_raw.sigmoid(t_act);
            listAppend(recon_x_sig, t_act);
            i = i + 1;
        }
        
        listAppend(params_out, recon_x_sig);
        listAppend(params_out, mu);
        listAppend(params_out, logvar);
    }
    
    function parameters(list params_out) void {
        this.enc_fc1.parameters(params_out);
        this.enc_fc_mu.parameters(params_out);
        this.enc_fc_logvar.parameters(params_out);
        this.dec_fc1.parameters(params_out);
        this.dec_fc2.parameters(params_out);
    }
}

class GenerativeLosses {
    function init() void {}
    

    function VAELoss(list recon_x, list x, list mu, list logvar, Tensor out_loss) void {

        var recon_loss = Tensor(); recon_loss.init(0.0, listCreate(), "");
        var i = 0;
        var len = listLength(x);
        while (i < len) {
            var r = listGet(recon_x, i);
            var target = listGet(x, i);
            var diff = Tensor();
            r.sub(target, diff);
            var sq = Tensor();
            diff.pow_t(2.0, sq);
            var next_r = Tensor();
            recon_loss.add(sq, next_r);
            recon_loss = next_r;
            i = i + 1;
        }
        

        var kld_loss = Tensor(); kld_loss.init(0.0, listCreate(), "");
        i = 0;
        var len_z = listLength(mu);
        while (i < len_z) {
            var m = listGet(mu, i);
            var lv = listGet(logvar, i);
            
            var m_sq = Tensor(); m.pow_t(2.0, m_sq);
            var exp_lv = Tensor(); lv.exp_t(exp_lv);
            
            var one_t = Tensor(); one_t.init(1.0, listCreate(), "");
            
            var term1 = Tensor(); one_t.add(lv, term1);
            var term2 = Tensor(); term1.sub(m_sq, term2);
            var term3 = Tensor(); term2.sub(exp_lv, term3);
            
            var neg_half = Tensor(); neg_half.init(-0.5, listCreate(), "");
            var kld_i = Tensor();
            neg_half.mul(term3, kld_i);
            
            var next_kld = Tensor();
            kld_loss.add(kld_i, next_kld);
            kld_loss = next_kld;
            i = i + 1;
        }
        
        recon_loss.add(kld_loss, out_loss);
    }
}






class DataAugmentation {
    function init() void {}
    
    function flip_horizontal(list image_flat, double height, double width, list out) void {
        var h = 0;
        while (h < height) {
            var w = 0;
            while (w < width) {
                var inv_w = width - 1.0 - w;
                var idx = (h * width) + inv_w;
                listAppend(out, listGet(image_flat, idx));
                w = w + 1;
            }
            h = h + 1;
        }
    }
    
    function invert_colors(list image_flat, double max_val, list out) void {
        var i = 0;
        var len = listLength(image_flat);
        while (i < len) {
            var p = listGet(image_flat, i);
            listAppend(out, max_val - p);
            i = i + 1;
        }
    }
}

class TrainTestSplitter {
    function init() void {}
    
    function split(list xs, list ys, double test_ratio, list out_train_x, list out_train_y, list out_test_x, list out_test_y) void {
        var len = listLength(xs);
        var split_idx = floor(len * (1.0 - test_ratio));
        
        var i = 0;
        while (i < len) {
            if (i < split_idx) {
                listAppend(out_train_x, listGet(xs, i));
                listAppend(out_train_y, listGet(ys, i));
            } else {
                listAppend(out_test_x, listGet(xs, i));
                listAppend(out_test_y, listGet(ys, i));
            }
            i = i + 1;
        }
    }
}

class LogicGateGenerator {
    function init(string gate_type, double noise_level) void {
        this.gate_type = gate_type;
        this.noise_level = noise_level;
    }
    
    function generate(double samples, list out_x, list out_y) void {
        var i = 0;
        while (i < samples) {
            var bit1 = 0.0; if (rand() > 0.5) { bit1 = 1.0; }
            var bit2 = 0.0; if (rand() > 0.5) { bit2 = 1.0; }
            
            var n1 = (rand() * this.noise_level) - (this.noise_level / 2.0);
            var n2 = (rand() * this.noise_level) - (this.noise_level / 2.0);
            
            var x_vec = listCreate();
            listAppend(x_vec, bit1 + n1);
            listAppend(x_vec, bit2 + n2);
            listAppend(out_x, x_vec);
            
            var y_val = 0.0;
            if (this.gate_type == "AND") {
                if (bit1 == 1.0) { if (bit2 == 1.0) { y_val = 1.0; } }
            } else if (this.gate_type == "OR") {
                if (bit1 == 1.0) { y_val = 1.0; }
                if (bit2 == 1.0) { y_val = 1.0; }
            } else if (this.gate_type == "XOR") {
                if (bit1 != bit2) { y_val = 1.0; }
            } else if (this.gate_type == "NAND") {
                y_val = 1.0;
                if (bit1 == 1.0) { if (bit2 == 1.0) { y_val = 0.0; } }
            }
            listAppend(out_y, y_val);
            i = i + 1;
        }
    }
}






class GANGenerator {
    function init(double latent_dim, double hidden_dim, double output_dim, string init_type) void {
        this.latent_dim = latent_dim;
        this.hidden_dim = hidden_dim;
        this.output_dim = output_dim;
        
        this.fc1 = Linear(); this.fc1.init(latent_dim, hidden_dim, init_type);
        this.fc2 = Linear(); this.fc2.init(hidden_dim, hidden_dim * 2.0, init_type);
        this.fc3 = Linear(); this.fc3.init(hidden_dim * 2.0, output_dim, init_type);
    }
    
    function forward(list z, list out) void {
        var h1 = listCreate();
        this.fc1.forward(z, h1);
        
        var h1_act = listCreate();
        var i = 0;
        while (i < listLength(h1)) {
            var t_act = Tensor();
            var t_raw = listGet(h1, i);
            t_raw.relu(t_act);
            listAppend(h1_act, t_act);
            i = i + 1;
        }
        
        var h2 = listCreate();
        this.fc2.forward(h1_act, h2);
        
        var h2_act = listCreate();
        i = 0;
        while (i < listLength(h2)) {
            var t_act = Tensor();
            var t_raw = listGet(h2, i);
            t_raw.relu(t_act);
            listAppend(h2_act, t_act);
            i = i + 1;
        }
        
        var out_raw = listCreate();
        this.fc3.forward(h2_act, out_raw);
        
        i = 0;
        while (i < listLength(out_raw)) {
            var t_act = Tensor();
            var t_raw = listGet(out_raw, i);
            t_raw.tanh_t(t_act);
            listAppend(out, t_act);
            i = i + 1;
        }
    }
    
    function parameters(list params_out) void {
        this.fc1.parameters(params_out);
        this.fc2.parameters(params_out);
        this.fc3.parameters(params_out);
    }
}

class GANDiscriminator {
    function init(double input_dim, double hidden_dim, string init_type) void {
        this.input_dim = input_dim;
        this.hidden_dim = hidden_dim;
        
        this.fc1 = Linear(); this.fc1.init(input_dim, hidden_dim * 2.0, init_type);
        this.fc2 = Linear(); this.fc2.init(hidden_dim * 2.0, hidden_dim, init_type);
        this.fc3 = Linear(); this.fc3.init(hidden_dim, 1.0, init_type);
    }
    
    function forward(list x, list out) void {
        var h1 = listCreate();
        this.fc1.forward(x, h1);
        
        var h1_act = listCreate();
        var i = 0;
        while (i < listLength(h1)) {
            var t_raw = listGet(h1, i);

            if (t_raw.data > 0.0) {
                var t_act = Tensor();
                t_raw.relu(t_act);
                listAppend(h1_act, t_act);
            } else {
                var t_act = Tensor();
                var leak = Tensor(); leak.init(0.2, listCreate(), "");
                t_raw.mul(leak, t_act);
                listAppend(h1_act, t_act);
            }
            i = i + 1;
        }
        
        var h2 = listCreate();
        this.fc2.forward(h1_act, h2);
        
        var h2_act = listCreate();
        i = 0;
        while (i < listLength(h2)) {
            var t_raw = listGet(h2, i);
            if (t_raw.data > 0.0) {
                var t_act = Tensor();
                t_raw.relu(t_act);
                listAppend(h2_act, t_act);
            } else {
                var t_act = Tensor();
                var leak = Tensor(); leak.init(0.2, listCreate(), "");
                t_raw.mul(leak, t_act);
                listAppend(h2_act, t_act);
            }
            i = i + 1;
        }
        
        var out_raw = listCreate();
        this.fc3.forward(h2_act, out_raw);
        
        var t_final = Tensor();
        var t_raw_final = listGet(out_raw, 0);
        t_raw_final.sigmoid(t_final);
        listAppend(out, t_final);
    }
    
    function parameters(list params_out) void {
        this.fc1.parameters(params_out);
        this.fc2.parameters(params_out);
        this.fc3.parameters(params_out);
    }
}






class TextTokenizer {
    function init() void {
        this.vocab = listCreate();
        this.word_to_id = listCreate();
        this.id_to_word = listCreate();
    }
    
    function add_word(string word, double id) void {
        listAppend(this.word_to_id, word);
        listAppend(this.id_to_word, id);
    }
    
    function tokenize(string sentence, list out_tokens) void {



        var i = 0;
        var token_val = 0.0;
        while (i < 10) {
            token_val = token_val + rand();
            listAppend(out_tokens, token_val);
            i = i + 1;
        }
    }
}

class NLPDatasetGenerator {
    function init() void {}
    
    function generate_sequence_task(double num_samples, double seq_len, list out_sequences, list out_targets) void {
        var i = 0;
        while (i < num_samples) {
            var seq = listCreate();
            var sum = 0.0;
            var t = 0;
            while (t < seq_len) {
                var val = rand();
                var t_val = Tensor(); t_val.init(val, listCreate(), "");
                listAppend(seq, t_val);
                sum = sum + val;
                t = t + 1;
            }
            listAppend(out_sequences, seq);
            

            var target = 0.0;
            if (sum > (seq_len / 2.0)) { target = 1.0; }
            listAppend(out_targets, target);
            
            i = i + 1;
        }
    }
}

class EmbeddingLayer {
    function init(double vocab_size, double embed_size, string init_type) void {
        this.vocab_size = vocab_size;
        this.embed_size = embed_size;
        
        this.embeddings = listCreate();
        var initializer = Init();
        
        var i = 0;
        while (i < vocab_size) {
            var emb_vec = listCreate();
            var d = 0;
            while (d < embed_size) {
                var w_tensor = Tensor();
                var w_val = initializer.kaiming_uniform(embed_size);
                w_tensor.init(w_val, listCreate(), "");
                listAppend(emb_vec, w_tensor);
                d = d + 1;
            }
            listAppend(this.embeddings, emb_vec);
            i = i + 1;
        }
    }
    
    function forward(list input_ids, list out_seq) void {
        var i = 0;
        var len = listLength(input_ids);
        while (i < len) {
            var word_id_t = listGet(input_ids, i);
            var id_val = word_id_t.data;
            var idx = floor(id_val);
            if (idx >= this.vocab_size) { idx = this.vocab_size - 1.0; }
            if (idx < 0.0) { idx = 0.0; }
            
            var emb_vec = listGet(this.embeddings, idx);
            listAppend(out_seq, emb_vec);
            i = i + 1;
        }
    }
    
    function parameters(list params_out) void {
        var i = 0;
        while (i < this.vocab_size) {
            var emb_vec = listGet(this.embeddings, i);
            var d = 0;
            while (d < this.embed_size) {
                listAppend(params_out, listGet(emb_vec, d));
                d = d + 1;
            }
            i = i + 1;
        }
    }
}






class KMeans {
    function init(double n_clusters, double max_iters) void {
        this.n_clusters = n_clusters;
        this.max_iters = max_iters;
        this.centroids = listCreate();
    }
    
    function fit(list data_list, double dims) void {
        var num_samples = listLength(data_list);
        if (num_samples < this.n_clusters) { return; }
        

        var i = 0;
        while (i < this.n_clusters) {
            var rand_idx = floor(rand() * num_samples);
            var sample = listGet(data_list, rand_idx);
            
            var centroid = listCreate();
            var d = 0;
            while (d < dims) {
                var t = listGet(sample, d);
                listAppend(centroid, t.data);
                d = d + 1;
            }
            listAppend(this.centroids, centroid);
            i = i + 1;
        }
        
        var iter = 0;
        while (iter < this.max_iters) {
            var cluster_assignments = listCreate();
            var i_s = 0;
            while (i_s < num_samples) {
                var sample = listGet(data_list, i_s);
                var min_dist = 99999999.0;
                var closest_c = 0;
                
                var c = 0;
                while (c < this.n_clusters) {
                    var centroid = listGet(this.centroids, c);
                    var dist = 0.0;
                    var d = 0;
                    while (d < dims) {
                        var t = listGet(sample, d);
                        var diff = t.data - listGet(centroid, d);
                        dist = dist + (diff * diff);
                        d = d + 1;
                    }
                    if (dist < min_dist) {
                        min_dist = dist;
                        closest_c = c;
                    }
                    c = c + 1;
                }
                listAppend(cluster_assignments, closest_c);
                i_s = i_s + 1;
            }
            

            var new_centroids = listCreate();
            var counts = listCreate();
            var c_idx = 0;
            while (c_idx < this.n_clusters) {
                var empty_c = listCreate();
                var d = 0;
                while (d < dims) { listAppend(empty_c, 0.0); d = d + 1; }
                listAppend(new_centroids, empty_c);
                listAppend(counts, 0.0);
                c_idx = c_idx + 1;
            }
            
            i_s = 0;
            while (i_s < num_samples) {
                var sample = listGet(data_list, i_s);
                var assigned_c = listGet(cluster_assignments, i_s);
                
                var nc = listGet(new_centroids, assigned_c);
                var d = 0;
                while (d < dims) {
                    var t = listGet(sample, d);
                    var curr = listGet(nc, d);
                    listSet(nc, d, curr + t.data);
                    d = d + 1;
                }
                var c_count = listGet(counts, assigned_c);
                listSet(counts, assigned_c, c_count + 1.0);
                
                i_s = i_s + 1;
            }
            
            c_idx = 0;
            while (c_idx < this.n_clusters) {
                var nc = listGet(new_centroids, c_idx);
                var c_count = listGet(counts, c_idx);
                if (c_count > 0.0) {
                    var d = 0;
                    while (d < dims) {
                        var curr = listGet(nc, d);
                        listSet(nc, d, curr / c_count);
                        d = d + 1;
                    }
                }
                c_idx = c_idx + 1;
            }
            this.centroids = new_centroids;
            iter = iter + 1;
        }
    }
}






class Dropout {
    function init(double p) void {
        this.p = p;
        this.is_training = true;
    }
    
    function forward(list x, list out) void {
        var i = 0;
        var len = listLength(x);
        
        if (this.is_training) {
            var scale_factor = 1.0 / (1.0 - this.p);
            var scale_t = Tensor(); scale_t.init(scale_factor, listCreate(), "");
            var zero_t = Tensor(); zero_t.init(0.0, listCreate(), "");
            
            while (i < len) {
                var t = listGet(x, i);
                if (rand() < this.p) {

                    var dropped = Tensor();
                    t.mul(zero_t, dropped);
                    listAppend(out, dropped);
                } else {

                    var scaled = Tensor();
                    t.mul(scale_t, scaled);
                    listAppend(out, scaled);
                }
                i = i + 1;
            }
        } else {

            while (i < len) {
                listAppend(out, listGet(x, i));
                i = i + 1;
            }
        }
    }
    
    function parameters(list params_out) void { }
}

class EarlyStopping {
    function init(double patience, double min_delta) void {
        this.patience = patience;
        this.min_delta = min_delta;
        this.best_loss = 9999999.0;
        this.counter = 0.0;
        this.stop = false;
    }
    
    function step(double val_loss) void {
        if (val_loss < (this.best_loss - this.min_delta)) {
            this.best_loss = val_loss;
            this.counter = 0.0;
        } else {
            this.counter = this.counter + 1.0;
            if (this.counter >= this.patience) {
                this.stop = true;
            }
        }
    }
}

class LRScheduler {
    function init(double initial_lr, double decay_factor, double step_size) void {
        this.initial_lr = initial_lr;
        this.decay_factor = decay_factor;
        this.step_size = step_size;
        this.current_epoch = 0.0;
        this.current_lr = initial_lr;
    }
    
    function step() void {
        this.current_epoch = this.current_epoch + 1.0;
        var r = this.current_epoch / this.step_size;
        if (floor(r) == r) {
            this.current_lr = this.current_lr * this.decay_factor;
        }
    }
}






class AdvancedMetrics {
    function init() void {}
    
    function evaluate(list y_true, list y_pred, double threshold, list out_metrics) void {
        var tp = 0.0;
        var fp = 0.0;
        var tn = 0.0;
        var fn = 0.0;
        
        var i = 0;
        var len = listLength(y_true);
        while (i < len) {
            var true_val = listGet(y_true, i);
            var pred_val = listGet(y_pred, i);
            var pred_class = 0.0;
            if (pred_val > threshold) { pred_class = 1.0; }
            
            if (true_val == 1.0) {
                if (pred_class == 1.0) { tp = tp + 1.0; } else { fn = fn + 1.0; }
            } else {
                if (pred_class == 1.0) { fp = fp + 1.0; } else { tn = tn + 1.0; }
            }
            i = i + 1;
        }
        
        var precision = 0.0;
        if ((tp + fp) > 0.0) { precision = tp / (tp + fp); }
        
        var recall = 0.0;
        if ((tp + fn) > 0.0) { recall = tp / (tp + fn); }
        
        var f1 = 0.0;
        if ((precision + recall) > 0.0) { f1 = 2.0 * ((precision * recall) / (precision + recall)); }
        
        var accuracy = (tp + tn) / len;
        
        listAppend(out_metrics, accuracy);
        listAppend(out_metrics, precision);
        listAppend(out_metrics, recall);
        listAppend(out_metrics, f1);
    }
}






class TerminalGrapher {
    function init(double max_height, double max_width) void {
        this.max_height = max_height;
        this.max_width = max_width;
    }
    
    function plot(list values) void {
        var len = listLength(values);
        if (len == 0) { return; }
        
        var min_val = 9999999.0;
        var max_val = -9999999.0;
        
        var i = 0;
        while (i < len) {
            var v = listGet(values, i);
            if (v < min_val) { min_val = v; }
            if (v > max_val) { max_val = v; }
            i = i + 1;
        }
        
        if (min_val == max_val) {
            print("GrÃ¡fico Constante: " + max_val);
            return;
        }
        
        var range_v = max_val - min_val;
        print("GrÃ¡fico de Valores (Min: " + min_val + " Max: " + max_val + ")");
        
        var h = this.max_height;
        while (h >= 0.0) {
            var line_str = "| ";
            var w = 0;
            while (w < this.max_width) {
                var data_idx = floor((w / this.max_width) * len);
                if (data_idx >= len) { data_idx = len - 1.0; }
                
                var v = listGet(values, data_idx);
                var norm_v = (v - min_val) / range_v;
                var h_level = norm_v * this.max_height;
                
                if (h_level >= h) {
                    line_str = line_str + "* ";
                } else {
                    line_str = line_str + "  ";
                }
                w = w + 1;
            }
            print(line_str);
            h = h - 1.0;
        }
        var base = "+-";
        var b = 0;
        while (b < this.max_width) {
            base = base + "--";
            b = b + 1;
        }
        print(base);
    }
}






class MinMaxScaler {
    function init(double min_range, double max_range) void {
        this.min_range = min_range;
        this.max_range = max_range;
        this.data_min = listCreate();
        this.data_max = listCreate();
        this.fitted = false;
    }
    
    function fit(list dataset_list, double dims) void {
        var num_samples = listLength(dataset_list);
        if (num_samples == 0) { return; }
        
        var d = 0;
        while (d < dims) {
            listAppend(this.data_min, 999999.0);
            listAppend(this.data_max, -999999.0);
            d = d + 1;
        }
        
        var i = 0;
        while (i < num_samples) {
            var sample = listGet(dataset_list, i);
            d = 0;
            while (d < dims) {
                var t = listGet(sample, d);
                var v = t.data;
                var c_min = listGet(this.data_min, d);
                var c_max = listGet(this.data_max, d);
                if (v < c_min) { listSet(this.data_min, d, v); }
                if (v > c_max) { listSet(this.data_max, d, v); }
                d = d + 1;
            }
            i = i + 1;
        }
        this.fitted = true;
    }
    
    function transform(list dataset_list, double dims, list out_dataset) void {
        if (!this.fitted) { return; }
        var i = 0;
        var num_samples = listLength(dataset_list);
        var scale_range = this.max_range - this.min_range;
        
        while (i < num_samples) {
            var sample = listGet(dataset_list, i);
            var scaled_sample = listCreate();
            
            var d = 0;
            while (d < dims) {
                var t = listGet(sample, d);
                var v = t.data;
                var d_min = listGet(this.data_min, d);
                var d_max = listGet(this.data_max, d);
                var r = d_max - d_min;
                
                var norm_v = 0.0;
                if (r > 0.0) { norm_v = (v - d_min) / r; }
                
                var final_v = (norm_v * scale_range) + this.min_range;
                var new_t = Tensor(); new_t.init(final_v, listCreate(), "");
                listAppend(scaled_sample, new_t);
                d = d + 1;
            }
            listAppend(out_dataset, scaled_sample);
            i = i + 1;
        }
    }
}

class StandardScaler {
    function init(double eps) void {
        this.eps = eps;
        this.means = listCreate();
        this.stds = listCreate();
        this.fitted = false;
    }
    
    function fit(list dataset_list, double dims) void {
        var num_samples = listLength(dataset_list);
        if (num_samples == 0) { return; }
        
        var sums = listCreate();
        var d = 0;
        while (d < dims) {
            listAppend(sums, 0.0);
            listAppend(this.means, 0.0);
            listAppend(this.stds, 0.0);
            d = d + 1;
        }
        
        var i = 0;
        while (i < num_samples) {
            var sample = listGet(dataset_list, i);
            d = 0;
            while (d < dims) {
                var t = listGet(sample, d);
                var c_sum = listGet(sums, d);
                listSet(sums, d, c_sum + t.data);
                d = d + 1;
            }
            i = i + 1;
        }
        
        d = 0;
        while (d < dims) {
            var c_sum = listGet(sums, d);
            listSet(this.means, d, c_sum / num_samples);
            d = d + 1;
        }
        
        var sq_sums = listCreate();
        d = 0;
        while (d < dims) { listAppend(sq_sums, 0.0); d = d + 1; }
        
        i = 0;
        while (i < num_samples) {
            var sample = listGet(dataset_list, i);
            d = 0;
            while (d < dims) {
                var t = listGet(sample, d);
                var m = listGet(this.means, d);
                var diff = t.data - m;
                var c_sq = listGet(sq_sums, d);
                listSet(sq_sums, d, c_sq + (diff * diff));
                d = d + 1;
            }
            i = i + 1;
        }
        
        d = 0;
        while (d < dims) {
            var c_sq = listGet(sq_sums, d);
            var var_val = c_sq / num_samples;
            listSet(this.stds, d, sqrt(var_val + this.eps));
            d = d + 1;
        }
        this.fitted = true;
    }
    
    function transform(list dataset_list, double dims, list out_dataset) void {
        if (!this.fitted) { return; }
        var num_samples = listLength(dataset_list);
        
        var i = 0;
        while (i < num_samples) {
            var sample = listGet(dataset_list, i);
            var scaled_sample = listCreate();
            
            var d = 0;
            while (d < dims) {
                var t = listGet(sample, d);
                var m = listGet(this.means, d);
                var s = listGet(this.stds, d);
                var val = (t.data - m) / s;
                var new_t = Tensor(); new_t.init(val, listCreate(), "");
                listAppend(scaled_sample, new_t);
                d = d + 1;
            }
            listAppend(out_dataset, scaled_sample);
            i = i + 1;
        }
    }
}






class AdvancedActivations {
    function init() void {}
    

    function gelu(Tensor x, Tensor out) void {
        var x_val = x.data;
        var x3 = x_val * x_val * x_val;
        var inner = (x_val + (0.044715 * x3)) * 0.797884;
        

        var e_pos = exp(inner);
        var e_neg = exp(-1.0 * inner);
        var t_val = (e_pos - e_neg) / (e_pos + e_neg);
        
        var final_val = x_val * 0.5 * (1.0 + t_val);
        
        var children = listCreate();
        listAppend(children, x);
        out.init(final_val, children, "gelu");
    }
    

    function swish(Tensor x, Tensor out) void {
        var sig_val = 1.0 / (1.0 + exp(-1.0 * x.data));
        var final_val = x.data * sig_val;
        
        var children = listCreate();
        listAppend(children, x);
        out.init(final_val, children, "swish");
    }
    

    function mish(Tensor x, Tensor out) void {


        var e_val = exp(x.data);
        var softplus_val = 0.0;
        if (e_val < 100.0) {

            softplus_val = x.data; 
        } else {
            softplus_val = x.data;
        }
        
        var e_pos = exp(softplus_val);
        var e_neg = exp(-1.0 * softplus_val);
        var t_val = (e_pos - e_neg) / (e_pos + e_neg);
        
        var final_val = x.data * t_val;
        
        var children = listCreate();
        listAppend(children, x);
        out.init(final_val, children, "mish");
    }
}






class ModelCheckpoint {
    function init(string file_path, double save_freq) void {
        this.file_path = file_path;
        this.save_freq = save_freq;
        this.current_epoch = 0.0;
    }
    
    function step(list parameters) void {
        this.current_epoch = this.current_epoch + 1.0;
        var r = this.current_epoch / this.save_freq;
        if (floor(r) == r) {
            printColor("cyan", "Salvando Checkpoint do Modelo em " + this.file_path + " [Epoch " + this.current_epoch + "]");
            var param_count = listLength(parameters);
            var i = 0;
            var checksum = 0.0;
            while (i < param_count) {
                var t = listGet(parameters, i);
                checksum = checksum + t.data;
                i = i + 1;
            }
            print(" -> Total ParÃ¢metros Registrados: " + param_count);
            print(" -> Hash Estrutural de MemÃ³ria: " + checksum);
        }
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

class CrossEntropyLoss {
    function init() void {}
    
    function forward(list preds, list targets, Tensor out) void {
        var total_loss = Tensor();
        total_loss.init(0.0, listCreate(), "");
        
        var i = 0;
        var len = listLength(preds);
        while (i < len) {
            var p = listGet(preds, i);
            var t = listGet(targets, i);
            
            var log_p = Tensor();
            p.log_t(log_p);
            
            var target_t = Tensor();
            target_t.init(t, listCreate(), "");
            
            var term = Tensor();
            target_t.mul(log_p, term);
            
            var next_loss = Tensor();
            total_loss.sub(term, next_loss);
            total_loss = next_loss;
            
            i = i + 1;
        }
        
        var div_t = Tensor();
        div_t.init(1.0 * len, listCreate(), "");
        total_loss.div(div_t, out);
    }
}

class Adagrad {
    function init(list parameters, double lr) void {
        this.parameters = parameters;
        this.lr = lr;
        this.eps = 0.00000001;
        this.sum_sq_grad = listCreate();
        
        var i = 0;
        var len = listLength(parameters);
        while (i < len) {
            listAppend(this.sum_sq_grad, 0.0);
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
        var i = 0;
        var len = listLength(this.parameters);
        while (i < len) {
            var p = listGet(this.parameters, i);
            var s_sq = listGet(this.sum_sq_grad, i);
            
            s_sq = s_sq + (p.grad * p.grad);
            listSet(this.sum_sq_grad, i, s_sq);
            
            p.data = p.data - (this.lr * p.grad / (sqrt(s_sq) + this.eps));
            i = i + 1;
        }
    }
}

class AdaDelta {
    function init(list parameters, double rho) void {
        this.parameters = parameters;
        this.rho = rho;
        this.eps = 0.00000001;
        this.e_sq_grad = listCreate();
        this.e_sq_delta = listCreate();
        
        var i = 0;
        var len = listLength(parameters);
        while (i < len) {
            listAppend(this.e_sq_grad, 0.0);
            listAppend(this.e_sq_delta, 0.0);
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
        var i = 0;
        var len = listLength(this.parameters);
        while (i < len) {
            var p = listGet(this.parameters, i);
            var eg2 = listGet(this.e_sq_grad, i);
            var ed2 = listGet(this.e_sq_delta, i);
            
            eg2 = (this.rho * eg2) + ((1.0 - this.rho) * (p.grad * p.grad));
            listSet(this.e_sq_grad, i, eg2);
            
            var delta = -1.0 * (sqrt(ed2 + this.eps) / (sqrt(eg2 + this.eps))) * p.grad;
            
            ed2 = (this.rho * ed2) + ((1.0 - this.rho) * (delta * delta));
            listSet(this.e_sq_delta, i, ed2);
            
            p.data = p.data + delta;
            i = i + 1;
        }
    }
}

class ParallelGridSearch {
    function init() void {
        this.results_lr = listCreate();
        this.results_bs = listCreate();
        this.results_loss = listCreate();
    }
    
    function fit(list learning_rates, list batch_sizes) void {
        var num_lr = listLength(learning_rates);
        var num_bs = listLength(batch_sizes);
        
        var thread_ids = listCreate();
        var thread_lrs = listCreate();
        var thread_bss = listCreate();
        
        var i = 0;
        while (i < num_lr) {
            var lr = listGet(learning_rates, i);
            var j = 0;
            while (j < num_bs) {
                var bs = listGet(batch_sizes, j);
                
                var script_name = "pgs_worker_" + valueToString(lr) + "_" + valueToString(bs) + ".sp";
                var script_content = "
                import " + getQuote() + "sapphire_grad.sp" + getQuote() + ";
                
                var lr = " + valueToString(lr) + ";
                var bs = " + valueToString(bs) + ";
                
                var xs = listCreate();
                var x1 = listCreate(); listAppend(x1, 0.0); listAppend(x1, 0.0);
                var x2 = listCreate(); listAppend(x2, 0.0); listAppend(x2, 1.0);
                var x3 = listCreate(); listAppend(x3, 1.0); listAppend(x3, 0.0);
                var x4 = listCreate(); listAppend(x4, 1.0); listAppend(x4, 1.0);
                listAppend(xs, x1); listAppend(xs, x2); listAppend(xs, x3); listAppend(xs, x4);

                var ys = listCreate();
                listAppend(ys, 0.0);
                listAppend(ys, 1.0);
                listAppend(ys, 1.0);
                listAppend(ys, 1.0);
                
                var xs_t = listCreate();
                var i = 0;
                while (i < listLength(xs)) {
                    var curr_x = listGet(xs, i);
                    var t_x = listCreate();
                    var j = 0;
                    while (j < listLength(curr_x)) {
                        var t = Tensor();
                        t.init(listGet(curr_x, j), listCreate(), " + getQuote() + "input" + getQuote() + ");
                        listAppend(t_x, t);
                        j = j + 1;
                    }
                    listAppend(xs_t, t_x);
                    i = i + 1;
                }
                
                var nouts = listCreate();
                listAppend(nouts, 4.0);
                listAppend(nouts, 1.0);
                
                var activations = listCreate();
                listAppend(activations, " + getQuote() + "relu" + getQuote() + ");
                listAppend(activations, " + getQuote() + "sigmoid" + getQuote() + ");
                
                var model = MLP();
                model.init(2.0, nouts, activations, " + getQuote() + "xavier" + getQuote() + ");
                
                var model_params = listCreate();
                model.parameters(model_params);
                
                var opt = Adam();
                opt.init(model_params, lr);
                
                var loader = DataLoader();
                loader.init(xs_t, ys, bs, true);
                
                var total_loss = 0.0;
                var epoch = 0;
                while (epoch < 20) {
                    var batches = listCreate();
                    loader.get_batches(batches);
                    var b = 0;
                    while (b < listLength(batches)) {
                        var batch_pair = listGet(batches, b);
                        var b_x = listGet(batch_pair, 0);
                        var b_y = listGet(batch_pair, 1);
                        
                        var iter_loss = Tensor();
                        iter_loss.init(0.0, listCreate(), " + getQuote() + "" + getQuote() + ");
                        
                        var s = 0;
                        var b_size = listLength(b_x);
                        while (s < b_size) {
                            var x_in = listGet(b_x, s);
                            var y_targ = listGet(b_y, s);
                            
                            var out_list = listCreate();
                            model.forward(x_in, out_list);
                            
                            var target_t = Tensor();
                            target_t.init(y_targ, listCreate(), " + getQuote() + "" + getQuote() + ");
                            
                            var p = listGet(out_list, 0);
                            var diff = Tensor(); p.sub(target_t, diff);
                            var sq = Tensor(); diff.pow_t(2.0, sq);
                            var next_loss = Tensor(); iter_loss.add(sq, next_loss);
                            iter_loss = next_loss;
                            s = s + 1;
                        }
                        var div_t = Tensor(); div_t.init(1.0 * b_size, listCreate(), " + getQuote() + "" + getQuote() + ");
                        var final_loss = Tensor();
                        iter_loss.div(div_t, final_loss);
                        
                        opt.zero_grad();
                        final_loss.backward();
                        opt.step();
                        
                        total_loss = final_loss.data;
                        b = b + 1;
                    }
                    epoch = epoch + 1;
                }
                
                var result_name = " + getQuote() + "pgs_result_" + getQuote() + " + valueToString(lr) + " + getQuote() + "_" + getQuote() + " + valueToString(bs) + " + getQuote() + ".txt" + getQuote() + ";
                writeFile(result_name, valueToString(total_loss));
                ";
                
                writeFile(script_name, script_content);
                
                var tid = spawn(script_name);
                listAppend(thread_ids, tid);
                listAppend(thread_lrs, lr);
                listAppend(thread_bss, bs);
                
                j = j + 1;
            }
            i = i + 1;
        }
        
        var k = 0;
        var thread_count = listLength(thread_ids);
        while (k < thread_count) {
            var tid = listGet(thread_ids, k);
            join(tid);
            
            var lr = listGet(thread_lrs, k);
            var bs = listGet(thread_bss, k);
            var result_name = "pgs_result_" + valueToString(lr) + "_" + valueToString(bs) + ".txt";
            
            var res_str = readFile(result_name);
            var final_loss = 999.0;
            if (valueToString(res_str) != "") {
                final_loss = parseDouble(res_str);
            }
            
            listAppend(this.results_lr, lr);
            listAppend(this.results_bs, bs);
            listAppend(this.results_loss, final_loss);
            
            // Clean up temporary files
            deleteFile(result_name);
            var script_name = "pgs_worker_" + valueToString(lr) + "_" + valueToString(bs) + ".sp";
            deleteFile(script_name);
            
            k = k + 1;
        }
    }
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

class RNNCell {
    function init(double input_dim, double hidden_dim, string init_type) void {
        this.input_dim = input_dim;
        this.hidden_dim = hidden_dim;
        
        this.w_ih = Linear();
        this.w_ih.init(input_dim, hidden_dim, init_type);
        
        this.w_hh = Linear();
        this.w_hh.init(hidden_dim, hidden_dim, init_type);
    }
    
    function forward(list x, list h_prev, list h_next) void {
        var ih_out = listCreate();
        this.w_ih.forward(x, ih_out);
        
        var hh_out = listCreate();
        this.w_hh.forward(h_prev, hh_out);
        
        var i = 0;
        while (i < this.hidden_dim) {
            var ih_t = listGet(ih_out, i);
            var hh_t = listGet(hh_out, i);
            
            var sum_t = Tensor();
            ih_t.add(hh_t, sum_t);
            
            var final_t = Tensor();
            sum_t.tanh_t(final_t);
            
            listAppend(h_next, final_t);
            i = i + 1;
        }
    }
    
    function parameters(list params_out) void {
        this.w_ih.parameters(params_out);
        this.w_hh.parameters(params_out);
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
    print("Vocabulary Size: " + valueToString(vocab_size));
    
    var dim = 64.0;
    var num_heads = 4.0;
    var num_layers = 4.0;
    var max_seq_len = 30.0;
    
    var model = TransformerLanguageModel();
    model.init(vocab_size, dim, num_heads, num_layers, max_seq_len, "xavier");
    
    var parameters = listCreate();
    model.parameters(parameters);
    print("Model Parameters count: " + valueToString(listLength(parameters)));
    
    var optim = Adam();
    optim.init(parameters, 0.01);
    
    var epochs = 50;
    
    print("Starting training...");
    var epoch = 0;
    while (epoch < epochs) {
        var total_loss = 0.0;
        
        var j = 0;
        while (j < listLength(dataset)) {
            var text = listGet(dataset, j);
            var ids = listCreate();
            tokenizer.encode(text, ids);
            
            var input_ids = listCreate();
            var target_ids = listCreate();
            var k = 0;
            while (k < listLength(ids) - 1) {
                listAppend(input_ids, listGet(ids, k));
                listAppend(target_ids, listGet(ids, k + 1));
                k = k + 1;
            }
            
            if (listLength(input_ids) > 0) {
                optim.zero_grad();
                
                var logits = listCreate();
                model.forward(input_ids, logits);
                
                var loss_fn = SequenceCrossEntropyLoss();
                var loss_t = Tensor();
                loss_fn.forward(logits, target_ids, loss_t);
                
                loss_t.backward();
                optim.step();
                
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
