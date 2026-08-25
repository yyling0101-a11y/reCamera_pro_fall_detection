#!/usr/bin/env python3
"""Export the repository YOLOv8n-Pose checkpoint to the static ONNX contract."""
from pathlib import Path
from ultralytics import YOLO

ROOT = Path(__file__).resolve().parents[2]

if __name__ == "__main__":
    YOLO(str(ROOT / "yolov8n-pose.pt")).export(
        format="onnx", imgsz=640, opset=12, simplify=False,
        dynamic=False, device="cpu",
    )
