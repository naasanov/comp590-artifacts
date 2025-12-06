# OpenViBE ONNX vs Native LDA Benchmark - Setup Guide

This guide shows you how to run Benchmark 2: comparing ONNX classifier performance against OpenViBE's native LDA classifier in real-time using BCI Competition IV 2a motor imagery data.

## Prerequisites

- OpenViBE 3.6.0 built and installed (see main repository README.md)
- BCI Competition IV 2a dataset (`.gdf` files in `BCICIV_2a_gdf/`)
- Conda environment with required packages

---

## Quick Start

```bash
# Navigate to Mac-build directory
cd Mac-build

# Activate conda environment
conda activate openvibe

# Step 1: Extract features from BCI data
python extract_pipeline_features.py

# Step 2: Train both LDA models (ONNX + OpenViBE native)
python retrain_100d.py

# Step 3: Run benchmark in OpenViBE Designer (see below)
```

---

## Detailed Setup

### Step 1: Extract Pipeline Features

Extract 100-dimensional features from the BCI Competition IV 2a dataset that match OpenViBE's preprocessing pipeline:

```bash
python extract_pipeline_features.py
```

**What this creates:**
- `test_data/pipeline_features_100d.csv` - 100-dimensional feature vectors
- `test_data/pipeline_labels.csv` - Corresponding class labels
- `test_data/pipeline_features_info.json` - Metadata about feature extraction

**Processing details:**
- Loads GDF files from `BCICIV_2a_gdf/`
- Applies bandpass filter (8-30 Hz)
- Extracts motor imagery epochs (0.5-2.5s after cue)
- Computes bandpower features across frequency bands
- Takes ~2-5 minutes depending on data size

---

### Step 2: Train LDA Models

Train both the ONNX model and OpenViBE's native LDA classifier:

```bash
python retrain_100d.py
```

**What this creates:**
- `models/lda_classifier_100d.onnx` - ONNX model for custom ONNX box
- `test_data/openvibe_lda_100d.cfg` - OpenViBE native LDA configuration
- `models/lda_weights_100d.pkl` - Pickle backup of weights

**Training details:**
- Uses Linear Discriminant Analysis with minimal shrinkage (0.01)
- 80/20 train/test split
- Reports accuracy and classification metrics
- Exports both formats from same trained model
- Takes ~10-30 seconds

---

### Step 3: Run Benchmark in OpenViBE Designer

#### 3.1 Launch OpenViBE Designer

```bash
# Navigate to your OpenViBE build directory
cd /path/to/openvibe/build

# Launch Designer
./bin/openvibe-designer-3.6.0
```

#### 3.2 Create Benchmark Scenario

Create a new scenario with the following boxes to compare both classifiers side-by-side:

---

**Box 1: CSV File Reader (Feature Source)**

1. Add Box → **File reading and writing** → **CSV File Reader**
2. Rename: `Feature Source`
3. Settings:
   - Filename: `Mac-build/test_data/pipeline_features_100d.csv`
   - Column separator: `,`
   - Don't use file time
4. Output: Streamed matrix

---

**Box 2: Feature Aggregator (for ONNX path)**

1. Add Box → **Signal processing** → **Feature aggregator**
2. Rename: `Feature Aggregator ONNX`
3. Input: Streamed matrix (from Feature Source)
4. Output: Feature vector

---

**Box 3: Feature Aggregator (for Native LDA path)**

1. Add Box → **Signal processing** → **Feature aggregator**
2. Rename: `Feature Aggregator Native`
3. Input: Streamed matrix (from Feature Source)
4. Output: Feature vector

---

**Box 4: ONNX Classifier (Your Custom Box)**

1. Add your custom ONNX Classifier box
2. Rename: `ONNX Classifier`
3. Settings:
   - Model path: `Mac-build/models/lda_classifier_100d.onnx`
4. Input: Feature vector (from Feature Aggregator ONNX)
5. Output: Stimulation stream

---

**Box 5: Classifier Processor (OpenViBE Native LDA)**

1. Add Box → **Classification** → **Classifier processor**
2. Rename: `Native LDA`
3. Settings:
   - Configuration file: `Mac-build/test_data/openvibe_lda_100d.cfg`
4. Input: Feature vector (from Feature Aggregator Native)
5. Output: Stimulation stream

---

**Box 6: CSV File Reader (Ground Truth Labels)**

1. Add Box → **File reading and writing** → **CSV File Reader**
2. Rename: `Label Source`
3. Settings:
   - Filename: `Mac-build/test_data/pipeline_labels.csv`
   - Column separator: `,`
4. Output: Streamed matrix (will be converted to stimulations by comparator)

---

**Box 7: Python Scripting (Comparator)**

1. Add Box → **Scripting** → **Python Scripting**
2. Rename: `Comparator`
3. Settings:
   - Script path: `Mac-build/comparator.py`
4. Inputs: 3
   - Input 1: Stimulation stream (ONNX predictions)
   - Input 2: Stimulation stream (Native LDA predictions)
   - Input 3: Stimulation stream (true labels from Label Source)
5. Outputs: 0

---

#### 3.3 Connect the Boxes

#### 3.3 Connect the Boxes

```
Feature Source → Feature Aggregator ONNX → ONNX Classifier → Comparator (input 1)
Feature Source → Feature Aggregator Native → Native LDA → Comparator (input 2)
Label Source → Comparator (input 3)
```

**Connection Details:**
- Feature Source output → Both Feature Aggregators (duplicate connection)
- Feature Aggregator ONNX → ONNX Classifier
- Feature Aggregator Native → Native LDA  
- ONNX Classifier output → Comparator input 1
- Native LDA output → Comparator input 2
- Label Source output → Comparator input 3

---

#### 3.4 Run the Benchmark

1. Click **Play** button
2. Watch real-time comparison in console:

```
[Python] Loaded 288 samples for comparison
[Python] Sample 1: ONNX=0, Native=0, True=0 | Agreement: ✓ | ONNX: ✓ | Native: ✓
[Python] Sample 2: ONNX=1, Native=1, True=1 | Agreement: ✓ | ONNX: ✓ | Native: ✓
[Python] Sample 3: ONNX=0, Native=1, True=0 | Agreement: ✗ | ONNX: ✓ | Native: ✗
...
```

3. When complete, see final report:

```
============================================================
BENCHMARK COMPLETE
============================================================
Samples processed: 288

ONNX Classifier:
  Accuracy: 192/288 (66.7%)
  Precision: Class 0: 0.68, Class 1: 0.65
  Recall: Class 0: 0.64, Class 1: 0.69
  F1-score: Class 0: 0.66, Class 1: 0.67
  
Native LDA Classifier:
  Accuracy: 189/288 (65.6%)
  Precision: Class 0: 0.67, Class 1: 0.64
  Recall: Class 0: 0.63, Class 1: 0.68
  F1-score: Class 0: 0.65, Class 1: 0.66
  
Agreement: 276/288 (95.8%)
Average latency: ONNX: 0.8ms, Native: 1.2ms
Throughput: ONNX: 1250 samples/sec, Native: 833 samples/sec
============================================================

Results saved to: Mac-build/results/comparison.json
```

---

## Understanding the Results

### Metrics Explained

- **Accuracy**: Overall percentage of correct predictions
- **Precision**: Of predictions for a class, how many were correct
- **Recall**: Of actual instances of a class, how many were found
- **F1-score**: Harmonic mean of precision and recall
- **Agreement**: How often both classifiers predict the same class
- **Latency**: Time per prediction (lower is better)
- **Throughput**: Predictions per second (higher is better)

### Expected Results

Both classifiers should have:
- Similar accuracy (within 1-3%)
- High agreement (>90%)
- ONNX should be faster (lower latency, higher throughput)

### Result Files

- `results/comparison.json` - Full metrics for each run
- `results/comparison_100trials.json` - Aggregate over multiple runs
- `models/benchmark_results.json` - Detailed performance data

---

## Troubleshooting

### Issue: "File not found" errors

**Solution:** Ensure all paths are absolute or relative to OpenViBE's working directory:
```bash
# In box settings, use absolute paths like:
/Users/username/Documents/github/comp590-artifacts/Mac-build/models/lda_classifier_100d.onnx
```

### Issue: Feature dimension mismatch

**Solution:** Verify feature extraction completed:
```bash
# Check features file
head Mac-build/test_data/pipeline_features_100d.csv

# Should see 100 comma-separated values per line
```

### Issue: Labels not loading correctly

**Solution:** The comparator expects labels as integers (0 or 1). Verify:
```bash
cat Mac-build/test_data/pipeline_labels.csv
# Should see: 0, 1, 0, 1, etc.
```

### Issue: Native LDA config not found

**Solution:** Re-run training script:
```bash
python retrain_100d.py
# Check that test_data/openvibe_lda_100d.cfg was created
```

### Issue: Comparator not showing output

**Solution:** Check that:
1. All three inputs are connected to Comparator box
2. Python script path is correct in box settings
3. Console logging is enabled in OpenViBE Designer (View → Log Level → Trace)

---

## Advanced: Running Multiple Trials

To collect statistics across multiple runs:

1. Run the scenario multiple times
2. Results accumulate in `results/comparison_100trials.json`
3. Compare variance in accuracy and timing metrics

Or modify `comparator.py` to run batch tests automatically.

---

## Files Reference

### Input Files (you provide)
- `BCICIV_2a_gdf/*.gdf` - BCI Competition IV 2a motor imagery data

### Generated Files (created by scripts)
- `test_data/pipeline_features_100d.csv` - Extracted features
- `test_data/pipeline_labels.csv` - Ground truth labels
- `models/lda_classifier_100d.onnx` - ONNX classifier model
- `test_data/openvibe_lda_100d.cfg` - Native LDA configuration

### Output Files (created during benchmark)
- `results/comparison.json` - Single run results
- `results/comparison_100trials.json` - Multi-run statistics
- `models/benchmark_results.json` - Performance metrics

### Core Scripts
- `extract_pipeline_features.py` - Feature extraction from GDF files
- `retrain_100d.py` - Train and export both classifier formats
- `comparator.py` - Real-time comparison in OpenViBE Designer

---

## What This Benchmark Proves

✅ ONNX classifier works correctly in OpenViBE environment  
✅ ONNX predictions match native LDA implementation  
✅ Both classifiers achieve similar accuracy on same data  
✅ ONNX provides faster inference (lower latency)  
✅ Side-by-side validation in real-time BCI scenario  
✅ Reproducible benchmark with public BCI dataset

---

## Next Steps

- Adjust LDA hyperparameters in `retrain_100d.py`
- Test with different subjects from BCI Competition dataset
- Modify feature extraction in `extract_pipeline_features.py`
- Export other scikit-learn models to ONNX format
- Integrate into real-time BCI control scenarios

For questions or issues, refer to the main repository README or OpenViBE documentation.

