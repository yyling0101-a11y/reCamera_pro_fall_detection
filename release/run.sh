#!/bin/sh
set -eu

base_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec "$base_dir/recamera_fall_detection" \
  --model "$base_dir/yolov8n-pose-int8-rv1126b.rknn" \
  --alarm-sound "$base_dir/warning.wav" \
  "$@"
