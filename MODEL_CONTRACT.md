# Model contract

- Source: repository `yolov8n-pose.pt`, Ultralytics YOLOv8n-Pose, AGPL-3.0.
- Source SHA-256: `c6fa93dd1ee4a2c18c900a45c1d864a1c6f7aba75d84f91648a30b7fb641d212`.
- Exporter: Ultralytics 8.4.121, ONNX opset 12, static input.
- ONNX input: `images`, float32 NCHW `[1,3,640,640]`, RGB, range 0..1.
- RKNN input: uint8 NHWC `[1,640,640,3]`; converter mean `[0,0,0]`, std `[255,255,255]`.
- Capture: the reCamera ISP directly supplies 640x640 NV12; GStreamer converts it
  to RGB without a CPU `videoscale` stage.
- Production RKNN target: `rv1126b`, Toolkit 2.3.2, calibrated INT8 using the
  128-image COCO128 list in `calibration/dataset.txt`.
- The original FP16 model remains available as a correctness baseline.
- Output: float `[1,56,8400]`; channels are xywh, person confidence, then 17 COCO keypoints as x/y/confidence.
- The INT8 graph scales person and keypoint confidence channels by 640 before
  the final concat. Runtime decoding divides those channels by 640. This avoids
  mixing `[0,1]` probabilities and pixel coordinates under one coarse INT8
  output scale.
- Postprocessing: confidence filter, minimum four visible joints, class-agnostic IoU NMS.

The `.rknn.json` files beside the models record the exact conversion arguments.
