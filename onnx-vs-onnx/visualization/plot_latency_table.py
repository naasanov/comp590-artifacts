#!/usr/bin/env python3
"""
Script to create a matplotlib table comparing standalone vs OpenViBE latency statistics
"""

import matplotlib.pyplot as plt
import matplotlib.patches as mpatches

# Data from benchmarks (raw values)
standalone_raw = {
    'Average latency': 0.982131,
    'Min latency': 0.928626,
    'Max latency': 1.11231,
    'Std deviation': 0.0349307,
}

openvibe_raw = {
    'Average latency': 1.055144,
    'Min latency': 1.012170,
    'Max latency': 1.121150,
    'Std deviation': 0.024499,
}

# Calculate percentage differences (OpenViBE vs Standalone)
overhead = {}
for key in standalone_raw.keys():
    diff_pct = ((openvibe_raw[key] - standalone_raw[key]) / standalone_raw[key]) * 100
    overhead[key] = f'+{diff_pct:.2f}%' if diff_pct >= 0 else f'{diff_pct:.2f}%'

# Format data for table
standalone_data = {k: f'{v:.6f} ms' for k, v in standalone_raw.items()}
standalone_data.update({'Trials': '100', 'Epochs per trial': '100', 'Total epochs': '10000'})

openvibe_data = {k: f'{v:.6f} ms' for k, v in openvibe_raw.items()}
openvibe_data.update({'Trials': '100', 'Epochs per trial': '—', 'Total epochs': '—'})

overhead.update({'Trials': '—', 'Epochs per trial': '—', 'Total epochs': '—'})

# Create figure
fig, ax = plt.subplots(figsize=(10, 6))
ax.axis('tight')
ax.axis('off')

# Prepare table data
metrics = ['Average latency', 'Min latency', 'Max latency', 'Std deviation', 
           'Trials', 'Epochs per trial', 'Total epochs']
table_data = []
for metric in metrics:
    table_data.append([metric, standalone_data[metric], openvibe_data[metric], overhead[metric]])

# Create table
table = ax.table(cellText=table_data,
                colLabels=['Metric', 'Standalone C++', 'OpenViBE', 'Overhead'],
                cellLoc='center',
                loc='center',
                colWidths=[0.3, 0.22, 0.22, 0.15])

# Style the table
table.auto_set_font_size(False)
table.set_fontsize(10)
table.scale(1, 2.5)

# Color header
for i in range(4):
    table[(0, i)].set_facecolor('#4CAF50')
    table[(0, i)].set_text_props(weight='bold', color='white')

# Alternate row colors
for i in range(1, len(metrics) + 1):
    for j in range(4):
        if i % 2 == 0:
            table[(i, j)].set_facecolor('#f0f0f0')
        else:
            table[(i, j)].set_facecolor('white')

# Bold the metric column
for i in range(1, len(metrics) + 1):
    table[(i, 0)].set_text_props(weight='bold')

# Color the overhead column based on metric type
for i in range(1, 5):  # First 4 metrics are latency-related
    if overhead[metrics[i-1]] != '—':
        table[(i, 3)].set_facecolor('#ffcccc')  # Light red for overhead

plt.title('ONNX Inference Latency Comparison\n100 Trials Each', 
          fontsize=14, weight='bold', pad=20)

plt.tight_layout()
plt.savefig('latency_comparison_table.png', dpi=300, bbox_inches='tight')
print("Table saved as 'latency_comparison_table.png'")
plt.show()
