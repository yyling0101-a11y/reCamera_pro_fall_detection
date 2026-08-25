#!/usr/bin/env python3
"""Convert YOLOv8n-Pose to RV1126B INT8 without losing confidences."""
from pathlib import Path
import tempfile

import numpy as np
import onnx
from onnx import helper, numpy_helper
from rknn.api import RKNN

ROOT = Path(__file__).resolve().parents[2]
ONNX = ROOT / "Real-Time-Person-Elderly-Fall-Detection-System/yolov8n-pose.onnx"
OUTPUT = ROOT / "recamera_fall_detection/model/yolov8n-pose-int8-rv1126b.rknn"
DATASET = ROOT / "recamera_fall_detection/calibration/dataset.txt"
CONFIDENCE_SCALE = 640.0


def make_quantization_safe_onnx(source: Path, output: Path) -> None:
    """Scale sigmoid branches before they join coordinate-valued outputs."""
    model = onnx.load(str(source))
    if len(model.graph.output) != 1 or model.graph.output[0].name != "output0":
        raise RuntimeError("unexpected YOLOv8-Pose output contract")
    replacements = {
        "/model.22/Sigmoid_output_0": "/model.22/Sigmoid_output_0_x640",
        "/model.22/Sigmoid_1_output_0": "/model.22/Sigmoid_1_output_0_x640",
    }
    model.graph.initializer.append(numpy_helper.from_array(
        np.array(CONFIDENCE_SCALE, dtype=np.float32), "confidence_scale_640"))
    for node in model.graph.node:
        if node.op_type == "Concat":
            for index, name in enumerate(node.input):
                if name in replacements:
                    node.input[index] = replacements[name]
    ordered = []
    for node in model.graph.node:
        ordered.append(node)
        for source_name, scaled_name in replacements.items():
            if source_name in node.output:
                ordered.append(helper.make_node(
                    "Mul", [source_name, "confidence_scale_640"], [scaled_name],
                    name="Scale_" + source_name.rsplit("/", 1)[-1]))
    model.graph.ClearField("node")
    model.graph.node.extend(ordered)
    onnx.checker.check_model(model)
    onnx.save(model, str(output))

if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="rknn-pose-int8-") as temp_dir:
        quant_safe_onnx = Path(temp_dir) / "yolov8n-pose-confidence-x640.onnx"
        make_quantization_safe_onnx(ONNX, quant_safe_onnx)
        rknn = RKNN(verbose=True)
        try:
            assert rknn.config(target_platform="rv1126b", mean_values=[[0, 0, 0]],
                               std_values=[[255, 255, 255]]) == 0
            assert rknn.load_onnx(model=str(quant_safe_onnx)) == 0
            assert rknn.build(do_quantization=True, dataset=str(DATASET)) == 0
            OUTPUT.parent.mkdir(parents=True, exist_ok=True)
            assert rknn.export_rknn(str(OUTPUT)) == 0
        finally:
            rknn.release()
