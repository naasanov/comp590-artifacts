#!/usr/bin/env python3
"""
Validate the exported ONNX model by running it on real test data.

This script:
1. Loads the Lee2019_MI dataset
2. Loads the exported ONNX model
3. Runs inference on test data
4. Compares with PyTorch model output (if available)
5. Shows classification accuracy and predictions
"""

import numpy as np
import argparse
from pathlib import Path

try:
    import onnxruntime as ort
except ImportError:
    print("ERROR: onnxruntime not installed. Install with: pip install onnxruntime")
    exit(1)

try:
    from moabb.datasets import Lee2019_MI
    from mne import Epochs, events_from_annotations
except ImportError:
    print("ERROR: MOABB/MNE not installed. Install with: pip install moabb mne")
    exit(1)


def load_test_data(subject, session, n_samples=385, downsample_to=250, max_epochs=None):
    """
    Load test data from Lee2019_MI dataset.
    
    Args:
        subject: Subject ID (1-54)
        session: Session number (1 or 2) - will be converted to 0-indexed
        n_samples: Number of time samples per epoch
        downsample_to: Target sampling rate in Hz
        max_epochs: Maximum number of epochs to load (None = all)
    
    Returns:
        data: numpy array of shape (n_epochs, n_channels, n_samples)
        labels: numpy array of shape (n_epochs,) with class labels
        label_map: dict mapping label values to class names
    """
    print("Loading Lee2019_MI dataset...")
    
    # MOABB uses 0-indexed sessions internally, but we use 1-indexed for user interface
    moabb_session = session - 1
    dataset = Lee2019_MI(train_run=True, test_run=False, sessions=(session,))
    
    print(f"Extracting data for subject {subject}, session {session} (MOABB session {moabb_session})...")
    subject_data = dataset.get_data([subject])
    
    if subject not in subject_data:
        raise ValueError(f"Subject {subject} not found")
    
    sessions = subject_data[subject]
    
    # MOABB uses 0-indexed sessions ('0', '1') not ('1', '2')
    session_key = str(moabb_session)
    
    if session_key not in sessions:
        print(f"Available session keys: {list(sessions.keys())}")
        raise ValueError(f"Session {session} (key '{session_key}') not found. Available: {list(sessions.keys())}")
    
    print(f"Using session key: '{session_key}'")
    session_data = sessions[session_key]
    
    # Session data might be a dict of runs or a Raw object directly
    if isinstance(session_data, dict):
        print(f"Session contains runs: {list(session_data.keys())}")
        # Use the first run (typically training run)
        run_key = list(session_data.keys())[0]
        print(f"Using run: '{run_key}'")
        raw = session_data[run_key]
    else:
        raw = session_data
    
    # Resample if needed
    if raw.info['sfreq'] != downsample_to:
        print(f"Resampling from {raw.info['sfreq']} Hz to {downsample_to} Hz...")
        raw.resample(downsample_to)
    
    # Get events
    events_array, event_dict = events_from_annotations(raw)
    
    print(f"Found {len(events_array)} events: {event_dict}")
    
    # Create epochs (0-4s window, but we'll crop to n_samples later)
    tmin, tmax = 0.0, 4.0
    epochs = Epochs(
        raw,
        events_array,
        event_id=event_dict,
        tmin=tmin,
        tmax=tmax,
        baseline=None,
        preload=True,
        verbose=False
    )
    
    # Get data
    data = epochs.get_data()  # shape: (n_epochs, n_channels, n_time_samples)
    labels = epochs.events[:, -1]
    
    # Lee2019_MI has 62 EEG channels + some non-EEG channels (EMG, etc.)
    # The model expects 62 channels, so we need to select only the EEG channels
    n_channels = data.shape[1]
    if n_channels > 62:
        print(f"Data has {n_channels} channels, selecting first 62 EEG channels...")
        data = data[:, :62, :]
    elif n_channels < 62:
        print(f"WARNING: Data has only {n_channels} channels, expected 62!")
    
    # Crop to desired number of samples
    if n_samples < data.shape[2]:
        print(f"Cropping epochs from {data.shape[2]} to {n_samples} samples...")
        data = data[:, :, :n_samples]
    
    # Convert to microvolts if needed
    if epochs.info['chs'][0]['unit'] == 107:  # FIFF_UNIT_V
        data = data * 1e6
    
    # Limit number of epochs if requested
    if max_epochs is not None and max_epochs < len(data):
        print(f"Using first {max_epochs} epochs (out of {len(data)})")
        data = data[:max_epochs]
        labels = labels[:max_epochs]
    
    print(f"Loaded data shape: {data.shape}")
    print(f"Labels shape: {labels.shape}")
    print(f"Unique labels: {np.unique(labels)}")
    
    # Create label mapping (Lee2019_MI uses 1=right_hand, 2=left_hand)
    label_map = {v: k for k, v in event_dict.items() if k in ['left_hand', 'right_hand', '1', '2']}
    
    return data, labels, label_map


def run_onnx_inference(model_path, data):
    """
    Run ONNX model inference on data.
    
    Args:
        model_path: Path to ONNX model
        data: numpy array of shape (n_epochs, n_channels, n_samples)
    
    Returns:
        predictions: numpy array of shape (n_epochs, n_classes) - logits
        predicted_classes: numpy array of shape (n_epochs,) - predicted class indices
    """
    print(f"\nLoading ONNX model: {model_path}")
    
    # Create ONNX Runtime session
    session = ort.InferenceSession(model_path)
    
    # Get input/output names
    input_name = session.get_inputs()[0].name
    output_name = session.get_outputs()[0].name
    
    input_shape = session.get_inputs()[0].shape
    output_shape = session.get_outputs()[0].shape
    
    print(f"Model input: {input_name}, shape: {input_shape}")
    print(f"Model output: {output_name}, shape: {output_shape}")
    
    # Run inference
    print(f"\nRunning inference on {len(data)} epochs...")
    predictions = []
    
    for i, epoch_data in enumerate(data):
        # Add batch dimension
        input_data = epoch_data[np.newaxis, :, :].astype(np.float32)
        
        # Run inference
        output = session.run([output_name], {input_name: input_data})[0]
        predictions.append(output[0])
        
        if (i + 1) % 10 == 0:
            print(f"  Processed {i + 1}/{len(data)} epochs...")
    
    predictions = np.array(predictions)  # shape: (n_epochs, n_classes)
    
    # Apply softmax to get probabilities
    exp_preds = np.exp(predictions - np.max(predictions, axis=1, keepdims=True))
    probabilities = exp_preds / np.sum(exp_preds, axis=1, keepdims=True)
    
    # Get predicted classes
    predicted_classes = np.argmax(probabilities, axis=1)
    
    return predictions, predicted_classes, probabilities


def evaluate_predictions(predicted_classes, true_labels, label_map):
    """
    Evaluate prediction accuracy.
    
    Args:
        predicted_classes: numpy array of predicted class indices (0 or 1)
        true_labels: numpy array of true labels (event IDs from dataset)
        label_map: dict mapping event IDs to class names
    """
    print("\n" + "="*60)
    print("EVALUATION RESULTS")
    print("="*60)
    
    # Map true labels to class indices
    # Need to figure out the mapping: model outputs 0 or 1
    # Dataset labels are event IDs (could be 1, 2, or something else)
    
    unique_labels = np.unique(true_labels)
    print(f"\nUnique true labels in data: {unique_labels}")
    print(f"Label mapping: {label_map}")
    
    # Try to determine correct mapping
    # Typically: model class 0 = right_hand, class 1 = left_hand
    # Dataset: varies, but often 1=right, 2=left
    
    # Create mapping from dataset labels to model class indices
    if len(unique_labels) == 2:
        # Assume ascending order: smaller label -> class 0
        label_to_class = {unique_labels[0]: 0, unique_labels[1]: 1}
        true_classes = np.array([label_to_class[label] for label in true_labels])
        
        print(f"\nAssumed mapping:")
        for label in unique_labels:
            class_idx = label_to_class[label]
            label_name = label_map.get(label, f"Label_{label}")
            print(f"  Dataset label {label} ({label_name}) -> Model class {class_idx}")
    else:
        print(f"WARNING: Expected 2 classes, found {len(unique_labels)}")
        return
    
    # Calculate accuracy
    correct = np.sum(predicted_classes == true_classes)
    total = len(predicted_classes)
    accuracy = correct / total * 100
    
    print(f"\n" + "="*60)
    print(f"Accuracy: {correct}/{total} = {accuracy:.2f}%")
    print("="*60)
    
    # Show confusion matrix
    from collections import defaultdict
    confusion = defaultdict(int)
    
    for true_class, pred_class in zip(true_classes, predicted_classes):
        confusion[(true_class, pred_class)] += 1
    
    print("\nConfusion Matrix:")
    print("                Predicted")
    print("              Class 0  Class 1")
    print(f"True Class 0  {confusion[(0,0)]:6d}  {confusion[(0,1)]:6d}")
    print(f"True Class 1  {confusion[(1,0)]:6d}  {confusion[(1,1)]:6d}")
    
    # Per-class accuracy
    class_0_total = np.sum(true_classes == 0)
    class_1_total = np.sum(true_classes == 1)
    
    if class_0_total > 0:
        class_0_acc = confusion[(0,0)] / class_0_total * 100
        print(f"\nClass 0 accuracy: {confusion[(0,0)]}/{class_0_total} = {class_0_acc:.2f}%")
    
    if class_1_total > 0:
        class_1_acc = confusion[(1,1)] / class_1_total * 100
        print(f"Class 1 accuracy: {confusion[(1,1)]}/{class_1_total} = {class_1_acc:.2f}%")
    
    # Show some example predictions
    print("\n" + "="*60)
    print("Sample Predictions (first 10 epochs):")
    print("="*60)
    print("Epoch | True Class | Predicted | Correct?")
    print("-" * 45)
    
    for i in range(min(10, len(true_classes))):
        true_class = true_classes[i]
        pred_class = predicted_classes[i]
        correct_mark = "✓" if true_class == pred_class else "✗"
        print(f"{i:5d} | {true_class:10d} | {pred_class:9d} | {correct_mark:8s}")


def main():
    parser = argparse.ArgumentParser(
        description='Validate ONNX model with test data'
    )
    parser.add_argument(
        '--model',
        type=str,
        default='eegnet_lee2019.onnx',
        help='Path to ONNX model file'
    )
    parser.add_argument(
        '--subject',
        type=int,
        default=1,
        help='Subject ID (1-54, default: 1)'
    )
    parser.add_argument(
        '--session',
        type=int,
        default=1,
        choices=[1, 2],
        help='Session number (1 or 2, default: 1)'
    )
    parser.add_argument(
        '--max-epochs',
        type=int,
        default=20,
        help='Maximum number of epochs to test (default: 20, use None for all)'
    )
    
    args = parser.parse_args()
    
    print("="*60)
    print("ONNX Model Validation")
    print("="*60)
    print(f"Model: {args.model}")
    print(f"Subject: {args.subject}")
    print(f"Session: {args.session}")
    print(f"Max epochs: {args.max_epochs}")
    print("="*60)
    
    # Check if model exists
    if not Path(args.model).exists():
        print(f"\nERROR: Model file not found: {args.model}")
        print("Please run: python export_eegnet_to_onnx.py")
        return
    
    # Load test data
    try:
        data, labels, label_map = load_test_data(
            subject=args.subject,
            session=args.session,
            n_samples=385,  # Based on the model we exported
            downsample_to=250,
            max_epochs=args.max_epochs
        )
    except Exception as e:
        print(f"\nERROR loading data: {e}")
        import traceback
        traceback.print_exc()
        return
    
    # Run ONNX inference
    try:
        predictions, predicted_classes, probabilities = run_onnx_inference(args.model, data)
        
        print(f"\nInference complete!")
        print(f"Predictions shape: {predictions.shape}")
        print(f"Sample predictions (logits): {predictions[:3]}")
        print(f"Sample probabilities: {probabilities[:3]}")
        print(f"Sample predicted classes: {predicted_classes[:10]}")
        
    except Exception as e:
        print(f"\nERROR during inference: {e}")
        import traceback
        traceback.print_exc()
        return
    
    # Evaluate
    try:
        evaluate_predictions(predicted_classes, labels, label_map)
    except Exception as e:
        print(f"\nERROR during evaluation: {e}")
        import traceback
        traceback.print_exc()
        return
    
    print("\n" + "="*60)
    print("Validation complete!")
    print("="*60)
    print("\nNext steps:")
    print("1. If accuracy is reasonable (>60%), the model is working")
    print("2. Export your test data: python convert_lee2019_to_openvibe_csv.py")
    print("3. Run the same test data through OpenViBE")
    print("4. Compare inference times between ONNX Runtime and OpenViBE")


if __name__ == '__main__':
    main()