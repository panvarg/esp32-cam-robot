/*
 * ESP32-CAM + L298N – robot zdalnie sterowany z telefonu.
 *
 *  - ESP32 tworzy własną sieć Wi-Fi (lub łączy się z domową – patrz USE_AP_MODE)
 *  - strona www z wirtualnym analogiem (joystick jak w grach) + podgląd z kamery
 *  - sterowanie przez WebSocket (niskie opóźnienia), obraz MJPEG na porcie 81
 *  - failsafe: silniki stają, gdy przez FAILSAFE_MS nie przyjdzie żadna komenda
 *  - opcjonalnie odbiornik ExpressLRS (CRSF) – aparatura ma pierwszeństwo,
 *    gdy jest połączona i uzbrojona; w pozostałych przypadkach steruje telefon
 *
 * Płytka w Arduino IDE: "AI Thinker ESP32-CAM", Partition Scheme: "Huge APP".
 * Wymagany pakiet esp32 by Espressif w wersji 2.x lub 3.x.
 */

#include <WiFi.h>
#include "esp_arduino_version.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include "driver/uart.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include "camera_pins.h"
#include "index_html.h"

// ======================= KONFIGURACJA =======================

// true  -> robot tworzy własną sieć (telefon łączy się z nią, adres 192.168.4.1)
// false -> robot łączy się z Twoim routerem (adres sprawdzisz w Serial Monitorze)
#define USE_AP_MODE true

const char *AP_SSID = "ESP32-Robot";
const char *AP_PASS = "robot1234";        // min. 8 znaków

const char *STA_SSID = "TwojaSiec";
const char *STA_PASS = "TwojeHaslo";

// Piny L298N (zworki ENA i ENB ZAŁOŻONE – prędkość regulujemy PWM-em na INx)
#define PIN_IN1 14   // silnik lewy  (OUT1/OUT2)
#define PIN_IN2 15
#define PIN_IN3 13   // silnik prawy (OUT3/OUT4)
#define PIN_IN4 12

// Jeśli koło kręci się w złą stronę – zmień na true zamiast przekładać kable
#define INVERT_LEFT  false
#define INVERT_RIGHT false

// Dioda flash na płytce
#define PIN_LIGHT   4
#define LIGHT_DUTY  60   // jasność 0..255 (pełna moc mocno grzeje i oślepia)

// Obrót obrazu, jeśli kamera jest zamontowana do góry nogami
#define CAM_VFLIP   false
#define CAM_HMIRROR false

#define PWM_FREQ     1000  // Hz – dla L298N 500..2000 Hz działa dobrze
#define PWM_RES      8     // rozdzielczość 8 bit -> 0..255
#define MIN_DUTY     90    // minimalne wypełnienie, przy którym silnik rusza
#define FAILSAFE_MS  600   // stop, gdy brak komend przez tyle ms

// ---------- Odbiornik ELRS / CRSF ----------
// Podłączenie: TX odbiornika -> GPIO 2, VCC -> 5V, GND -> GND (RX odbiornika zostaw wolny)
// Numery kanałów jak w aparaturze (1..16). 0 = nieużywany.
#define RC_ENABLED      true
#define RC_RX_PIN       2
#define RC_CH_TURN      1    // prawy drążek lewo/prawo (aileron)
#define RC_CH_DRIVE     2    // prawy drążek góra/dół (elevator)
#define RC_CH_SPEED     3    // gaz = ograniczenie mocy (0 -> ustala RC_DEFAULT_SPEED)
#define RC_CH_ARM       5    // AUX1: włączony = aparatura przejmuje sterowanie
#define RC_CH_LIGHT     6    // AUX2: światło
#define RC_MIN_SPEED    25   // moc przy gazie na dole, %
#define RC_DEFAULT_SPEED 70  // moc, gdy RC_CH_SPEED = 0, %
#define RC_TURN_GAIN    75   // czułość skrętu, %
#define RC_DEADBAND     5    // martwa strefa drążków, %
#define RC_TIMEOUT_MS   250  // brak ramek przez tyle ms = brak połączenia

// Kanały LEDC – 0/1 (timer 0) zajmuje kamera (XCLK), więc zaczynamy od 2
#define CH_LIGHT 2
#define CH_IN1   4
#define CH_IN2   5
#define CH_IN3   6
#define CH_IN4   7

// ============================================================

static volatile int targetX = 0;            // -100..100 (skręt)
static volatile int targetY = 0;            // -100..100 (przód/tył)
static volatile unsigned long lastCmdMs = 0;
static volatile bool lightOn = false;
static bool lightApplied = false;

// Stan aparatury: 0 = brak, 1 = połączona (steruje telefon), 2 = uzbrojona (steruje aparatura)
static volatile uint8_t rcState = 0;
static bool cameraOk = false;
static bool hasPsram = false;

// ---------- Presety jakości wideo ----------
// Mniejsza rozdzielczość / wyższa liczba "quality" = mniej danych = więcej klatek/s.
// jpeg quality: 10 (najlepsza) .. 63 (najgorsza)
struct VideoPreset { framesize_t size; int quality; const char *name; };
static const VideoPreset VIDEO_PRESETS[] = {
  { FRAMESIZE_QVGA, 14, "Plynnie 320x240" },   // 0 – najwięcej FPS, najmniejsze opóźnienie
  { FRAMESIZE_VGA,  12, "Balans 640x480" },    // 1
  { FRAMESIZE_SVGA, 10, "Jakosc 800x600" },    // 2 – najładniej, ale wolniej
};
#define VIDEO_PRESET_COUNT (sizeof(VIDEO_PRESETS) / sizeof(VIDEO_PRESETS[0]))
#define VIDEO_PRESET_DEFAULT 1

httpd_handle_t webHttpd = NULL;
httpd_handle_t streamHttpd = NULL;

// ---------- PWM (kompatybilne z pakietem esp32 2.x i 3.x) ----------

static void pwmAttach(uint8_t pin, uint8_t ch, uint32_t freq) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttachChannel(pin, freq, PWM_RES, ch);
#else
  ledcSetup(ch, freq, PWM_RES);
  ledcAttachPin(pin, ch);
#endif
}

static void pwmWrite(uint8_t pin, uint8_t ch, uint32_t duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  (void)ch;
  ledcWrite(pin, duty);
#else
  (void)pin;
  ledcWrite(ch, duty);
#endif
}

// ---------- Silniki ----------

// speed: -255..255
static void setMotor(uint8_t pinA, uint8_t chA, uint8_t pinB, uint8_t chB, int speed) {
  if (speed > 0) {
    pwmWrite(pinA, chA, speed);
    pwmWrite(pinB, chB, 0);
  } else if (speed < 0) {
    pwmWrite(pinA, chA, 0);
    pwmWrite(pinB, chB, -speed);
  } else {
    pwmWrite(pinA, chA, 0);
    pwmWrite(pinB, chB, 0);
  }
}

// Przelicza wartość -100..100 na wypełnienie PWM z uwzględnieniem MIN_DUTY
static int toDuty(int v) {
  if (v == 0) return 0;
  int mag = map(abs(v), 1, 100, MIN_DUTY, 255);
  return v > 0 ? mag : -mag;
}

// Miksowanie "arcade drive": jeden analog -> dwa silniki (napęd czołgowy)
static void drive(int x, int y) {
  int left  = y + x;
  int right = y - x;

  int maxMag = max(abs(left), abs(right));
  if (maxMag > 100) {               // zachowaj proporcje przy wychyleniu po skosie
    left  = left  * 100 / maxMag;
    right = right * 100 / maxMag;
  }

  if (INVERT_LEFT)  left  = -left;
  if (INVERT_RIGHT) right = -right;

  setMotor(PIN_IN1, CH_IN1, PIN_IN2, CH_IN2, toDuty(left));
  setMotor(PIN_IN3, CH_IN3, PIN_IN4, CH_IN4, toDuty(right));
}

static void motorsInit() {
  pwmAttach(PIN_IN1, CH_IN1, PWM_FREQ);
  pwmAttach(PIN_IN2, CH_IN2, PWM_FREQ);
  pwmAttach(PIN_IN3, CH_IN3, PWM_FREQ);
  pwmAttach(PIN_IN4, CH_IN4, PWM_FREQ);
  drive(0, 0);

  pwmAttach(PIN_LIGHT, CH_LIGHT, 5000);
  pwmWrite(PIN_LIGHT, CH_LIGHT, 0);
}

static void applyVideoPreset(int idx) {
  if (!cameraOk) return;
  if (idx < 0 || idx >= (int)VIDEO_PRESET_COUNT) return;
  if (!hasPsram) idx = 0;             // bez PSRAM tylko najmniejsza rozdzielczość
  sensor_t *s = esp_camera_sensor_get();
  s->set_framesize(s, VIDEO_PRESETS[idx].size);
  s->set_quality(s, VIDEO_PRESETS[idx].quality);
  Serial.printf("Wideo: %s\n", VIDEO_PRESETS[idx].name);
}

// ---------- Odbiornik ELRS (protokół CRSF, 420000 bodów) ----------

#define RC_UART          UART_NUM_2
#define CRSF_ADDR_FC     0xC8
#define CRSF_TYPE_LINK   0x14
#define CRSF_TYPE_RC     0x16
#define CRSF_MID         992
#define CRSF_SPAN        820   // 172..1811 -> -100..100 %

static uint16_t rcCh[16];
static bool rcHaveFrame = false;
static unsigned long rcLastFrameMs = 0;
static uint8_t rcLinkQuality = 100;
static bool rcArmLatched = false;
static int rcLastLightSw = -1;
static int rcX = 0, rcY = 0;
static uint8_t rcBuf[64];
static uint8_t rcLen = 0;

static uint8_t crc8(const uint8_t *d, uint8_t n) {
  uint8_t c = 0;
  while (n--) {
    c ^= *d++;
    for (int i = 0; i < 8; i++) c = (c & 0x80) ? (c << 1) ^ 0xD5 : (c << 1);
  }
  return c;
}

static void rcInit() {
  if (!RC_ENABLED) return;
  // Sterownik UART z ESP-IDF zamiast Serial2 – HardwareSerial przy braku pinu TX
  // podpiąłby domyślny GPIO 17, który w ESP32-CAM obsługuje PSRAM.
  uart_config_t cfg = {};
  cfg.baud_rate = 420000;
  cfg.data_bits = UART_DATA_8_BITS;
  cfg.parity = UART_PARITY_DISABLE;
  cfg.stop_bits = UART_STOP_BITS_1;
  cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  cfg.source_clk = UART_SCLK_APB;
  uart_param_config(RC_UART, &cfg);
  uart_set_pin(RC_UART, UART_PIN_NO_CHANGE, RC_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  uart_driver_install(RC_UART, 1024, 0, 0, NULL, 0);
}

static void rcFrame(const uint8_t *f) {   // f: [adres][dlugosc][typ][dane...][crc]
  uint8_t type = f[2];
  const uint8_t *p = f + 3;
  uint8_t plen = f[1] - 2;

  if (type == CRSF_TYPE_RC && plen >= 22) {
    // 16 kanałów po 11 bitów, little-endian
    uint32_t bits = 0;
    uint8_t nb = 0, ch = 0;
    for (int i = 0; i < 22 && ch < 16; i++) {
      bits |= (uint32_t)p[i] << nb;
      nb += 8;
      while (nb >= 11 && ch < 16) {
        rcCh[ch++] = bits & 0x7FF;
        bits >>= 11;
        nb -= 11;
      }
    }
    rcHaveFrame = true;
    rcLastFrameMs = millis();
  } else if (type == CRSF_TYPE_LINK && plen >= 3) {
    rcLinkQuality = p[2];                // uplink LQ, 0 = utracone połączenie
  }
}

static void rcPoll() {
  if (!RC_ENABLED) return;
  uint8_t tmp[64];
  int n;
  while ((n = uart_read_bytes(RC_UART, tmp, sizeof(tmp), 0)) > 0) {
    for (int i = 0; i < n; i++) {
      uint8_t b = tmp[i];
      if (rcLen == 0 && b != CRSF_ADDR_FC) continue;
      rcBuf[rcLen++] = b;
      if (rcLen == 2 && (b < 2 || b > 62)) { rcLen = 0; continue; }
      if (rcLen > 2 && rcLen == rcBuf[1] + 2) {
        if (crc8(rcBuf + 2, rcBuf[1] - 1) == rcBuf[rcLen - 1]) rcFrame(rcBuf);
        rcLen = 0;
      }
    }
  }
}

static int rcRaw(int ch) { return ch >= 1 && ch <= 16 ? rcCh[ch - 1] : CRSF_MID; }

static int rcAxis(int ch) {               // -100..100 %
  if (ch == 0) return 0;
  int v = constrain((rcRaw(ch) - CRSF_MID) * 100 / CRSF_SPAN, -100, 100);
  return abs(v) < RC_DEADBAND ? 0 : v;
}

static bool rcSwitch(int ch) { return ch != 0 && rcRaw(ch) > 1300; }   // > ~1700 us

static void rcUpdate() {
  bool connected = RC_ENABLED && rcHaveFrame && rcLinkQuality > 0 &&
                   millis() - rcLastFrameMs < RC_TIMEOUT_MS;
  if (!connected) {
    rcState = 0;
    rcArmLatched = false;
    rcLastLightSw = -1;
    return;
  }

  int turn = rcAxis(RC_CH_TURN);
  int fwd  = rcAxis(RC_CH_DRIVE);
  int speed = RC_DEFAULT_SPEED;
  if (RC_CH_SPEED) {
    int pct = constrain((rcRaw(RC_CH_SPEED) - 172) * 100 / 1639, 0, 100);
    speed = RC_MIN_SPEED + pct * (100 - RC_MIN_SPEED) / 100;
  }

  // Uzbrojenie tylko przy drążku w środku – robot nie wystrzeli po przełączeniu AUX
  if (!rcSwitch(RC_CH_ARM)) rcArmLatched = false;
  else if (!rcArmLatched && abs(turn) < 10 && abs(fwd) < 10) rcArmLatched = true;

  rcX = turn * speed / 100 * RC_TURN_GAIN / 100;
  rcY = fwd * speed / 100;
  rcState = rcArmLatched ? 2 : 1;

  // Światło: przełącznik działa przy każdej zmianie, telefon też może je przełączać
  if (RC_CH_LIGHT) {
    int sw = rcSwitch(RC_CH_LIGHT);
    if (sw != rcLastLightSw) lightOn = sw;
    rcLastLightSw = sw;
  }
}

// ---------- Komendy z przeglądarki ----------
// "M x y"  – ruch, x i y w zakresie -100..100
// "L 0|1"  – światło
// "Q 0..2" – preset jakości wideo
// "P"      – ping (podtrzymanie failsafe)

static void handleCommand(const char *cmd) {
  int a = 0, b = 0;
  switch (cmd[0]) {
    case 'M':
      if (sscanf(cmd + 1, "%d %d", &a, &b) == 2) {
        targetX = constrain(a, -100, 100);
        targetY = constrain(b, -100, 100);
      }
      break;
    case 'L':
      if (sscanf(cmd + 1, "%d", &a) == 1) lightOn = (a != 0);
      break;
    case 'Q':
      if (sscanf(cmd + 1, "%d", &a) == 1) applyVideoPreset(a);
      break;
    default:
      break;
  }
  lastCmdMs = millis();
}

// ---------- Serwer WWW (port 80) ----------

static esp_err_t indexHandler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t wsHandler(httpd_req_t *req) {
  if (req->method == HTTP_GET) {      // handshake
    Serial.println("WebSocket: polaczono");
    return ESP_OK;
  }

  httpd_ws_frame_t frame;
  memset(&frame, 0, sizeof(frame));
  frame.type = HTTPD_WS_TYPE_TEXT;

  esp_err_t ret = httpd_ws_recv_frame(req, &frame, 0);   // tylko długość
  if (ret != ESP_OK) return ret;
  if (frame.len == 0) return ESP_OK;

  uint8_t buf[64];
  if (frame.len >= sizeof(buf)) return ESP_FAIL;
  frame.payload = buf;
  ret = httpd_ws_recv_frame(req, &frame, frame.len);
  if (ret != ESP_OK) return ret;
  buf[frame.len] = 0;

  if (frame.type != HTTPD_WS_TYPE_TEXT) return ESP_OK;
  handleCommand((const char *)buf);

  // Odpowiedź ze stanem: "R <stan aparatury> <światło>"
  char out[16];
  int olen = snprintf(out, sizeof(out), "R %d %d", rcState, lightOn ? 1 : 0);
  httpd_ws_frame_t reply;
  memset(&reply, 0, sizeof(reply));
  reply.type = HTTPD_WS_TYPE_TEXT;
  reply.payload = (uint8_t *)out;
  reply.len = olen;
  return httpd_ws_send_frame(req, &reply);
}

// ---------- Strumień MJPEG (port 81) ----------

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static esp_err_t streamHandler(httpd_req_t *req) {
  char partBuf[64];
  esp_err_t res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  while (true) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Blad pobrania klatki");
      res = ESP_FAIL;
      break;
    }

    size_t hlen = snprintf(partBuf, sizeof(partBuf), STREAM_PART, fb->len);
    res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, partBuf, hlen);
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);

    esp_camera_fb_return(fb);
    if (res != ESP_OK) break;         // klient się rozłączył
  }
  return res;
}

static void startServers() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.ctrl_port = 32768;
  config.lru_purge_enable = true;

  httpd_uri_t indexUri;
  memset(&indexUri, 0, sizeof(indexUri));
  indexUri.uri = "/";
  indexUri.method = HTTP_GET;
  indexUri.handler = indexHandler;

  httpd_uri_t wsUri;
  memset(&wsUri, 0, sizeof(wsUri));
  wsUri.uri = "/ws";
  wsUri.method = HTTP_GET;
  wsUri.handler = wsHandler;
  wsUri.is_websocket = true;

  if (httpd_start(&webHttpd, &config) == ESP_OK) {
    httpd_register_uri_handler(webHttpd, &indexUri);
    httpd_register_uri_handler(webHttpd, &wsUri);
  }

  config.server_port = 81;
  config.ctrl_port = 32769;

  httpd_uri_t streamUri;
  memset(&streamUri, 0, sizeof(streamUri));
  streamUri.uri = "/stream";
  streamUri.method = HTTP_GET;
  streamUri.handler = streamHandler;

  if (httpd_start(&streamHttpd, &config) == ESP_OK) {
    httpd_register_uri_handler(streamHttpd, &streamUri);
  }
}

// ---------- Kamera ----------

static bool cameraInit() {
  camera_config_t c;
  memset(&c, 0, sizeof(c));
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM;
  c.pin_d1 = Y3_GPIO_NUM;
  c.pin_d2 = Y4_GPIO_NUM;
  c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM;
  c.pin_d5 = Y7_GPIO_NUM;
  c.pin_d6 = Y8_GPIO_NUM;
  c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM;
  c.pin_pclk = PCLK_GPIO_NUM;
  c.pin_vsync = VSYNC_GPIO_NUM;
  c.pin_href = HREF_GPIO_NUM;
  c.pin_sccb_sda = SIOD_GPIO_NUM;
  c.pin_sccb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn = PWDN_GPIO_NUM;
  c.pin_reset = RESET_GPIO_NUM;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;
  c.grab_mode = CAMERA_GRAB_LATEST;   // zawsze najświeższa klatka = mniejsze opóźnienie

  hasPsram = psramFound();
  if (hasPsram) {
    // bufory alokujemy dla największego presetu, potem schodzimy do domyślnego
    c.frame_size = FRAMESIZE_SVGA;
    c.jpeg_quality = 10;
    c.fb_count = 2;
    c.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    c.frame_size = FRAMESIZE_QVGA;
    c.jpeg_quality = 14;
    c.fb_count = 1;
    c.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&c);
  if (err != ESP_OK) {
    Serial.printf("Blad inicjalizacji kamery: 0x%x\n", err);
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, CAM_VFLIP);
  s->set_hmirror(s, CAM_HMIRROR);
  cameraOk = true;
  applyVideoPreset(VIDEO_PRESET_DEFAULT);
  return true;
}

// ---------- Wi-Fi ----------

static void wifiInit() {
  WiFi.setSleep(false);               // mniejsze opóźnienia sterowania

  if (!USE_AP_MODE) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(STA_SSID, STA_PASS);
    Serial.print("Laczenie z siecia");
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
      delay(300);
      Serial.print(".");
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("Otworz w przegladarce: http://");
      Serial.println(WiFi.localIP());
      return;
    }
    Serial.println("Brak polaczenia - przelaczam na tryb AP");
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  WiFi.setSleep(false);
  Serial.printf("Siec: %s  haslo: %s\n", AP_SSID, AP_PASS);
  Serial.print("Otworz w przegladarce: http://");
  Serial.println(WiFi.softAPIP());
}

// ============================================================

void setup() {
  // Wyłącz detektor brown-out – rozruch silników potrafi chwilowo obniżyć napięcie
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  Serial.println("\nESP32-CAM Robot");

  motorsInit();
  rcInit();

  if (!cameraInit()) {
    Serial.println("Kamera nie dziala - sterowanie bedzie dzialac bez obrazu");
  }

  wifiInit();
  startServers();
}

void loop() {
  rcPoll();
  rcUpdate();

  int x = 0, y = 0;
  if (rcState == 2) {
    // aparatura połączona i uzbrojona – ma pierwszeństwo
    x = rcX;
    y = rcY;
  } else if (millis() - lastCmdMs <= FAILSAFE_MS) {
    // telefon; bez świeżych komend (failsafe) robot stoi
    x = targetX;
    y = targetY;
  }
  drive(x, y);

  bool l = lightOn;
  if (l != lightApplied) {
    pwmWrite(PIN_LIGHT, CH_LIGHT, l ? LIGHT_DUTY : 0);
    lightApplied = l;
  }

  delay(2);
}
