import matplotlib.pyplot as plt
import matplotlib.font_manager as fm
import numpy as np

plt.rcParams['font.sans-serif'] = [
    'Source Han Sans CN',
    'Noto Sans CJK SC',
    'WenQuanYi Micro Hei',
    'SimHei',
    'DejaVu Sans'
]

plt.rcParams['axes.unicode_minus'] = False

data = """FPS: 63.829, CPU(s): 0.073472, CPU/FPS Ratio: 0.00115107, Memory(MB): 82.3711, Avg Render Time (ms): 0.301441
FPS: 48.6637, CPU(s): 0.116523, CPU/FPS Ratio: 0.00239445, Memory(MB): 86.5078, Avg Render Time (ms): 0.238361
FPS: 54.4138, CPU(s): 0.147355, CPU/FPS Ratio: 0.00270805, Memory(MB): 86.9023, Avg Render Time (ms): 0.232899
FPS: 54.056, CPU(s): 0.181795, CPU/FPS Ratio: 0.00336309, Memory(MB): 86.9023, Avg Render Time (ms): 0.219961
FPS: 57.3488, CPU(s): 0.223914, CPU/FPS Ratio: 0.00390442, Memory(MB): 86.9023, Avg Render Time (ms): 0.215918
FPS: 52.9785, CPU(s): 0.268827, CPU/FPS Ratio: 0.00507427, Memory(MB): 86.9023, Avg Render Time (ms): 0.222304
FPS: 51.5268, CPU(s): 0.313388, CPU/FPS Ratio: 0.00608204, Memory(MB): 86.9023, Avg Render Time (ms): 0.230559
FPS: 51.7445, CPU(s): 0.353033, CPU/FPS Ratio: 0.00682262, Memory(MB): 86.9023, Avg Render Time (ms): 0.231117
FPS: 54.5303, CPU(s): 0.384111, CPU/FPS Ratio: 0.007044, Memory(MB): 86.9023, Avg Render Time (ms): 0.234071
FPS: 55.5936, CPU(s): 0.421318, CPU/FPS Ratio: 0.00757853, Memory(MB): 86.9023, Avg Render Time (ms): 0.234814
FPS: 55.6894, CPU(s): 0.473264, CPU/FPS Ratio: 0.00849828, Memory(MB): 86.9023, Avg Render Time (ms): 0.231909
FPS: 56.7207, CPU(s): 0.519639, CPU/FPS Ratio: 0.00916136, Memory(MB): 86.9023, Avg Render Time (ms): 0.22876
FPS: 57.0355, CPU(s): 0.565628, CPU/FPS Ratio: 0.00991713, Memory(MB): 86.9023, Avg Render Time (ms): 0.228201
FPS: 57.994, CPU(s): 0.617992, CPU/FPS Ratio: 0.0106561, Memory(MB): 86.9023, Avg Render Time (ms): 0.227523
FPS: 58.4275, CPU(s): 0.712113, CPU/FPS Ratio: 0.012188, Memory(MB): 86.9023, Avg Render Time (ms): 0.233687
FPS: 59.4478, CPU(s): 0.755387, CPU/FPS Ratio: 0.0127067, Memory(MB): 86.9023, Avg Render Time (ms): 0.23239
FPS: 60.0757, CPU(s): 0.802151, CPU/FPS Ratio: 0.0133523, Memory(MB): 86.9023, Avg Render Time (ms): 0.230525
FPS: 60.0732, CPU(s): 0.860792, CPU/FPS Ratio: 0.014329, Memory(MB): 86.9023, Avg Render Time (ms): 0.226903
FPS: 60.4611, CPU(s): 0.929493, CPU/FPS Ratio: 0.0153734, Memory(MB): 86.9023, Avg Render Time (ms): 0.228768
FPS: 59.312, CPU(s): 0.965708, CPU/FPS Ratio: 0.0162818, Memory(MB): 86.9023, Avg Render Time (ms): 0.227967
FPS: 60.6607, CPU(s): 1.00679, CPU/FPS Ratio: 0.0165971, Memory(MB): 86.9023, Avg Render Time (ms): 0.227587
FPS: 59.7579, CPU(s): 1.02954, CPU/FPS Ratio: 0.0172285, Memory(MB): 86.9023, Avg Render Time (ms): 0.224702
FPS: 60.3235, CPU(s): 1.05697, CPU/FPS Ratio: 0.0175217, Memory(MB): 86.9023, Avg Render Time (ms): 0.223682
FPS: 60.5631, CPU(s): 1.0879, CPU/FPS Ratio: 0.0179632, Memory(MB): 87.2617, Avg Render Time (ms): 0.222102"""

fps_values = []
cpu_values = []
cpu_fps_ratio = []
memory_values = []
render_time_values = []

for line in data.strip().split('\n'):
    parts = line.split(', ')
    fps = float(parts[0].split(': ')[1])
    cpu = float(parts[1].split(': ')[1])
    ratio = float(parts[2].split(': ')[1])
    memory = float(parts[3].split(': ')[1])
    render_time = float(parts[4].split(': ')[1])
    
    fps_values.append(fps)
    cpu_values.append(cpu)
    cpu_fps_ratio.append(ratio)
    memory_values.append(memory)
    render_time_values.append(render_time)

time_points = list(range(len(fps_values)))

fig, axes = plt.subplots(2, 3, figsize=(18, 12))
fig.suptitle('Wayland Compositor Performance Data', fontsize=16, fontweight='bold')

axes[0, 0].plot(time_points, fps_values, 'b-', linewidth=2, marker='o', markersize=4)
axes[0, 0].set_title('FPS')
axes[0, 0].set_xlabel('timestamp')
axes[0, 0].set_ylabel('FPS')
axes[0, 0].grid(True, alpha=0.3)
axes[0, 0].set_ylim(40, 70)

axes[0, 1].plot(time_points, cpu_values, 'r-', linewidth=2, marker='s', markersize=4)
axes[0, 1].set_title('CPU Time')
axes[0, 1].set_xlabel('timestamp')
axes[0, 1].set_ylabel('CPU Time (s)')
axes[0, 1].grid(True, alpha=0.3)

axes[0, 2].plot(time_points, cpu_fps_ratio, 'g-', linewidth=2, marker='^', markersize=4)
axes[0, 2].set_title('CPU/FPS Ratio')
axes[0, 2].set_xlabel('timestamp')
axes[0, 2].set_ylabel('CPU/FPS Ratio')
axes[0, 2].grid(True, alpha=0.3)

axes[1, 0].plot(time_points, memory_values, 'm-', linewidth=2, marker='d', markersize=4)
axes[1, 0].set_title('Memory Usage')
axes[1, 0].set_xlabel('timestamp')
axes[1, 0].set_ylabel('Memory (MB)')
axes[1, 0].grid(True, alpha=0.3)
axes[1, 0].set_ylim(80, 90)

axes[1, 1].plot(time_points, render_time_values, 'c-', linewidth=2, marker='p', markersize=4)
axes[1, 1].set_title('Average Render Time')
axes[1, 1].set_xlabel('timestamp')
axes[1, 1].set_ylabel('Render Time (ms)')
axes[1, 1].grid(True, alpha=0.3)

axes[1, 2].scatter(cpu_values, fps_values, c=time_points, cmap='viridis', alpha=0.7, s=50)
axes[1, 2].set_title('FPS vs CPU')
axes[1, 2].set_xlabel('CPU Time (s)')
axes[1, 2].set_ylabel('FPS')
axes[1, 2].grid(True, alpha=0.3)

cbar = plt.colorbar(axes[1, 2].collections[0], ax=axes[1, 2])
cbar.set_label('timestamp')

plt.tight_layout()
plt.savefig("test_report_graph.png")
plt.show()

print("=== Statistics ===")
print(f"FPS: Average={np.mean(fps_values):.2f}, Min={np.min(fps_values):.2f}, Max={np.max(fps_values):.2f}")
print(f"CPU: Average={np.mean(cpu_values):.3f}s, Min={np.min(cpu_values):.3f}s, Max={np.max(cpu_values):.3f}s")
print(f"Memory: Average={np.mean(memory_values):.2f}MB, Min={np.min(memory_values):.2f}MB, Max={np.max(memory_values):.2f}MB")
print(f"Render Time: Average={np.mean(render_time_values):.3f}ms, Min={np.min(render_time_values):.3f}ms, Max={np.max(render_time_values):.3f}ms")