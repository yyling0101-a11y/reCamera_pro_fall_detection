# INT8 calibration dataset

- Dataset: Ultralytics COCO128, the first 128 images of COCO train2017.
- Source: https://github.com/ultralytics/assets/releases/download/v0.0.0/coco128.zip
- Documentation: https://docs.ultralytics.com/datasets/detect/coco128
- Downloaded archive SHA-256: `61e5e3028863d8ffc3b81d6a514603954889f0edd5e4b44c4ce60b2da99aeb8e`.
- Image list: `dataset.txt`, paths relative to this directory as required by RKNN Toolkit2.

The images are used only to determine INT8 activation ranges. COCO images retain
the licenses specified by their individual Flickr sources; COCO annotations are
CC BY 4.0. The archive is not needed on the target device.
