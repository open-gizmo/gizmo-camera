#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <esp_http_server.h>

#include "camera_pins.h"
#include "secrets.h"

struct FrameSizeName {
  framesize_t size;
  const char *name;
  uint16_t width;
  uint16_t height;
};

static const FrameSizeName FRAME_SIZES[] = {
    {FRAMESIZE_QQVGA, "QQVGA", 160, 120},    {FRAMESIZE_QVGA, "QVGA", 320, 240},
    {FRAMESIZE_CIF, "CIF", 400, 296},        {FRAMESIZE_VGA, "VGA", 640, 480},
    {FRAMESIZE_SVGA, "SVGA", 800, 600},      {FRAMESIZE_XGA, "XGA", 1024, 768},
    {FRAMESIZE_HD, "HD", 1280, 720},         {FRAMESIZE_SXGA, "SXGA", 1280, 1024},
    {FRAMESIZE_UXGA, "UXGA", 1600, 1200},    {FRAMESIZE_FHD, "FHD", 1920, 1080},
    {FRAMESIZE_QXGA, "QXGA", 2048, 1536},    {FRAMESIZE_QHD, "QHD", 2560, 1440},
    {FRAMESIZE_WQXGA, "WQXGA", 2560, 1600},  {FRAMESIZE_QSXGA, "QSXGA", 2560, 1920},
};

static const int DEFAULT_JPEG_QUALITY = 10;  // 0-63, lower = better quality

static framesize_t maxFrameSize = FRAMESIZE_UXGA;
static framesize_t currentFrameSize = FRAMESIZE_UXGA;
static int currentQuality = DEFAULT_JPEG_QUALITY;
static const char *sensorName = "unknown";
static httpd_handle_t server = nullptr;

static const FrameSizeName *findFrameSize(framesize_t size) {
  for (const auto &fs : FRAME_SIZES) {
    if (fs.size == size) return &fs;
  }
  return nullptr;
}

static const FrameSizeName *findFrameSize(const char *name) {
  for (const auto &fs : FRAME_SIZES) {
    if (strcasecmp(fs.name, name) == 0) return &fs;
  }
  return nullptr;
}

static esp_err_t initCamera(framesize_t frameSize) {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = frameSize;
  config.jpeg_quality = DEFAULT_JPEG_QUALITY;
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  return esp_camera_init(&config);
}

static bool setupCamera() {
  if (!psramFound()) {
    Serial.println("PSRAM not found: high resolutions are not available");
  }

  esp_err_t err = initCamera(FRAMESIZE_UXGA);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }

  sensor_t *sensor = esp_camera_sensor_get();
  camera_sensor_info_t *info = esp_camera_sensor_get_info(&sensor->id);
  if (info) {
    sensorName = info->name;
    maxFrameSize = info->max_size;
  }

  // Sensors larger than 2MP (OV3660, OV5640) need buffers sized for their full resolution
  if (maxFrameSize > FRAMESIZE_UXGA) {
    esp_camera_deinit();
    err = initCamera(maxFrameSize);
    if (err != ESP_OK) {
      Serial.printf("Camera re-init at max resolution failed: 0x%x, falling back to UXGA\n", err);
      maxFrameSize = FRAMESIZE_UXGA;
      if (initCamera(maxFrameSize) != ESP_OK) return false;
    }
    sensor = esp_camera_sensor_get();
  }

  currentFrameSize = maxFrameSize;
  currentQuality = DEFAULT_JPEG_QUALITY;

  if (sensor->id.PID == OV3660_PID) {
    sensor->set_vflip(sensor, 1);
  }

  Serial.printf("Camera ready: sensor %s, max frame size %s\n", sensorName,
                findFrameSize(maxFrameSize) ? findFrameSize(maxFrameSize)->name : "?");
  return true;
}

static void discardFrames(int count) {
  for (int i = 0; i < count; i++) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) esp_camera_fb_return(fb);
  }
}

static bool getQueryParam(httpd_req_t *req, const char *key, char *value, size_t valueLen) {
  size_t queryLen = httpd_req_get_url_query_len(req) + 1;
  if (queryLen <= 1 || queryLen > 256) return false;
  char query[256];
  if (httpd_req_get_url_query_str(req, query, queryLen) != ESP_OK) return false;
  return httpd_query_key_value(query, key, value, valueLen) == ESP_OK;
}

static void addCommonHeaders(httpd_req_t *req) {
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
}

static esp_err_t sendError(httpd_req_t *req, const char *status, const char *message) {
  httpd_resp_set_status(req, status);
  httpd_resp_set_type(req, "application/json");
  addCommonHeaders(req);
  char body[160];
  snprintf(body, sizeof(body), "{\"error\":\"%s\"}", message);
  return httpd_resp_sendstr(req, body);
}

// GET /capture?res=UXGA&quality=10&flash=1
static esp_err_t captureHandler(httpd_req_t *req) {
  sensor_t *sensor = esp_camera_sensor_get();
  char value[16];

  framesize_t wantedSize = maxFrameSize;
  if (getQueryParam(req, "res", value, sizeof(value))) {
    const FrameSizeName *fs = findFrameSize(value);
    if (!fs) return sendError(req, "400 Bad Request", "unknown resolution");
    if (fs->size > maxFrameSize) return sendError(req, "400 Bad Request", "resolution not supported by sensor");
    wantedSize = fs->size;
  }

  int wantedQuality = DEFAULT_JPEG_QUALITY;
  if (getQueryParam(req, "quality", value, sizeof(value))) {
    wantedQuality = atoi(value);
    if (wantedQuality < 4 || wantedQuality > 63) {
      return sendError(req, "400 Bad Request", "quality must be between 4 and 63 (lower is better)");
    }
  }

  bool useFlash = getQueryParam(req, "flash", value, sizeof(value)) && strcmp(value, "1") == 0;

  // Frames right after a settings change can be corrupted or use the old settings
  int framesToDiscard = 1;
  if (wantedSize != currentFrameSize) {
    sensor->set_framesize(sensor, wantedSize);
    currentFrameSize = wantedSize;
    framesToDiscard = 3;
  }
  if (wantedQuality != currentQuality) {
    sensor->set_quality(sensor, wantedQuality);
    currentQuality = wantedQuality;
    framesToDiscard = 3;
  }

  if (useFlash) {
    digitalWrite(FLASH_LED_GPIO_NUM, HIGH);
    delay(150);
    framesToDiscard = max(framesToDiscard, 2);
  }

  uint32_t start = millis();
  // The single frame buffer holds a frame captured right after the previous request: drop it
  discardFrames(framesToDiscard);
  camera_fb_t *fb = esp_camera_fb_get();

  if (useFlash) digitalWrite(FLASH_LED_GPIO_NUM, LOW);

  if (!fb) return sendError(req, "500 Internal Server Error", "capture failed");
  uint32_t captureMs = millis() - start;

  char header[32];
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  addCommonHeaders(req);
  snprintf(header, sizeof(header), "%ux%u", fb->width, fb->height);
  httpd_resp_set_hdr(req, "X-Resolution", header);
  char captureHeader[16];
  snprintf(captureHeader, sizeof(captureHeader), "%u", captureMs);
  httpd_resp_set_hdr(req, "X-Capture-Time-Ms", captureHeader);

  esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  Serial.printf("Captured %ux%u, %u bytes, %u ms\n", fb->width, fb->height, fb->len, captureMs);
  esp_camera_fb_return(fb);
  return res;
}

// GET /status
static esp_err_t statusHandler(httpd_req_t *req) {
  const FrameSizeName *maxFs = findFrameSize(maxFrameSize);
  char resolutions[256] = "";
  for (const auto &fs : FRAME_SIZES) {
    if (fs.size > maxFrameSize) break;
    if (resolutions[0]) strlcat(resolutions, ",", sizeof(resolutions));
    strlcat(resolutions, "\"", sizeof(resolutions));
    strlcat(resolutions, fs.name, sizeof(resolutions));
    strlcat(resolutions, "\"", sizeof(resolutions));
  }

  char body[640];
  snprintf(body, sizeof(body),
           "{\"sensor\":\"%s\",\"max_resolution\":\"%s\",\"max_width\":%u,\"max_height\":%u,"
           "\"resolutions\":[%s],\"default_quality\":%d,\"ip\":\"%s\",\"rssi\":%d,"
           "\"uptime_s\":%lu,\"free_heap\":%u,\"free_psram\":%u}",
           sensorName, maxFs ? maxFs->name : "?", maxFs ? maxFs->width : 0, maxFs ? maxFs->height : 0,
           resolutions, DEFAULT_JPEG_QUALITY, WiFi.localIP().toString().c_str(), WiFi.RSSI(),
           millis() / 1000, ESP.getFreeHeap(), ESP.getFreePsram());

  httpd_resp_set_type(req, "application/json");
  addCommonHeaders(req);
  return httpd_resp_sendstr(req, body);
}

// GET /
static esp_err_t indexHandler(httpd_req_t *req) {
  static const char html[] =
      "<!doctype html><html><head><meta name=viewport content='width=device-width'>"
      "<title>ESP32-CAM</title><style>body{font-family:sans-serif;margin:1em}"
      "img{max-width:100%;margin-top:1em}</style></head><body>"
      "<h1>ESP32-CAM</h1>"
      "<button onclick=\"snap('')\">Capture</button> "
      "<button onclick=\"snap('&flash=1')\">Capture with flash</button>"
      "<p id=info></p><img id=img>"
      "<script>function snap(q){const t=Date.now();const i=document.getElementById('img');"
      "i.onload=()=>{document.getElementById('info').textContent=i.naturalWidth+'x'+i.naturalHeight+"
      "' in '+(Date.now()-t)+' ms'};i.src='/capture?t='+t+q}</script>"
      "<p>API: <code>GET /capture[?res=UXGA&amp;quality=10&amp;flash=1]</code>, "
      "<code>GET /status</code></p></body></html>";
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, html, sizeof(html) - 1);
}

static void startServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.stack_size = 8192;
  config.send_wait_timeout = 10;

  if (httpd_start(&server, &config) != ESP_OK) {
    Serial.println("Failed to start HTTP server");
    return;
  }

  httpd_uri_t index = {.uri = "/", .method = HTTP_GET, .handler = indexHandler, .user_ctx = nullptr};
  httpd_uri_t capture = {.uri = "/capture", .method = HTTP_GET, .handler = captureHandler, .user_ctx = nullptr};
  httpd_uri_t status = {.uri = "/status", .method = HTTP_GET, .handler = statusHandler, .user_ctx = nullptr};
  httpd_register_uri_handler(server, &index);
  httpd_register_uri_handler(server, &capture);
  httpd_register_uri_handler(server, &status);
}

static void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.printf("Connecting to %s", WIFI_SSID);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 30000) {
      Serial.println("\nWiFi connection timed out, restarting");
      ESP.restart();
    }
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nConnected, IP: %s\n", WiFi.localIP().toString().c_str());

  if (MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("mDNS: http://%s.local/\n", HOSTNAME);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  pinMode(FLASH_LED_GPIO_NUM, OUTPUT);
  digitalWrite(FLASH_LED_GPIO_NUM, LOW);

  if (!setupCamera()) {
    Serial.println("Restarting in 5 s");
    delay(5000);
    ESP.restart();
  }

  uint32_t start = millis();
  camera_fb_t *fb = esp_camera_fb_get();
  if (fb) {
    Serial.printf("Self-test capture: %ux%u, %u bytes, %lu ms\n", fb->width, fb->height, fb->len, millis() - start);
    esp_camera_fb_return(fb);
  } else {
    Serial.println("Self-test capture failed");
  }

  connectWiFi();
  startServer();
  Serial.printf("Ready: http://%s/capture\n", WiFi.localIP().toString().c_str());
}

void loop() {
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck > 10000) {
    lastCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi lost, reconnecting");
      WiFi.reconnect();
    }
  }
  delay(100);
}
