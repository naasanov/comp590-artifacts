"""
OpenViBE Python Box: Classifier Comparator
Compares ONNX vs Native LDA predictions in real-time
Tracks: accuracy, precision, recall, F1, latency, throughput
"""

import json
import time

class MyOVBox(OVBox):
    def __init__(self):
        OVBox.__init__(self)
        # Store only counts and running statistics, not all predictions
        self.n_samples = 0
        self.onnx_correct = 0
        self.native_correct = 0
        self.agreement = 0
        
        # Per-class metrics (cumulative)
        self.onnx_tp = [0, 0]
        self.onnx_fp = [0, 0]
        self.onnx_fn = [0, 0]
        self.native_tp = [0, 0]
        self.native_fp = [0, 0]
        self.native_fn = [0, 0]
        
        # Timing - only store deltas for latency
        self.last_prediction_time = 0.0
        self.onnx_latency_sum = 0.0
        self.native_latency_sum = 0.0
        self.latency_count = 0
        
        # Buffers for current batch
        self.onnx_pending = []
        self.native_pending = []
        self.label_pending = []
        
        self.start_time = 0.0
        self.first_prediction_time = 0.0
        self.received_end = False
        self.last_process_time = 0.0
        self.last_sample_count = 0
        self.report_generated = False
    
    def getSettings(self, index):
        if index == 0:
            return '/Users/vidurshah/Documents/School/bci-proj/comparator.py'
        elif index == 1:
            return ''
        
    def initialize(self):
        self.start_time = time.time()
        self.last_process_time = time.time()
        print('[Python] Comparator initialized')
        return True
    
    def process(self):
        current_time = time.time()
        
        # Debug logging
        if not hasattr(self, '_process_count'):
            self._process_count = 0
        self._process_count += 1
        
        if self._process_count == 1:
            print('[Python] process() called - {} inputs available'.format(len(self.input)))
            for i in range(len(self.input)):
                print('[Python] Input {}: {} chunks'.format(i, len(self.input[i])))
        
        # Unified stimulation processing - single loop for all inputs
        stim_map = {0x00008101: 0, 0x00008102: 1}
        timeout_signals = (0x00008005, 0x00008001, 0x00008002, 0x00008016)  # ExperimentStop, SegmentStop, TrainCompleted, BaselineStop
        
        # Process all inputs: 0=ONNX, 1=Native, 2=True labels, 3=Clock timeout, 4=Signal (ignored)
        for input_idx in range(min(5, len(self.input))):
            for chunkIdx in range(len(self.input[input_idx])):
                chunk = self.input[input_idx].pop()
                if type(chunk) == OVStimulationSet:
                    for stimIdx in range(len(chunk)):
                        stim = chunk.pop()
                        stim_id = stim.identifier
                        
                        # Debug: log first few stimulations
                        if self._process_count < 50 and (stim_id in stim_map or stim_id in timeout_signals):
                            print('[Python] Input {}: Received stim 0x{:08x}'.format(input_idx, stim_id))
                        
                        # Check for timeout signals on any input
                        if stim_id in timeout_signals:
                            print('[Python] Received timeout signal: 0x{:08x}'.format(stim_id))
                            self.received_end = True
                        
                        # Process predictions only from specific inputs
                        elif stim_id in stim_map:
                            if input_idx == 0:  # ONNX predictions
                                self.onnx_pending.append(stim_map[stim_id])
                                if self.first_prediction_time == 0.0:
                                    self.first_prediction_time = current_time
                            elif input_idx == 1:  # Native predictions
                                self.native_pending.append(stim_map[stim_id])
                            elif input_idx == 2:  # True labels
                                self.label_pending.append(stim_map[stim_id])
        
        # Process complete triplets and update streaming statistics
        min_pending = min(len(self.onnx_pending), len(self.native_pending), len(self.label_pending))
        if min_pending > 0:
            for i in range(min_pending):
                onnx_pred = self.onnx_pending[i]
                native_pred = self.native_pending[i]
                true_label = self.label_pending[i]
                
                # Update streaming statistics
                if onnx_pred == true_label:
                    self.onnx_correct += 1
                    self.onnx_tp[true_label] += 1
                else:
                    self.onnx_fp[onnx_pred] += 1
                    self.onnx_fn[true_label] += 1
                
                if native_pred == true_label:
                    self.native_correct += 1
                    self.native_tp[true_label] += 1
                else:
                    self.native_fp[native_pred] += 1
                    self.native_fn[true_label] += 1
                
                if onnx_pred == native_pred:
                    self.agreement += 1
                
                # Track latency using deltas
                if self.last_prediction_time > 0.0:
                    self.onnx_latency_sum += current_time - self.last_prediction_time
                    self.native_latency_sum += current_time - self.last_prediction_time
                    self.latency_count += 1
                self.last_prediction_time = current_time
                
                self.n_samples += 1
            
            # Remove processed items
            self.onnx_pending = self.onnx_pending[min_pending:]
            self.native_pending = self.native_pending[min_pending:]
            self.label_pending = self.label_pending[min_pending:]
            
            # Save incremental results after each batch
            self.save_current_results()
        
        # Check completion conditions
        if self.received_end and self.n_samples > 0:
            if not self.report_generated:
                self.generate_final_report()
                self.report_generated = True
        elif self.n_samples > 0:
            if self.n_samples != self.last_sample_count:
                self.last_sample_count = self.n_samples
                self.last_process_time = current_time
        
        return True
    
    def generate_final_report(self):
        """Generate and save the final comparison report"""
        if self.n_samples == 0:
            print('[Python] ERROR: No predictions received!')
            return
        
        end_time = time.time()
        
        # Calculate precision, recall, F1 from accumulated statistics
        def calc_metrics(tp, fp, fn):
            precision = [0.0, 0.0]
            recall = [0.0, 0.0]
            f1 = [0.0, 0.0]
            for cls in (0, 1):
                denom_p = tp[cls] + fp[cls]
                denom_r = tp[cls] + fn[cls]
                if denom_p > 0:
                    precision[cls] = float(tp[cls]) / denom_p
                if denom_r > 0:
                    recall[cls] = float(tp[cls]) / denom_r
                if precision[cls] + recall[cls] > 0:
                    f1[cls] = 2.0 * precision[cls] * recall[cls] / (precision[cls] + recall[cls])
            return precision, recall, f1
        
        onnx_prec, onnx_rec, onnx_f1 = calc_metrics(self.onnx_tp, self.onnx_fp, self.onnx_fn)
        native_prec, native_rec, native_f1 = calc_metrics(self.native_tp, self.native_fp, self.native_fn)
        
        # Calculate timing metrics
        total_runtime = end_time - self.start_time
        processing_time = end_time - self.first_prediction_time if self.first_prediction_time > 0.0 else 0.0
        throughput = self.n_samples / processing_time if processing_time > 0.0 else 0.0
        
        onnx_avg_latency = self.onnx_latency_sum / self.latency_count if self.latency_count > 0 else 0
        native_avg_latency = self.native_latency_sum / self.latency_count if self.latency_count > 0 else 0
        
        onnx_acc = float(self.onnx_correct) / self.n_samples * 100
        native_acc = float(self.native_correct) / self.n_samples * 100
        agreement_rate = float(self.agreement) / self.n_samples * 100
        
        # Print summary
        print('')
        print('=' * 70)
        print('CLASSIFIER COMPARISON RESULTS')
        print('=' * 70)
        print('Samples: {}  |  Runtime: {:.1f}s  |  Throughput: {:.1f} samples/sec'.format(
            self.n_samples, total_runtime, throughput))
        print('')
        print('ONNX:   Accuracy {:.1f}%  |  F1 {:.3f}  |  Latency {:.1f}ms'.format(
            onnx_acc, (onnx_f1[0] + onnx_f1[1]) / 2, onnx_avg_latency * 1000))
        print('Native: Accuracy {:.1f}%  |  F1 {:.3f}  |  Latency {:.1f}ms'.format(
            native_acc, (native_f1[0] + native_f1[1]) / 2, native_avg_latency * 1000))
        print('')
        print('Agreement: {}/{} ({:.1f}%)'.format(self.agreement, self.n_samples, agreement_rate))
        print('=' * 70)
    
    def save_current_results(self):
        """Save current results incrementally - survives early termination"""
        if self.n_samples == 0:
            return
        
        end_time = time.time()
        
        # Calculate metrics from current state
        def calc_metrics(tp, fp, fn):
            precision = [0.0, 0.0]
            recall = [0.0, 0.0]
            f1 = [0.0, 0.0]
            for cls in (0, 1):
                denom_p = tp[cls] + fp[cls]
                denom_r = tp[cls] + fn[cls]
                if denom_p > 0:
                    precision[cls] = float(tp[cls]) / denom_p
                if denom_r > 0:
                    recall[cls] = float(tp[cls]) / denom_r
                if precision[cls] + recall[cls] > 0:
                    f1[cls] = 2.0 * precision[cls] * recall[cls] / (precision[cls] + recall[cls])
            return precision, recall, f1
        
        onnx_prec, onnx_rec, onnx_f1 = calc_metrics(self.onnx_tp, self.onnx_fp, self.onnx_fn)
        native_prec, native_rec, native_f1 = calc_metrics(self.native_tp, self.native_fp, self.native_fn)
        
        total_runtime = end_time - self.start_time
        processing_time = end_time - self.first_prediction_time if self.first_prediction_time > 0.0 else 0.0
        throughput = self.n_samples / processing_time if processing_time > 0.0 else 0.0
        
        onnx_avg_latency = self.onnx_latency_sum / self.latency_count if self.latency_count > 0 else 0
        native_avg_latency = self.native_latency_sum / self.latency_count if self.latency_count > 0 else 0
        
        onnx_acc = float(self.onnx_correct) / self.n_samples * 100
        native_acc = float(self.native_correct) / self.n_samples * 100
        agreement_rate = float(self.agreement) / self.n_samples * 100
        
        # Save results
        results = {
            'overview': {
                'samples_processed': self.n_samples,
                'total_runtime_sec': total_runtime,
                'processing_time_sec': processing_time,
                'throughput_samples_per_sec': throughput
            },
            'onnx': {
                'accuracy_percent': onnx_acc,
                'correct_predictions': self.onnx_correct,
                'avg_latency_ms': onnx_avg_latency * 1000,
                'class_0': {'precision': onnx_prec[0], 'recall': onnx_rec[0], 'f1_score': onnx_f1[0]},
                'class_1': {'precision': onnx_prec[1], 'recall': onnx_rec[1], 'f1_score': onnx_f1[1]},
                'macro_avg_f1': (onnx_f1[0] + onnx_f1[1]) / 2
            },
            'native': {
                'accuracy_percent': native_acc,
                'correct_predictions': self.native_correct,
                'avg_latency_ms': native_avg_latency * 1000,
                'class_0': {'precision': native_prec[0], 'recall': native_rec[0], 'f1_score': native_f1[0]},
                'class_1': {'precision': native_prec[1], 'recall': native_rec[1], 'f1_score': native_f1[1]},
                'macro_avg_f1': (native_f1[0] + native_f1[1]) / 2
            },
            'comparison': {
                'agreement_rate_percent': agreement_rate,
                'agreements': self.agreement,
                'accuracy_difference_percent': abs(onnx_acc - native_acc),
                'f1_difference': abs((onnx_f1[0] + onnx_f1[1]) / 2 - (native_f1[0] + native_f1[1]) / 2)
            }
        }
        
        try:
            results_path = '/Users/vidurshah/Documents/School/bci-proj/results/comparison.json'
            with open(results_path, 'w') as f:
                json.dump(results, f, indent=2)
        except Exception as e:
            pass
    
    def uninitialize(self):
        if not self.report_generated:
            self.generate_final_report()
            self.report_generated = True
        return True

box = MyOVBox()
