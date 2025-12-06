#!/usr/bin/env python3
"""
Retrain LDA classifiers with 100-dimensional features from OpenViBE pipeline
Creates both ONNX and OpenViBE native LDA configurations
"""

import numpy as np
from pathlib import Path
from sklearn.discriminant_analysis import LinearDiscriminantAnalysis
from sklearn.model_selection import train_test_split
from sklearn.metrics import accuracy_score, classification_report
import pickle
import json

# ONNX export
try:
    import onnx
    from skl2onnx import convert_sklearn
    from skl2onnx.common.data_types import FloatTensorType
    ONNX_AVAILABLE = True
except ImportError:
    print("Warning: ONNX libraries not available")
    ONNX_AVAILABLE = False


def train_lda(features: np.ndarray, labels: np.ndarray, test_size: float = 0.2):
    """Train LDA classifier on 100-dimensional features"""
    
    print("\n" + "="*70)
    print("Training LDA with 100-dimensional features")
    print("="*70)
    
    # Split data
    X_train, X_test, y_train, y_test = train_test_split(
        features, labels, test_size=test_size, random_state=42, stratify=labels
    )
    
    print(f"\nTrain/Test split:")
    print(f"  Train: {len(X_train)} samples")
    print(f"  Test: {len(X_test)} samples")
    
    # Train LDA with shrinkage for high-dimensional data
    print(f"\nTraining LDA classifier...")
    print(f"  Features: {X_train.shape[1]}")
    print(f"  Classes: {np.unique(y_train)}")
    
    # Use minimal shrinkage to avoid zero coefficients
    print(f"  Using solver='lsqr' with shrinkage=0.01 (minimal regularization)")
    
    lda = LinearDiscriminantAnalysis(solver='lsqr', shrinkage=0.01)
    lda.fit(X_train, y_train)
    
    # Evaluate
    train_pred = lda.predict(X_train)
    train_acc = accuracy_score(y_train, train_pred)
    print(f"  Training accuracy: {train_acc:.4f}")
    
    test_pred = lda.predict(X_test)
    test_acc = accuracy_score(y_test, test_pred)
    print(f"\nEvaluating LDA classifier...")
    print(f"  Test accuracy: {test_acc:.4f}")
    
    print("\nClassification Report:")
    print(classification_report(y_test, test_pred, 
                                target_names=['left_hand', 'right_hand']))
    
    return lda, X_test, y_test


def export_onnx(lda, n_features: int, output_path: Path):
    """Export LDA model to ONNX format"""
    
    if not ONNX_AVAILABLE:
        print("ERROR: ONNX libraries not available. Cannot export.")
        return False
    
    try:
        print(f"\nExporting to ONNX...")
        print(f"  Input shape: [1, {n_features}]")
        
        # Define input type with explicit batch size of 1
        initial_type = [('float_input', FloatTensorType([1, n_features]))]
        
        # Convert to ONNX
        onnx_model = convert_sklearn(lda, initial_types=initial_type, target_opset=12)
        
        # Save model
        output_path = Path(output_path)
        with open(output_path, 'wb') as f:
            f.write(onnx_model.SerializeToString())
        
        print(f"Exported ONNX model to {output_path}")
        
        # Verify the model
        onnx.checker.check_model(onnx_model)
        print("ONNX model validated successfully")
        
        # Verify shape
        input_shape = [d.dim_value for d in onnx_model.graph.input[0].type.tensor_type.shape.dim]
        print(f"Verified input shape: {input_shape}")
        
        return True
        
    except Exception as e:
        print(f"ERROR exporting to ONNX: {e}")
        return False


def export_openvibe_config(lda, output_path: Path):
    """Export LDA to OpenViBE configuration format"""
    
    try:
        print(f"\nExporting OpenViBE LDA configuration...")
        
        # Extract LDA parameters
        coef = lda.coef_[0]  # Coefficients: (n_features,)
        intercept = lda.intercept_[0]  # Intercept: scalar
        
        n_features = len(coef)
        
        # DEBUG: Print coefficient statistics
        print(f"DEBUG: coef shape: {coef.shape}")
        print(f"DEBUG: First 10 coefs: {coef[:10]}")
        print(f"DEBUG: Non-zero count: {(coef != 0).sum()}")
        
        # For binary LDA, sklearn uses: decision = X @ coef + intercept
        # If decision > 0: predict class 1, else: predict class 0
        # OpenViBE expects same format - single hyperplane
        
        # Create XML configuration matching OpenViBE format
        xml_lines = [
            '<OpenViBE-Classifier-Box Creator="Python Script" CreatorVersion="1.0.0" FormatVersion="4">',
            '\t<Strategy-Identifier class-id="(0xffffffff, 0xffffffff)">Native</Strategy-Identifier>',
            '\t<Algorithm-Identifier class-id="(0x2ba17a3c, 0x1bd46d84)">Linear Discrimimant Analysis (LDA)</Algorithm-Identifier>',
            '\t<Stimulations>',
            '\t\t<Class-Stimulation class-id="0">OVTK_StimulationId_Label_01</Class-Stimulation>',
            '\t\t<Class-Stimulation class-id="1">OVTK_StimulationId_Label_02</Class-Stimulation>',
            '\t</Stimulations>',
            '\t<OpenViBE-Classifier>',
            '\t\t<LDA version="1">',
            '\t\t\t<Classes>0 1 </Classes>',
            '\t\t\t<Class-config-list>',
            '\t\t\t\t<Class-config>',
            '\t\t\t\t\t<Weights> ' + ' '.join(f'{x:.6e}' for x in coef) + '</Weights>',
            f'\t\t\t\t\t<Bias>{intercept:.6f}</Bias>',
            '\t\t\t\t</Class-config>',
            '\t\t\t</Class-config-list>',
            '\t\t</LDA>',
            '\t</OpenViBE-Classifier>',
            '</OpenViBE-Classifier-Box>'
        ]
        
        xml_content = '\n'.join(xml_lines)
        
        # Save to file
        output_path = Path(output_path)
        with open(output_path, 'w') as f:
            f.write(xml_content)
        
        print(f"Exported OpenViBE config to {output_path}")
        print(f"  Features: {n_features}")
        print(f"  Classes: 2")
        
        return True
        
    except Exception as e:
        print(f"ERROR exporting OpenViBE config: {e}")
        import traceback
        traceback.print_exc()
        return False


def main():
    """Main training pipeline"""
    
    # Load 100-dimensional features
    features_path = Path("test_data/pipeline_features_100d.csv")
    labels_path = Path("test_data/pipeline_labels.csv")
    
    if not features_path.exists():
        print(f"ERROR: Features file not found: {features_path}")
        print("Please run extract_pipeline_features.py first!")
        return
    
    print("="*70)
    print("Retraining Models with 100-Dimensional Features")
    print("="*70)
    
    print(f"\nLoading features from: {features_path}")
    features = np.loadtxt(features_path, delimiter=',')
    labels = np.loadtxt(labels_path, delimiter=',', dtype=int)
    
    print(f"  Features shape: {features.shape}")
    print(f"  Labels shape: {labels.shape}")
    print(f"  Label distribution: {np.bincount(labels)}")
    
    # Train LDA
    lda, X_test, y_test = train_lda(features, labels)
    
    # Create output directory
    output_dir = Path("models")
    output_dir.mkdir(exist_ok=True)
    
    # Export to ONNX
    onnx_path = output_dir / "lda_classifier_100d.onnx"
    onnx_success = export_onnx(lda, n_features=100, output_path=onnx_path)
    
    # Export to OpenViBE format
    openvibe_path = Path("test_data/openvibe_lda_100d.cfg")
    openvibe_success = export_openvibe_config(lda, output_path=openvibe_path)
    
    # Save weights as pickle
    weights_path = output_dir / "lda_weights_100d.pkl"
    with open(weights_path, 'wb') as f:
        pickle.dump(lda, f)
    print(f"\nSaved weights to {weights_path}")
    
    # Summary
    print("\n" + "="*70)
    print("Training Complete!")
    print("="*70)
    print(f"\nModels saved to: {output_dir.absolute()}")
    if onnx_success:
        print(f"  ✓ ONNX model: {onnx_path}")
        print(f"    Use in ONNX Box with path: {onnx_path.absolute()}")
    if openvibe_success:
        print(f"  ✓ OpenViBE config: {openvibe_path}")
        print(f"    Use in Classifier processor with path: {openvibe_path.absolute()}")
    
    print("\n" + "="*70)
    print("Next steps:")
    print("1. Update ONNX Box model path to:", onnx_path.absolute())
    print("2. Update Classifier processor config path to:", openvibe_path.absolute())
    print("3. Run the scenario in OpenViBE Designer")
    print("="*70)


if __name__ == "__main__":
    main()
