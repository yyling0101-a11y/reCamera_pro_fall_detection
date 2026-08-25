# reCamera Pro fall-detection bundle

This bundle is for a reCamera Pro with an RV1126B SoC and aarch64 firmware. It is not compatible with x86_64 Linux or RISC-V reCamera hardware.

Copy the extracted `recamera-fall-detection` directory to `/userdata/` on the device and start it there:

```sh
cd /userdata/recamera-fall-detection
./run.sh
```

The launcher uses `/dev/video13`. If the device uses another V4L2 camera node, pass it through, for example `./run.sh --device /dev/video0`.

The RTSP preview is at `rtsp://<device-ip>:8554/fall`; WebSocket alerts are at `ws://<device-ip>:9000/alerts`.

The device firmware must provide compatible RKNN Runtime 2.3.2 and GStreamer RTSP libraries. Do not replace `librknnrt.so` on the board with a library from another board or SDK.

Verify the extracted files before deployment:

```sh
sha256sum -c SHA256SUMS
```
