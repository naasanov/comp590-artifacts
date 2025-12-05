#!/usr/bin/env python3
"""
Convert Lee2019_MI Motor Imagery dataset to OpenViBE-compatible CSV format.

This script:
1. Downloads Lee2019_MI data using MOABB
2. Extracts epochs for a specified subject
3. Exports to OpenViBE CSV format with proper timestamps and stimulations

Usage:
    python convert_lee2019_to_openvibe_csv.py --subject 1 --output subject_01.csv
"""

import numpy as np
import argparse
from pathlib import Path

try:
    from moabb.datasets import Lee2019_MI
    from moabb.paradigms import LeftRightImagery
except ImportError:
    print("ERROR: MOABB not installed. Install with: pip install moabb")
    exit(1)


def create_openvibe_csv(data, labels, channel_names, sampling_rate, output_path):
    """
    Create OpenViBE-compatible CSV file from epoched data.
    
    Args:
        data: numpy array of shape (n_epochs, n_channels, n_samples)
        labels: numpy array of shape (n_epochs,) with class labels (1 or 2)
        channel_names: list of channel names
        sampling_rate: sampling rate in Hz
        output_path: path to output CSV file
    """
    n_epochs, n_channels, n_samples = data.shape
    
    # OpenViBE CSV format requires:
    # - First row: header with Time:SamplingRate, Epoch, Channel names, Event Id, Event Date, Event Duration
    # - Data rows: timestamp, epoch_number, channel_values..., event_id (optional), event_date (optional), event_duration (optional)
    
    print(f"Creating OpenViBE CSV with:")
    print(f"  - {n_epochs} epochs")
    print(f"  - {n_channels} channels")
    print(f"  - {n_samples} samples per epoch")
    print(f"  - Sampling rate: {sampling_rate} Hz")
    
    with open(output_path, 'w') as f:
        # Write header
        header_parts = [f"Time:{int(sampling_rate)}Hz", "Epoch"]
        header_parts.extend(channel_names)
        header_parts.extend(["Event Id", "Event Date", "Event Duration"])
        f.write(','.join(header_parts) + '\n')
        
        # Write data
        # We'll write continuous data with epoch markers
        time = 0.0
        time_step = 1.0 / sampling_rate
        
        for epoch_idx in range(n_epochs):
            for sample_idx in range(n_samples):
                # Build row
                row = [f"{time:.5f}", str(epoch_idx)]
                
                # Add channel values
                for ch_idx in range(n_channels):
                    row.append(f"{data[epoch_idx, ch_idx, sample_idx]:.6f}")
                
                # Add stimulation at the first sample of each epoch
                if sample_idx == 0:
                    # OpenViBE stimulation IDs: left=1, right=2
                    # Lee2019 labels: left_hand=2, right_hand=1 (we need to map these)
                    # Actually, let's use standard OVTK stimulation codes
                    # OVTK_StimulationId_Label_00 = 0x00008100 = 33024
                    # OVTK_StimulationId_Label_01 = 0x00008101 = 33025
                    # We'll use 33024 for left (label=2) and 33025 for right (label=1)
                    if labels[epoch_idx] == 2:  # left_hand
                        event_id = "33024"
                    else:  # right_hand (label=1)
                        event_id = "33025"
                    event_date = f"{time:.5f}"
                    event_duration = "0"
                    row.extend([event_id, event_date, event_duration])
                else:
                    # No event for other samples
                    row.extend(["", "", ""])
                
                f.write(','.join(row) + '\n')
                time += time_step
    
    print(f"Successfully wrote CSV to {output_path}")
    print(f"Total duration: {time:.2f} seconds")


def main():
    parser = argparse.ArgumentParser(
        description='Convert Lee2019_MI data to OpenViBE CSV format'
    )
    parser.add_argument(
        '--subject',
        type=int,
        required=True,
        help='Subject number (1-54)'
    )
    parser.add_argument(
        '--session',
        type=int,
        default=1,
        choices=[1, 2],
        help='Session number (1 or 2, default: 1)'
    )
    parser.add_argument(
        '--output',
        type=str,
        default=None,
        help='Output CSV file path (default: lee2019_s{subject}_sess{session}.csv)'
    )
    parser.add_argument(
        '--downsample',
        type=int,
        default=250,
        help='Target sampling rate in Hz (default: 250, original is 1000)'
    )
    parser.add_argument(
        '--epoch-samples',
        type=int,
        default=None,
        help='Number of samples per epoch (default: None = use full epoch). Set to 385 for EEGNetv4_Lee2019_MI model'
    )
    
    args = parser.parse_args()
    
    # Set default output path if not specified
    if args.output is None:
        args.output = f"lee2019_s{args.subject:02d}_sess{args.session}.csv"
    
    print("="*60)
    print("Lee2019_MI to OpenViBE CSV Converter")
    print("="*60)
    print(f"Subject: {args.subject}")
    print(f"Session: {args.session}")
    print(f"Output: {args.output}")
    print(f"Target sampling rate: {args.downsample} Hz")
    print("="*60)
    
    # Load dataset
    print("\nLoading Lee2019_MI dataset...")
    print("(This may take a while on first run as data is downloaded)")
    
    # MOABB uses 0-indexed sessions internally
    moabb_session = args.session - 1
    dataset = Lee2019_MI(train_run=True, test_run=False, sessions=(args.session,))
    
    # Get data for the specified subject
    print(f"\nExtracting data for subject {args.subject}...")
    
    # MOABB uses 1-based subject indexing
    subject_id = args.subject
    
    # Get the raw data
    subject_data = dataset.get_data([subject_id])
    
    if subject_id not in subject_data:
        print(f"ERROR: Subject {subject_id} not found in dataset")
        print(f"Available subjects: {list(subject_data.keys())}")
        return
    
    # Get the sessions
    sessions = subject_data[subject_id]
    session_key = str(moabb_session)  # Use 0-indexed session
    
    if session_key not in sessions:
        print(f"ERROR: Session {args.session} (key '{session_key}') not found")
        print(f"Available sessions: {list(sessions.keys())}")
        return
    
    session_data = sessions[session_key]
    
    # Handle case where session contains multiple runs
    if isinstance(session_data, dict):
        print(f"Session contains runs: {list(session_data.keys())}")
        run_key = list(session_data.keys())[0]
        print(f"Using run: '{run_key}'")
        raw = session_data[run_key]
    else:
        raw = session_data
    
    print(f"Raw data info:")
    print(f"  - Channels: {len(raw.ch_names)}")
    print(f"  - Sampling rate: {raw.info['sfreq']} Hz")
    print(f"  - Duration: {raw.times[-1]:.2f} seconds")
    
    # Resample if needed
    if args.downsample != raw.info['sfreq']:
        print(f"\nResampling from {raw.info['sfreq']} Hz to {args.downsample} Hz...")
        raw.resample(args.downsample)
    
    # Extract epochs
    print("\nExtracting epochs...")
    
    # Get events from raw
    from mne import Epochs, events_from_annotations
    
    events_array, event_dict = events_from_annotations(raw)
    
    print(f"Event types found: {event_dict}")
    print(f"Total events: {len(events_array)}")
    
    # For Lee2019_MI, use 0-4s after cue onset
    tmin, tmax = 0.0, 4.0
    
    # Create epochs
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
    
    # Filter to only keep motor imagery events (left_hand=2, right_hand=1)
    # The actual event IDs in the raw data might be different, let's check
    mi_event_ids = {}
    for key, value in event_dict.items():
        if 'left' in key.lower() or 'right' in key.lower() or key in ['1', '2']:
            mi_event_ids[key] = value
    
    if not mi_event_ids:
        print("WARNING: No motor imagery events found, using all events")
        mi_event_ids = event_dict
    
    print(f"Using events: {mi_event_ids}")
    
    # Get data and labels
    data = epochs.get_data()  # shape: (n_epochs, n_channels, n_samples)
    labels = epochs.events[:, -1]  # event IDs
    
    # Select only first 62 EEG channels (Lee2019_MI has extra non-EEG channels)
    if data.shape[1] > 62:
        print(f"\nSelecting first 62 EEG channels (data has {data.shape[1]} channels)...")
        data = data[:, :62, :]
        channel_names = epochs.ch_names[:62]
    else:
        channel_names = epochs.ch_names
    
    # Crop to specified number of samples if requested
    if args.epoch_samples is not None:
        if args.epoch_samples > data.shape[2]:
            print(f"\nWARNING: Requested {args.epoch_samples} samples but epochs only have {data.shape[2]}")
            print(f"Cannot pad, keeping original {data.shape[2]} samples")
        else:
            print(f"\nCropping epochs from {data.shape[2]} to {args.epoch_samples} samples...")
            data = data[:, :, :args.epoch_samples]
    
    print(f"\nEpochs extracted:")
    print(f"  - Shape: {data.shape}")
    print(f"  - Labels: {np.unique(labels)} (counts: {np.bincount(labels)})")
    
    # Get channel names (exclude non-EEG channels if any)
    # Already set above when selecting channels
    
    # Convert to microvolts (MNE uses volts by default)
    # Actually, let's check the units first
    if epochs.info['chs'][0]['unit'] == 107:  # FIFF_UNIT_V (volts)
        print("\nConverting from Volts to microvolts...")
        data = data * 1e6  # Convert to microvolts
    
    # Create OpenViBE CSV
    print("\nWriting OpenViBE CSV file...")
    create_openvibe_csv(
        data=data,
        labels=labels,
        channel_names=channel_names,
        sampling_rate=args.downsample,
        output_path=args.output
    )
    
    print("\n" + "="*60)
    print("Conversion complete!")
    print("="*60)
    print(f"\nYou can now use this file in OpenViBE with the CSV File Reader box.")
    print(f"The file contains {len(data)} epochs of motor imagery data.")
    print(f"\nStimulation IDs:")
    print(f"  - 33024 (0x8100): Left hand")
    print(f"  - 33025 (0x8101): Right hand")


if __name__ == '__main__':
    main()