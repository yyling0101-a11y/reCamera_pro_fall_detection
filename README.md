# reCamera Pro Fall Detection

[English](#english) | [中文](#中文)

为 Seeed reCamera Pro（RV1126B / aarch64）提供的原生跌倒检测程序。它在设备端运行 YOLOv8n-Pose RKNN 模型，跟踪人体姿态并在确认跌倒后通过 WebSocket 推送 JSON 告警、提供带骨架和状态标识的 RTSP 预览流，并可播放设备端提示音。

## 快速开始（无需编译）

从 [Releases](https://github.com/yyling0101-a11y/reCamera_pro_fall_detection/releases/latest) 下载 `recamera-fall-detection-rv1126b.tar.gz`，解压后把目录复制到 reCamera Pro：

```sh
tar -xzf recamera-fall-detection-rv1126b.tar.gz
scp -r recamera-fall-detection root@<RECAMERA_IP>:/userdata/
ssh root@<RECAMERA_IP>
cd /userdata/recamera-fall-detection
./run.sh
```

该包包含 aarch64 可执行文件、已量化的 RV1126B INT8 RKNN 模型、提示音、启动脚本及 `SHA256SUMS`。它不是 x86_64 Linux 或第一代 reCamera（SG2002/RISC-V）的程序。

默认命令使用 `/dev/video13`、WebSocket `9000` 端口与 RTSP `8554` 端口。不同固件的摄像头节点可能不同，可先查看 `v4l2-ctl --list-devices`，再以 `--device /dev/videoN` 替换。生产 reCamera Pro 固件应已经提供兼容的 RKNN Runtime 和 GStreamer RTSP 库；本项目不会覆盖板端运行时库。

观看预览流：

```sh
ffplay -rtsp_transport tcp rtsp://<RECAMERA_IP>:8554/fall
# 或 vlc rtsp://<RECAMERA_IP>:8554/fall
```

WebSocket 客户端连接：`ws://<RECAMERA_IP>:9000/alerts`。

## 使用方法

```sh
./recamera_fall_detection \
  --model yolov8n-pose-int8-rv1126b.rknn \
  --ws-bind 0.0.0.0 --ws-port 9000 \
  --rtsp-port 8554 --rtsp-fps 10
```

| 选项 | 作用 |
| --- | --- |
| `--device /dev/video13` | 摄像头 V4L2 设备 |
| `--confidence 0.35` / `--nms 0.45` | 检测与 NMS 阈值 |
| `--no-rtsp` | 不启动 RTSP 服务 |
| `--alarm-test` | 仅测试告警队列与扬声器 |
| `--websocket-test` | 仅测试 WebSocket 服务，无需模型和摄像头 |
| `--max-frames N` | 运行 N 帧后退出，便于排查 |

告警载荷示例：

```json
{"type":"fall_alert","severity":"critical","track_id":1,"state":"CONFIRMED_FALL","timestamp_ms":0,"torso_angle":24.1,"vertical_velocity":0.7}
```

## 已知要求与限制

- 目标平台固定为 reCamera Pro RV1126B（aarch64），并需要与模型匹配的 RKNN 2.3.2 运行时。
- 输入为 640×640 NV12 摄像头画面；模型、输入输出和量化细节见 [MODEL_CONTRACT.md](MODEL_CONTRACT.md)。
- 这是一项辅助检测功能，不能作为医疗、紧急救援或人身安全系统的唯一依据。部署前请在实际场景中验证阈值、光照、遮挡和网络告警链路。
- 模型源自 Ultralytics YOLOv8n-Pose；其来源、校准和再分发注意事项见模型契约。使用或再分发前请自行确认适用许可证。

## 从源码构建

需要与设备 ABI 匹配的 aarch64 工具链、sysroot、GStreamer 1.22 开发文件、RKNN Runtime 2.3.2 和 `rknn_api.h`。不要使用 WSL 主机的 x86_64 库来链接。模型转换需在 x86_64 环境使用 RKNN-Toolkit2 2.3.2，目标为 `rv1126b`。现有转换参数保存在模型旁的 `.rknn.json` 文件中。

```sh
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=<toolchain-file> \
  -DRECAMERA_RKNNRT=<aarch64-librknnrt.so>
cmake --build build
```

## 项目结构

- `src/`：采集、RKNN 推理、姿态/跌倒状态机、RTSP、WebSocket 和叠加绘制
- `include/`：公共头文件与 RKNN C API 头文件
- `tools/`：ONNX 导出与 FP16/INT8 RKNN 转换脚本
- `MODEL_CONTRACT.md`：模型输入、输出、量化与来源记录
- `calibration/README.md`：INT8 校准数据说明（原始校准数据不随仓库发布）

## English

Native fall detection for **Seeed reCamera Pro (RV1126B, aarch64)**. It runs an INT8 YOLOv8n-Pose RKNN model on-device, tracks people, emits WebSocket alerts, publishes an annotated RTSP stream, and can play an audible alert.

For a no-build install, download `recamera-fall-detection-rv1126b.tar.gz` from [Releases](https://github.com/yyling0101-a11y/reCamera_pro_fall_detection/releases/latest), copy it to `/userdata/` on the device, then run `./run.sh`. The bundle contains the aarch64 executable, the RV1126B INT8 model, an alert sound, launcher, and checksums. It requires compatible RKNN Runtime 2.3.2 and GStreamer RTSP libraries already on a production reCamera Pro; it does not replace board runtime libraries.

The default endpoints are `rtsp://<DEVICE_IP>:8554/fall` and `ws://<DEVICE_IP>:9000/alerts`. See the Chinese quick-start section above for commands, options, model details, build requirements, and safety limitations.
