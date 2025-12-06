#!/usr/bin/env python3
"""
Extract 100-dimensional features from OpenViBE pipeline
This script runs the GDF data through the same preprocessing as the Designer scenario
and extracts features to retrain the classifiers
"""

import numpy as np
import mne
from pathlib import Path
from scipy import signal as sp_signal
from typing import Tuple
import json

class OpenViBEFeatureExtractor:
    """Extract features matching OpenViBE Designer pipeline configuration"""
    
    def __init__(self, gdf_path: str, sampling_rate: float = 250.0):
        self.gdf_path = Path(gdf_path)
        self.sampling_rate = sampling_rate
        
    def load_and_preprocess(self, lowcut: float = 8.0, highcut: float = 30.0) -> Tuple[mne.io.Raw, np.ndarray]:
        """Load GDF file and apply bandpass filtering (matching Simple DSP boxes)"""
        print(f"Loading {self.gdf_path}...")
        raw = mne.io.read_raw_gdf(self.gdf_path, preload=True, verbose=False)
        events, event_id = mne.events_from_annotations(raw, verbose=False)
        
        print(f"  Sample rate: {raw.info['sfreq']} Hz")
        print(f"  Channels: {len(raw.ch_names)}")
        print(f"  Duration: {raw.times[-1]:.1f} seconds")
        
        # Bandpass filter (matching Simple DSP boxes in Designer)
        print(f"Bandpass filtering: {lowcut}-{highcut} Hz")
        raw.filter(lowcut, highcut, fir_design='firwin', verbose=False)
        
        return raw, events
    
    def create_epochs(self, raw: mne.io.Raw, events: np.ndarray, 
                      tmin: float = 0.5, tmax: float = 2.5,
                      event_ids: dict = None) -> mne.Epochs:
        """Create epochs matching Stream Switch configuration"""
        
        if event_ids is None:
            # BCI Competition IV 2a event codes
            event_ids = {
                'left_hand': 7,   # Event ID from annotations (769 in original GDF)
                'right_hand': 8   # Event ID from annotations (770 in original GDF)
            }
        
        print(f"Creating epochs: {tmin} to {tmax} seconds")
        print(f"Event IDs: {event_ids}")
        
        epochs = mne.Epochs(raw, events, event_id=event_ids, 
                           tmin=tmin, tmax=tmax, baseline=None,
                           preload=True, verbose=False)
        
        print(f"  Total epochs: {len(epochs)}")
        for event_name, event_code in event_ids.items():
            count = len(epochs[event_name])
            print(f"    {event_name}: {count}")
        
        return epochs
    
    def extract_features_per_channel(self, signal_data: np.ndarray, n_freq_bands: int = 4) -> np.ndarray:
        """
        Extract features from a single channel matching Feature Aggregator behavior
        This creates multiple frequency band features per channel
        
        Args:
            signal_data: 1D array of signal samples for one channel
            n_freq_bands: Number of frequency bands to extract (simulates Feature Aggregator)
        
        Returns:
            Feature vector for this channel
        """
        # Compute power spectral density
        freqs, psd = sp_signal.welch(signal_data, fs=self.sampling_rate, nperseg=min(256, len(signal_data)))
        
        # Define frequency bands (matching typical motor imagery bands)
        bands = [
            (8, 12),   # Alpha
            (12, 16),  # Low beta
            (16, 24),  # High beta
            (24, 30)   # Low gamma
        ]
        
        features = []
        for fmin, fmax in bands[:n_freq_bands]:
            # Find indices for this frequency band
            idx = np.logical_and(freqs >= fmin, freqs <= fmax)
            # Compute mean power in this band (bandpower)
            # Use log power to make values more manageable and match typical BCI processing
            band_power = np.mean(psd[idx]) if np.any(idx) else 1e-20
            # Convert to log scale (dB) to avoid tiny values
            band_power_db = 10 * np.log10(band_power + 1e-20)  # Add epsilon to avoid log(0)
            features.append(band_power_db)
        
        return np.array(features)
    
    def extract_pipeline_features(self, epochs: mne.Epochs, n_channels: int = 25) -> Tuple[np.ndarray, np.ndarray]:
        """
        Extract 100-dimensional features matching the Designer pipeline:
        - 25 channels (from GDF file)
        - 4 frequency bands per channel
        - Total: 25 * 4 = 100 features
        """
        
        n_epochs = len(epochs)
        n_freq_bands = 4
        n_features = n_channels * n_freq_bands  # Should be 100
        
        print(f"\nExtracting features:")
        print(f"  Channels: {n_channels}")
        print(f"  Frequency bands per channel: {n_freq_bands}")
        print(f"  Total features per epoch: {n_features}")
        
        # Get epoch data: (n_epochs, n_channels, n_times)
        epoch_data = epochs.get_data()
        
        # Ensure we have the right number of channels
        if epoch_data.shape[1] != n_channels:
            print(f"WARNING: Expected {n_channels} channels, got {epoch_data.shape[1]}")
            # Take first n_channels if we have more
            epoch_data = epoch_data[:, :n_channels, :]
        
        features = np.zeros((n_epochs, n_features))
        
        for epoch_idx in range(n_epochs):
            feature_idx = 0
            for ch_idx in range(n_channels):
                # Extract features for this channel
                ch_features = self.extract_features_per_channel(
                    epoch_data[epoch_idx, ch_idx, :],
                    n_freq_bands=n_freq_bands
                )
                
                # DEBUG: Check first channel of first epoch
                if epoch_idx == 0 and ch_idx == 0:
                    print(f"DEBUG: First channel features: {ch_features}")
                    print(f"DEBUG: Signal stats: min={epoch_data[epoch_idx, ch_idx, :].min():.3e}, max={epoch_data[epoch_idx, ch_idx, :].max():.3e}")
                
                # Store in feature vector
                features[epoch_idx, feature_idx:feature_idx+n_freq_bands] = ch_features
                feature_idx += n_freq_bands
        
        # Get labels (convert event IDs to 0/1)
        labels = epochs.events[:, -1]  # Last column is event ID
        unique_labels = np.unique(labels)
        label_map = {unique_labels[0]: 0, unique_labels[1]: 1}
        labels = np.array([label_map[l] for l in labels])
        
        print(f"\nFeature extraction complete:")
        print(f"  Feature shape: {features.shape}")
        print(f"  Labels shape: {labels.shape}")
        print(f"  Label distribution: {np.bincount(labels)}")
        
        return features, labels


def main():
    """Extract features and save for retraining"""
    
    # Configuration
    gdf_file = "BCICIV_2a_gdf/A01T.gdf"
    output_dir = Path("test_data")
    output_dir.mkdir(exist_ok=True)
    
    print("="*70)
    print("OpenViBE Pipeline Feature Extraction")
    print("="*70)
    
    # Extract features
    extractor = OpenViBEFeatureExtractor(gdf_file)
    raw, events = extractor.load_and_preprocess(lowcut=8.0, highcut=30.0)
    epochs = extractor.create_epochs(raw, events, tmin=0.5, tmax=2.5)
    features, labels = extractor.extract_pipeline_features(epochs, n_channels=25)
    
    # Save features and labels
    features_path = output_dir / "pipeline_features_100d.csv"
    labels_path = output_dir / "pipeline_labels.csv"
    
    np.savetxt(features_path, features, delimiter=',', fmt='%.6f')
    np.savetxt(labels_path, labels, delimiter=',', fmt='%d')
    
    print(f"\nSaved features to: {features_path}")
    print(f"Saved labels to: {labels_path}")
    
    # Save feature info
    info = {
        'n_samples': int(features.shape[0]),
        'n_features': int(features.shape[1]),
        'n_channels': 25,
        'n_freq_bands_per_channel': 4,
        'frequency_bands': [
            {'name': 'alpha', 'range': '8-12 Hz'},
            {'name': 'low_beta', 'range': '12-16 Hz'},
            {'name': 'high_beta', 'range': '16-24 Hz'},
            {'name': 'low_gamma', 'range': '24-30 Hz'}
        ],
        'class_0': 'left_hand',
        'class_1': 'right_hand'
    }
    
    info_path = output_dir / "pipeline_features_info.json"
    with open(info_path, 'w') as f:
        json.dump(info, f, indent=2)
    
    print(f"Saved feature info to: {info_path}")
    print("="*70)
    print("\nNext step: Run retrain script to create 100-feature models")


if __name__ == "__main__":
    main()
