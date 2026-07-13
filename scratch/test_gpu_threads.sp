// test_gpu_threads.sp
import "sapphire_grad.sp";

print("--- Testing GPULinear and ParallelGridSearch ---");

// 1. Verify log operator
var t = Tensor();
t.init(2.718281828, listCreate(), "");
var out_log = Tensor();
t.log_t(out_log);
print("Tensor log value: " + valueToString(out_log.data));
out_log.backward();
print("Tensor log gradient: " + valueToString(t.grad));

// 2. Verify Softmax and CrossEntropyLoss
var preds = listCreate();
var p1 = Tensor(); p1.init(0.1, listCreate(), "");
var p2 = Tensor(); p2.init(0.8, listCreate(), "");
var p3 = Tensor(); p3.init(0.1, listCreate(), "");
listAppend(preds, p1); listAppend(preds, p2); listAppend(preds, p3);

var targets = listCreate();
listAppend(targets, 0.0); listAppend(targets, 1.0); listAppend(targets, 0.0);

var sm = Softmax();
var sm_out = listCreate();
sm.forward(preds, sm_out);
print("Softmax output: [" + valueToString(listGet(sm_out, 0).data) + ", " + valueToString(listGet(sm_out, 1).data) + ", " + valueToString(listGet(sm_out, 2).data) + "]");

var ce = CrossEntropyLoss();
var ce_loss = Tensor();
ce.forward(sm_out, targets, ce_loss);
print("CrossEntropy Loss: " + valueToString(ce_loss.data));

// 3. Verify GPULinear
var gpu_layer = GPULinear();
gpu_layer.init(2.0, 3.0, "xavier");
print("GPULinear active? " + valueToString(gpu_layer.gpu_active));

var input_x = listCreate();
var x0 = Tensor(); x0.init(1.0, listCreate(), "");
var x1 = Tensor(); x1.init(2.0, listCreate(), "");
listAppend(input_x, x0); listAppend(input_x, x1);

var output_y = listCreate();
gpu_layer.forward(input_x, output_y);
print("GPULinear output length: " + valueToString(listLength(output_y)));

// 4. Verify ParallelGridSearch
print("Starting ParallelGridSearch...");
var pgs = ParallelGridSearch();
pgs.init();
var lrs = listCreate();
listAppend(lrs, 0.1);
listAppend(lrs, 0.01);

var bss = listCreate();
listAppend(bss, 2.0);

pgs.fit(lrs, bss);
print("ParallelGridSearch completed.");
var idx = 0;
while (idx < listLength(pgs.results_loss)) {
    print("Trial " + valueToString(idx) + ": lr=" + valueToString(listGet(pgs.results_lr, idx)) + ", bs=" + valueToString(listGet(pgs.results_bs, idx)) + " -> Loss=" + valueToString(listGet(pgs.results_loss, idx)));
    idx = idx + 1;
}

print("Done all tests successfully.");
