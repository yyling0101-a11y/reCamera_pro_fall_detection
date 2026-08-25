#!/usr/bin/env python3
"""Convert static YOLOv8n-Pose ONNX to RV1126B FP16 with RKNN Toolkit 2.3.2."""
from pathlib import Path
from rknn.api import RKNN

ROOT = Path(__file__).resolve().parents[2]
ONNX = ROOT / "yolov8n-pose.onnx"
OUTPUT = ROOT / "recamera_fall_detection/model/yolov8n-pose-fp16-rv1126b.rknn"

if __name__ == "__main__":
    rknn = RKNN(verbose=True)
    try:
        assert rknn.config(
            target_platform="rv1126b",
            mean_values=[[0, 0, 0]],
            std_values=[[255, 255, 255]],
        ) == 0
        assert rknn.load_onnx(model=str(ONNX)) == 0
        assert rknn.build(do_quantization=False) == 0
        OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        assert rknn.export_rknn(str(OUTPUT)) == 0
    finally:
        rknn.release()
