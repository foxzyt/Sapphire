import "sapphire_grad.sp";

print("--- Training a Circle Classification Model ---");

// Generate a synthetic dataset: classifying if a point (x,y) is inside a circle of radius 0.7
var xs = listCreate();
print("listCreate ys");
var ys = listCreate();

print("InitUtil = Init()");
var InitUtil = Init();

// Generate 200 random points between -1.0 and 1.0
print("loop");
var i = 0;
var num_samples = 200;
while (i < num_samples) {
    print("i = " + valueToString(i));
    var x_val = InitUtil.uniform(-1.0, 1.0);
    var y_val = InitUtil.uniform(-1.0, 1.0);
    
    var sample = listCreate();
    listAppend(sample, x_val);
    listAppend(sample, y_val);
    
    var t_sample = listCreate();
    var tx = Tensor(); tx.init(x_val, listCreate(), "input");
    var ty = Tensor(); ty.init(y_val, listCreate(), "input");
    listAppend(t_sample, tx);
    listAppend(t_sample, ty);
    
    listAppend(xs, t_sample);
    
    // Circle equation: x^2 + y^2 < r^2
    var dist_sq = (x_val * x_val) + (y_val * y_val);
    if (dist_sq < (0.7 * 0.7)) {
        listAppend(ys, 1.0);
    } else {
        listAppend(ys, 0.0);
    }
    
    i = i + 1;
}

print("Dataset generated with 200 samples.");

// Define the Neural Network Architecture
// 2 Inputs -> 8 Neurons (ReLU) -> 8 Neurons (ReLU) -> 1 Output (Sigmoid)
var nouts = listCreate();
listAppend(nouts, 8.0);
listAppend(nouts, 8.0);
listAppend(nouts, 1.0);

var activations = listCreate();
listAppend(activations, "relu");
listAppend(activations, "relu");
listAppend(activations, "sigmoid");

var model = MLP();
model.init(2.0, nouts, activations, "xavier");

var model_params = listCreate();
model.parameters(model_params);
print("Model initialized with " + valueToString(listLength(model_params)) + " parameters.");

// Setup Optimizer and DataLoader
var opt = Adam();
opt.init(model_params, 0.05);

var loader = DataLoader();
var batch_size = 16.0;
loader.init(xs, ys, batch_size, true);

// Training Loop
var epochs = 30;
var epoch = 0;
var epoch_loss = 0.0;
while (epoch < epochs) {
    var batches = listCreate();
    loader.get_batches(batches);
    
    var b = 0;
    epoch_loss = 0.0;
    while (b < listLength(batches)) {
        var batch_pair = listGet(batches, b);
        var b_x = listGet(batch_pair, 0);
        var b_y = listGet(batch_pair, 1);
        
        var iter_loss = Tensor();
        iter_loss.init(0.0, listCreate(), "");
        
        var s = 0;
        var b_size = listLength(b_x);
        while (s < b_size) {
            var x_in = listGet(b_x, s);
            var y_targ = listGet(b_y, s);
            
            var out_list = listCreate();
            model.forward(x_in, out_list);
            
            var target_t = Tensor();
            target_t.init(y_targ, listCreate(), "");
            
            var p = listGet(out_list, 0);
            
            // Mean Squared Error (MSE) for simplicity
            var diff = Tensor(); p.sub(target_t, diff);
            var sq = Tensor(); diff.pow_t(2.0, sq);
            
            var next_loss = Tensor(); iter_loss.add(sq, next_loss);
            iter_loss = next_loss;
            
            s = s + 1;
        }
        
        var div_t = Tensor(); div_t.init(1.0 * b_size, listCreate(), "");
        var final_loss = Tensor();
        iter_loss.div(div_t, final_loss);
        
        opt.zero_grad();
        final_loss.backward();
        opt.step();
        
        epoch_loss = epoch_loss + final_loss.data;
        b = b + 1;
    }
    
    var avg_loss = epoch_loss / listLength(batches);
    print("Epoch " + valueToString(epoch) + " | Loss: " + valueToString(avg_loss));
    
    epoch = epoch + 1;
}

print("Training finished! Final Epoch Loss: " + valueToString(epoch_loss));

// Let's do a quick evaluation on a few points
print("--- Evaluation ---");
var eval_pts = listCreate();
var ev1 = listCreate(); listAppend(ev1, 0.0); listAppend(ev1, 0.0); // Inside circle -> 1.0
var ev2 = listCreate(); listAppend(ev2, 0.9); listAppend(ev2, 0.9); // Outside circle -> 0.0
var ev3 = listCreate(); listAppend(ev3, -0.2); listAppend(ev3, -0.4); // Inside circle -> 1.0
listAppend(eval_pts, ev1);
listAppend(eval_pts, ev2);
listAppend(eval_pts, ev3);

var e = 0;
while (e < listLength(eval_pts)) {
    var pt = listGet(eval_pts, e);
    var t_pt = listCreate();
    var tx = Tensor(); tx.init(listGet(pt, 0), listCreate(), "");
    var ty = Tensor(); ty.init(listGet(pt, 1), listCreate(), "");
    listAppend(t_pt, tx);
    listAppend(t_pt, ty);
    
    var out_list = listCreate();
    model.forward(t_pt, out_list);
    var prob = listGet(out_list, 0).data;
    
    print("Point (" + valueToString(listGet(pt, 0)) + ", " + valueToString(listGet(pt, 1)) + ") -> Predicted: " + valueToString(prob));
    e = e + 1;
}

print("Done.");
