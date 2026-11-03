# ESP32-CAM HTTP snapshot server

PlatformIO firmware for the AI Thinker ESP32-CAM (OV2640). It joins your WiFi network and returns a JPEG photo for each HTTP GET request. Photos are taken at the sensor's highest resolution (UXGA, 1600x1200) by default.

## Setup

1. Put your WiFi credentials in `include/secrets.h`. See `include/secrets.example.h` for the format.
2. Build and flash: `pio run -t upload`. The upload port is set to `COM5` in `platformio.ini`.
3. Open the serial monitor (`pio device monitor`) to see the IP address. The board is also reachable at `http://esp32cam.local/` through mDNS.

## API

| Request | Response |
|---|---|
| `GET /` | Web page with capture buttons |
| `GET /capture` | JPEG at the highest resolution |
| `GET /capture?res=SVGA` | JPEG at a lower resolution: `QQVGA`, `QVGA`, `CIF`, `VGA`, `SVGA`, `XGA`, `HD`, `SXGA`, `UXGA` |
| `GET /capture?quality=8` | JPEG quality from 4 to 63, where lower numbers mean better quality. The default is 10. |
| `GET /capture?flash=1` | Turns on the flash LED for the shot |
| `GET /status` | JSON with sensor info, IP address, RSSI, uptime and free memory |

You can combine the parameters, for example `/capture?res=XGA&quality=12&flash=1`.

Each capture response includes the `X-Resolution` and `X-Capture-Time-Ms` headers.

Example:

```sh
curl -o photo.jpg http://esp32cam.local/capture
```
