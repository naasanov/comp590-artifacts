#!/bin/bash

###################################################################################################
# Script: run_openvibe_trials.sh
# Purpose: Run OpenViBE Designer scenario multiple times and collect latency statistics
# Usage: ./run_openvibe_trials.sh [num_trials] [scenario_path]
###################################################################################################

# Default configuration
NUM_TRIALS=${1:-100}
SCENARIO_PATH=${2:-"/home/naasanov/.config/OpenVIBE-3.6.0/scenarios/box-tutorials/eegnet_onnx.xml"}
DESIGNER_PATH="./build/bin/openvibe-designer-3.6.0"

echo "=== OpenViBE Multi-Trial Benchmark ==="
echo "Scenario: $SCENARIO_PATH"
echo "Number of trials: $NUM_TRIALS"
echo "======================================="
echo ""

# Array to store latencies
latencies=()

# Run trials
for ((trial=1; trial<=NUM_TRIALS; trial++)); do
    (
        echo "Started trial $trial/$NUM_TRIALS" >&2
        
        # Run OpenViBE with --no-gui to avoid ANSI color codes
        output=$($DESIGNER_PATH --play-fast "$SCENARIO_PATH" --no-gui 2>&1)
        
        # Strip all ANSI escape codes
        output_clean=$(echo "$output" | sed 's/\x1B\[[0-9;]*[JKmsu]//g')
        
        # Extract average latency using awk
        avg_latency=$(echo "$output_clean" | grep "Average latency per buffer:" | awk '{for(i=1;i<=NF;i++) if($i ~ /^[0-9]+\.[0-9]+$/) print $i}' | head -1)
        
        if [ -n "$avg_latency" ]; then
            echo "$avg_latency"
            echo "Completed trial $trial: $avg_latency ms" >&2
        else
            echo "ERROR: Trial $trial failed to parse latency" >&2
        fi
    ) &
    
    # Limit to 2 parallel jobs
    if (( trial % 2 == 0 )) || (( trial == NUM_TRIALS )); then
        wait
    fi
done > /tmp/openvibe_latencies_$$.txt

# Read latencies from file
mapfile -t latencies < /tmp/openvibe_latencies_$$.txt
rm -f /tmp/openvibe_latencies_$$.txt

# Print progress summary
echo "Completed $NUM_TRIALS trials with ${#latencies[@]} successful measurements"

echo ""
echo "=== Processing Results ==="

# Calculate statistics using awk
if [ ${#latencies[@]} -gt 0 ]; then
    # Create a string of latencies for awk
    latency_string="${latencies[*]}"
    
    # Calculate mean, min, max, and std dev using awk
    stats=$(echo "$latency_string" | awk '{
        sum = 0
        sumsq = 0
        min = $1
        max = $1
        
        for (i = 1; i <= NF; i++) {
            sum += $i
            sumsq += $i * $i
            if ($i < min) min = $i
            if ($i > max) max = $i
        }
        
        mean = sum / NF
        variance = (sumsq / NF) - (mean * mean)
        stddev = sqrt(variance)
        
        printf "%.6f %.6f %.6f %.6f", mean, stddev, min, max
    }')
    
    # Parse the statistics
    read mean stddev min max <<< "$stats"
    
    echo ""
    echo "=== Latency Statistics Across $NUM_TRIALS Trials ==="
    echo "Average latency per buffer: $mean ms"
    echo "Standard deviation: $stddev ms"
    echo "Min latency: $min ms"
    echo "Max latency: $max ms"
    echo "====================================================="
else
    echo "ERROR: No valid latencies collected"
    exit 1
fi
