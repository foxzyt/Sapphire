import "sapphire_grad.sp";

function main() void {
    printColor("cyan", "--- SaphireGrad V2.0 Training Test ---");
    
    // Dataset (Logical OR)
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
    
    // Converte inputs para tensores
    var xs_t = listCreate();
    var i = 0;
    while (i < listLength(xs)) {
        var curr_x = listGet(xs, i);
        var t_x = listCreate();
        var j = 0;
        while (j < listLength(curr_x)) {
            var t = Tensor();
            t.init(listGet(curr_x, j), listCreate(), "input");
            listAppend(t_x, t);
            j = j + 1;
        }
        listAppend(xs_t, t_x);
        i = i + 1;
    }
    
    // Configura o modelo MLP
    var nouts = listCreate();
    listAppend(nouts, 4.0);
    listAppend(nouts, 1.0);
    
    var activations = listCreate();
    listAppend(activations, "relu");
    listAppend(activations, "sigmoid");
    
    var model = MLP();
    model.init(2.0, nouts, activations, "xavier");
    
    var model_params = listCreate();
    model.parameters(model_params);
    
    var opt = Adam();
    opt.init(model_params, 0.1); // Learning rate de 0.1 para Adam
    
    var loader = DataLoader();
    loader.init(xs_t, ys, 2.0, true); // Batch size 2, Shuffle true
    
    var loss_api = LossAPI();
    
    var trainer = Trainer();
    trainer.init(model, opt, loss_api, loader, 50.0); // 50 epochs
    
    trainer.fit();
    
    printColor("cyan", "Final predictions:");
    i = 0;
    while (i < listLength(xs_t)) {
        var x_input = listGet(xs_t, i);
        var pred_list = listCreate();
        model.eval();
        model.forward(x_input, pred_list);
        var pred = listGet(pred_list, 0);
        
        var x_raw = listGet(xs, i);
        var x0 = listGet(x_raw, 0);
        var x1_val = listGet(x_raw, 1);
        
        print "Input: [" + x0 + ", " + x1_val + "] -> Pred: " + pred.data;
        i = i + 1;
    }
}

main();
