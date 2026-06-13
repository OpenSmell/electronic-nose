#!/usr/bin/env python3
"""
OpenSmell Electronic Nose — Real‑Time Sensor Dashboard
Usage:
    python visualizer.py <csv_path>
"""
import argparse
import glob
import os
import sys
from pathlib import Path

import matplotlib
matplotlib.use("TkAgg")
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.gridspec import GridSpec
import numpy as np
import pandas as pd

plt.style.use("dark_background")

SENSOR_COLORS = ["cyan", "magenta", "lime", "orange"]

REAL_DIMENSIONS = {
    "Base properties": (0, 12, "#00ffff"),
    "Topological indices": (12, 15, "#ff00ff"),
    "Functional groups": (15, 29, "#adff2f"),
}


def find_smellnet_csv_random():
    cache = Path.home() / ".cache" / "huggingface" / "hub"
    pattern = str(
        cache / "datasets--DeweiFeng--smell-net" / "snapshots" / "*" / "base_data" / "**" / "*.csv"
    )
    files = sorted(glob.glob(pattern, recursive=True))
    if not files:
        return None
    idx = np.random.randint(0, len(files))
    chosen = files[idx]
    substance = Path(chosen).parent.name
    print(f"No CSV given. Randomly selected: {substance}/{Path(chosen).name}")
    return chosen


def detect_sensor_columns(df):
    expected = ["no2", "c2h5oh", "voc", "co"]
    found = []
    for col in df.columns:
        if col.lower() in expected:
            found.append(col)
    if len(found) >= 4:
        return found[:4]
    fallback = [c for c in df.columns if c.lower().startswith("mq")]
    if len(fallback) >= 4:
        return fallback[:4]
    fallback2 = [c for c in df.columns if c.lower().startswith("sensor_")]
    if len(fallback2) >= 4:
        return fallback2[:4]
    return df.columns[:4].tolist()


def get_chemoprint(filepath):
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "opensmell"))
    import opensmell
    result = opensmell.process(filepath)
    return result.chemoprint, result.substance, result.confidence


class Dashboard:
    def __init__(self, filepath):
        self.filepath = filepath
        self.df = pd.read_csv(filepath)
        self.sensor_cols = detect_sensor_columns(self.df)
        self.data = self.df[self.sensor_cols].values.astype(np.float32)
        self.n_timesteps = self.data.shape[0]

        self.fig = plt.figure(figsize=(16, 10), facecolor="#0a0a0a")
        gs = GridSpec(3, 4, figure=self.fig, hspace=0.35, wspace=0.3)

        self.axes_lines = []
        for i in range(4):
            ax = self.fig.add_subplot(gs[0, i])
            ax.set_facecolor("#111111")
            ax.set_title(self.sensor_cols[i], color=SENSOR_COLORS[i], fontsize=11, fontweight="bold")
            ax.set_xlabel("Time step", color="gray", fontsize=8)
            ax.set_ylabel("Raw value", color="gray", fontsize=8)
            ax.tick_params(colors="gray", labelsize=7)
            ax.grid(True, alpha=0.15, color="white")
            (line,) = ax.plot([], [], color=SENSOR_COLORS[i], linewidth=1.5, alpha=0.9)
            self.axes_lines.append((ax, line))

        self.ax_chemo = self.fig.add_subplot(gs[1, :])
        self.ax_chemo.set_facecolor("#111111")
        self.ax_chemo.set_title("Chemoprint (29-dim)", color="white", fontsize=12, fontweight="bold")
        self.ax_chemo.tick_params(colors="gray", labelsize=7)
        self.ax_chemo.grid(True, alpha=0.1, color="white", axis="x")

        self.ax_status = self.fig.add_subplot(gs[2, :])
        self.ax_status.set_facecolor("#111111")
        self.ax_status.set_xlim(0, 1)
        self.ax_status.set_ylim(0, 1)
        self.ax_status.axis("off")
        self.status_text = self.ax_status.text(
            0.5, 0.6, "", ha="center", va="center", fontsize=18, fontweight="bold", color="lime"
        )
        self.confidence_text = self.ax_status.text(
            0.5, 0.3, "", ha="center", va="center", fontsize=14, color="cyan"
        )

        self._prediction_done = False
        self._chemo = None
        self._substance = ""
        self._confidence = 0.0

        self.anim = animation.FuncAnimation(
            self.fig, self.update, interval=50, frames=self.n_timesteps, repeat=True, cache_frame_data=False
        )

    def update(self, frame):
        window = min(frame + 1, self.n_timesteps)
        for i, (ax, line) in enumerate(self.axes_lines):
            line.set_data(range(window), self.data[:window, i])
            ax.set_xlim(0, max(window, 10))
            ymin = self.data[:window, i].min() - 10
            ymax = self.data[:window, i].max() + 10
            ax.set_ylim(ymin, max(ymax, ymin + 1))

        if frame == self.n_timesteps - 1 and not self._prediction_done:
            self._prediction_done = True
            self._chemo, self._substance, self._confidence = get_chemoprint(self.filepath)
            self._draw_chemoprint()
            self.status_text.set_text(f"Prediction: {self._substance}")
            self.confidence_text.set_text(f"Confidence: {self._confidence:.4f}")

        return [l for _, l in self.axes_lines] + [self.status_text, self.confidence_text]

    def _draw_chemoprint(self):
        if self._chemo is None:
            return
        self.ax_chemo.clear()
        self.ax_chemo.set_facecolor("#111111")
        self.ax_chemo.set_title("Chemoprint (29-dim)", color="white", fontsize=12, fontweight="bold")
        self.ax_chemo.tick_params(colors="gray", labelsize=7)
        self.ax_chemo.grid(True, alpha=0.1, color="white", axis="x")

        y_pos = np.arange(29)
        group_colors = []
        for i in range(29):
            for _, (start, end, color) in REAL_DIMENSIONS.items():
                if start <= i < end:
                    group_colors.append(color)
                    break

        self.ax_chemo.barh(y_pos, self._chemo, color=group_colors, height=0.7, edgecolor="white", linewidth=0.3)
        self.ax_chemo.set_yticks(y_pos)
        self.ax_chemo.set_yticklabels([str(i) for i in range(29)], fontsize=6, color="gray")
        self.ax_chemo.axvline(0, color="white", linewidth=0.5, alpha=0.3)

        legend_elements = [
            plt.Rectangle((0, 0), 1, 1, facecolor=c, edgecolor="none") for c in
            ["#00ffff", "#ff00ff", "#adff2f"]
        ]
        self.ax_chemo.legend(
            legend_elements, list(REAL_DIMENSIONS.keys()),
            loc="lower right", fontsize=7, facecolor="#222222", edgecolor="gray", labelcolor="white"
        )

    def show(self):
        plt.tight_layout()
        plt.show()


def main():
    parser = argparse.ArgumentParser(description="OpenSmell E-Nose Dashboard")
    parser.add_argument("csv", nargs="?", default=None, help="Path to sensor CSV file")
    parser.add_argument("--live", action="store_true", help="Serial mode (not yet implemented)")
    args = parser.parse_args()

    if args.live:
        print("ERROR: --live requires serial hardware. Use a CSV file instead.")
        sys.exit(1)

    csv_path = args.csv
    if csv_path is None:
        csv_path = find_smellnet_csv_random()
        if csv_path is None:
            print("No CSV file specified and no SmellNet cache found.", file=sys.stderr)
            sys.exit(1)

    dash = Dashboard(csv_path)
    dash.show()


if __name__ == "__main__":
    main()
