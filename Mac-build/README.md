# Mac-build - Benchmark 2: ONNX vs Native LDA Comparison

This directory contains all files needed to run Benchmark 2, which compares ONNX classifier performance against OpenViBE's native LDA classifier using BCI Competition IV 2a motor imagery data.

## Directory Structure

```
Mac-build/
├── SETUP_GUIDE.md                    # Complete setup and usage instructions
├── comparator.py                     # Python script to compare classifiers in real-time
├── extract_pipeline_features.py      # Extract features from BCI data
├── retrain_100d.py                   # Train and export both LDA models
├── environment.yml                   # Conda environment for M2 Mac
├── requirements.txt                  # Python dependencies
├── BCICIV_2a_gdf/                    # BCI Competition IV 2a dataset (.gdf files)
├── models/                           # Trained models (generated)
│   ├── lda_classifier_100d.onnx     # ONNX model for custom box
│   ├── lda_weights_100d.pkl         # Pickle backup
│   ├── lda_weights.json             # Human-readable weights
│   ├── metadata.json                # Training configuration
│   └── benchmark_results.json       # Performance metrics
├── test_data/                        # Test data and configurations (generated)
│   ├── pipeline_features_100d.csv   # 100D feature vectors
│   ├── pipeline_labels.csv          # Ground truth labels
│   ├── openvibe_lda_100d.cfg       # Native LDA configuration
│   ├── pipeline_features_info.json  # Feature extraction metadata
│   └── reference*.json              # Reference predictions
└── results/                          # Benchmark results (generated)
    ├── comparison.json              # Single run metrics
    └── comparison_100trials.json    # Multi-run statistics
```

## Quick Start

```bash
# 1. Extract features
python extract_pipeline_features.py

# 2. Train models
python retrain_100d.py

# 3. Run benchmark in OpenViBE Designer
# See SETUP_GUIDE.md for detailed instructions
```

## Core Files

### Scripts
- **comparator.py** - Compares ONNX vs Native LDA in OpenViBE Designer
- **extract_pipeline_features.py** - Extracts 100D features from GDF files
- **retrain_100d.py** - Trains LDA and exports to both ONNX and OpenViBE formats

### Configuration
- **environment.yml** - Conda environment for Mac M2 (arm64)
- **requirements.txt** - Python packages (ONNX, scikit-learn, MNE, etc.)

### Data Directories
- **BCICIV_2a_gdf/** - BCI Competition IV 2a motor imagery dataset
- **models/** - Generated classifier models
- **test_data/** - Generated features and configurations  
- **results/** - Benchmark output (metrics, comparisons)

## What This Benchmark Does

1. **Feature Extraction**: Processes BCI motor imagery data through OpenViBE-style pipeline
2. **Model Training**: Trains LDA on 100-dimensional features, exports to both formats
3. **Real-time Comparison**: Runs both classifiers side-by-side in OpenViBE Designer
4. **Performance Metrics**: Measures accuracy, precision, recall, F1, latency, throughput

## Expected Results

- Both classifiers achieve similar accuracy (within 1-3%)
- High agreement between classifiers (>90%)
- ONNX shows lower latency and higher throughput
- Validates ONNX implementation matches native LDA

## Documentation

See **SETUP_GUIDE.md** for complete setup instructions, troubleshooting, and usage examples.
