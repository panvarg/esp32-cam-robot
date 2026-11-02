/*
 * ESP32-CAM + L298N – robot zdalnie sterowany z telefonu.
 *
 *  - ESP32 tworzy własną sieć Wi-Fi lub łączy się z domową (menu ⚙ → Wi-Fi)
 *  - strona www z wirtualnym analogiem (joystick jak w grach) + podgląd z kamery
 *  - sterowanie przez WebSocket (niskie opóźnienia), obraz MJPEG na porcie 81
 *  - failsafe: silniki stają, gdy przez FAILSAFE_MS nie przyjdzie żadna komenda
 *  - opcjonalnie odbiornik ExpressLRS (CRSF) – aparatura ma pierwszeństwo,
 *    gdy jest połączona i uzbrojona; w pozostałych przypadkach steruje telefon
 *  - napęd czołgowy (2 lub 4 silniki) albo Mecanum (4 silniki, 8 pinów);
 *    piny, napęd i kanały aparatury ustawia się na stronie (⚙ → Napęd / ELRS)
 *
 * Płytka w Arduino IDE: "AI Thinker ESP32-CAM", Partition Scheme: "Huge APP".
 * Wymagany pakiet esp32 by Espressif w wersji 2.x lub 3.x.
 */

#include <WiFi.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include "esp_arduino_version.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include "driver/uart.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include "camera_pins.h"
#include "index_html.h"

// ======================= KONFIGURACJA =======================

// Wi-Fi ustawiasz na stronie (⚙ → Wi-Fi). Poniżej wartości domyślne – obowiązują przy
// pierwszym starcie i po awaryjnym resecie (3 włączenia robota, każde krótsze niż 10 s).
#define DEF_AP_SSID   "ESP32-Robot"
#define DEF_AP_PASS   "robot1234"        // min. 8 znaków
#define DEF_HOSTNAME  "robot"            // adres http://robot.local w sieci domowej

// Napęd, piny L298N i odbiornik ELRS ustawiasz na stronie: ⚙ → Napęd / ELRS.
// Konfiguracja jest zapisywana w pamięci ESP; domyślne wartości są w HW_DEFAULTS niżej.
// Zworki ENA i ENB na L298N ZAŁOŻONE – prędkość regulujemy PWM-em na pinach INx.

// Dioda flash
#define LIGHT_DUTY  60   // jasność 0..255 (pełna moc mocno grzeje i oślepia)

// Domyślne odbicie obrazu (później można je zmienić w menu ustawień na stronie)
#define CAM_VFLIP   false
#define CAM_HMIRROR false

#define PWM_FREQ     1000  // Hz – dla L298N 500..2000 Hz działa dobrze
#define PWM_RES      8     // rozdzielczość 8 bit -> 0..255
#define FAILSAFE_MS  600   // stop, gdy brak komend przez tyle ms

// Aparatura (przypisanie kanałów jest w menu ELRS)
#define RC_MIN_SPEED    25   // moc przy gazie na dole, %
#define RC_DEFAULT_SPEED 70  // moc, gdy kanał mocy nie jest przypisany, %
#define RC_TURN_GAIN    75   // czułość skrętu, %
#define RC_DEADBAND     5    // martwa strefa drążków, %
#define RC_TIMEOUT_MS   250  // brak ramek przez tyle ms = brak połączenia

// ============================================================

// ---------- Napęd i piny (menu "Napęd" / "ELRS", zapisywane w pamięci ESP) ----------
// Wolne piny ESP32-CAM (AI-Thinker); reszta jest zajęta przez kamerę i PSRAM.
static const int8_t FREE_PINS[] = { 1, 2, 3, 4, 12, 13, 14, 15 };

#define HW_SETTINGS_VERSION 1
#define MODE_TANK    0
#define MODE_MECANUM 1
// Funkcje kanałów aparatury (indeksy w HwSettings::ch)
enum { RCF_TURN, RCF_DRIVE, RCF_STRAFE, RCF_SPEED, RCF_ARM, RCF_LIGHT, RCF_COUNT };

struct HwSettings {
  int8_t  ver;
  int8_t  mode;      // MODE_TANK / MODE_MECANUM
  int8_t  motors;    // 2 lub 4 (w trybie czołgu 4 silniki = dwa L298N z wejściami połączonymi równolegle)
  int8_t  pin[8];    // czołg: lewy A, lewy B, prawy A, prawy B (4 piny)
                     // mecanum: LP A/B, PP A/B, LT A/B, PT A/B (8 pinów)
  int8_t  inv[4];    // odwrócenie: czołg [lewy, prawy]; mecanum [LP, PP, LT, PT]
  uint8_t minDuty;   // minimalne wypełnienie PWM, przy którym silnik rusza
  int8_t  lightPin;  // -1 = brak
  int8_t  rcPin;     // -1 = brak odbiornika
  int8_t  ch[RCF_COUNT];  // numer kanału 1..16, 0 = nieprzypisany
};
static const HwSettings HW_DEFAULTS = {
  HW_SETTINGS_VERSION, MODE_TANK, 2,
  { 14, 15, 13, 12, 2, 4, 1, 3 },
  { 0, 0, 0, 0 },
  90,
  4,    // dioda flash na płytce
  2,    // TX odbiornika ELRS
  { 1, 2, 4, 3, 5, 6 }   // skręt CH1, jazda CH2, bok CH4, moc CH3 (gaz), uzbrojenie CH5, światło CH6
};
static HwSettings hw = HW_DEFAULTS;   // edytowane z menu
static HwSettings hwBoot;             // aktywne od startu (piny i tryb zmieniają się po restarcie)
static volatile bool hwDirty = false;
static volatile unsigned long hwDirtyMs = 0;
static volatile bool rebootRequested = false;
static bool lightActive = false;      // pin diody jest ustawiony i nie koliduje z silnikami
static bool rcActive = false;         // jw. dla odbiornika

// Kanały LEDC: 0/1 (timer 0) zajmuje kamera (XCLK); 8/9 omijamy na wszelki wypadek
static const uint8_t MOTOR_CH[8] = { 4, 5, 6, 7, 10, 11, 12, 13 };
#define CH_LIGHT 2

static volatile int targetX = 0;            // -100..100 (skręt / obrót)
static volatile int targetY = 0;            // -100..100 (przód/tył)
static volatile int targetS = 0;            // -100..100 (jazda bokiem, tylko Mecanum)
static volatile unsigned long lastCmdMs = 0;
static volatile bool lightOn = false;
static bool lightApplied = false;

// Stan aparatury: 0 = brak, 1 = połączona (steruje telefon), 2 = uzbrojona (steruje aparatura)
static volatile uint8_t rcState = 0;
static bool cameraOk = false;
static bool hasPsram = false;

// ---------- Ustawienia obrazu (menu na stronie, zapisywane w pamięci ESP) ----------
// Rozdzielczości dostępne w menu – indeks w tej tablicy to wartość "fs"
static const framesize_t FRAME_SIZES[] = {
  FRAMESIZE_QVGA,   // 0: 320x240
  FRAMESIZE_CIF,    // 1: 400x296
  FRAMESIZE_VGA,    // 2: 640x480
  FRAMESIZE_SVGA,   // 3: 800x600
  FRAMESIZE_XGA,    // 4: 1024x768
  FRAMESIZE_HD,     // 5: 1280x720
};
#define FRAME_SIZE_COUNT (int)(sizeof(FRAME_SIZES) / sizeof(FRAME_SIZES[0]))

#define CAM_SETTINGS_VERSION 1
struct CamSettings {
  int8_t  ver;
  int8_t  fs;   // rozdzielczość (indeks FRAME_SIZES)
  int8_t  q;    // jakość JPEG 10 (najlepsza) .. 40
  int8_t  br;   // jasność -2..2
  int8_t  ct;   // kontrast -2..2
  int8_t  sa;   // nasycenie -2..2
  int8_t  wb;   // balans bieli: 0 auto, 1 słońce, 2 pochmurno, 3 biuro, 4 dom
  int8_t  ae;   // automatyczna ekspozycja 0/1
  int8_t  al;   // korekta ekspozycji -2..2
  int16_t ex;   // ręczny czas naświetlania 0..1200 (gdy ae = 0)
  int8_t  fx;   // efekt: 0 brak, 1 negatyw, 2 cz-b, 3 czerwony, 4 zielony, 5 niebieski, 6 sepia
  int8_t  hm;   // odbicie lustrzane 0/1
  int8_t  vf;   // odwrócenie w pionie 0/1
  int16_t rot;  // obrót w przeglądarce: 0, 90, 180, 270
};
static const CamSettings CAM_DEFAULTS = {
  CAM_SETTINGS_VERSION, 2, 12, 0, 0, 0, 0, 1, 0, 300, 0, CAM_HMIRROR, CAM_VFLIP, 0
};
static CamSettings cam = CAM_DEFAULTS;
static volatile bool camDirty = false;
static volatile unsigned long camDirtyMs = 0;
static Preferences prefs;

static volatile uint32_t frameCount = 0;   // licznik klatek strumienia (do FPS)
static volatile uint32_t frameBytes = 0;
static volatile int streamFps = 0;
static volatile int streamKB = 0;          // średni rozmiar klatki w KB

// Kamera startuje z buforem 800x600 (jak w v1.0). Dla większych rozdzielczości
// jest uruchamiana ponownie z większym buforem – w loop(), pod mutexem.
#define FS_BASE_INIT 3                     // indeks FRAMESIZE_SVGA w FRAME_SIZES
static int camInitFs = -1;
static volatile bool camNeedReinit = false;
static volatile bool camPause = false;     // strumień nie bierze nowych klatek (restart kamery)
static SemaphoreHandle_t camMutex = NULL;

static int camWantedInitFs() { return hasPsram ? max((int)cam.fs, FS_BASE_INIT) : 0; }

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

// ---------- Napęd: konfiguracja ----------

static int motorPinCount(const HwSettings &h) { return h.mode == MODE_MECANUM ? 8 : 4; }

static bool isFreePin(int p) {
  for (unsigned i = 0; i < sizeof(FREE_PINS); i++) if (FREE_PINS[i] == p) return true;
  return false;
}

static bool usedByMotors(const HwSettings &h, int p) {
  for (int i = 0; i < motorPinCount(h); i++) if (h.pin[i] == p) return true;
  return false;
}

// Piny silników: tylko wolne i każdy inny. Złe ustawienia -> domyślne (robot zawsze ruszy).
static bool hwValid(const HwSettings &h) {
  if (h.ver != HW_SETTINGS_VERSION || h.mode < 0 || h.mode > 1) return false;
  int n = motorPinCount(h);
  for (int i = 0; i < n; i++) {
    if (!isFreePin(h.pin[i])) return false;
    for (int j = 0; j < i; j++) if (h.pin[i] == h.pin[j]) return false;
  }
  for (int i = 0; i < RCF_COUNT; i++) if (h.ch[i] < 0 || h.ch[i] > 16) return false;
  return true;
}

static void hwLoad() {
  prefs.begin("hw", true);
  HwSettings tmp;
  if (prefs.getBytesLength("s") == sizeof(tmp) &&
      prefs.getBytes("s", &tmp, sizeof(tmp)) == sizeof(tmp) && hwValid(tmp)) {
    hw = tmp;
  } else if (prefs.getBytesLength("s") > 0) {
    Serial.println("Bledna konfiguracja napedu - uzywam domyslnej");
  }
  prefs.end();
  hwBoot = hw;
  lightActive = isFreePin(hw.lightPin) && !usedByMotors(hw, hw.lightPin);
  rcActive = isFreePin(hw.rcPin) && !usedByMotors(hw, hw.rcPin) && hw.rcPin != hw.lightPin;
}

static void hwSave() {
  prefs.begin("hw", false);
  prefs.putBytes("s", &hw, sizeof(hw));
  prefs.end();
  Serial.println("Zapisano konfiguracje napedu");
}

// Czy jakiś aktywny pin to 1 lub 3 (UART0) – wtedy Serial Monitor musi zostać wyłączony
static bool hwUsesUart0() {
  for (int i = 0; i < motorPinCount(hwBoot); i++) if (hwBoot.pin[i] == 1 || hwBoot.pin[i] == 3) return true;
  if (lightActive && (hwBoot.lightPin == 1 || hwBoot.lightPin == 3)) return true;
  if (rcActive && (hwBoot.rcPin == 1 || hwBoot.rcPin == 3)) return true;
  return false;
}

// ---------- Silniki ----------

// silnik nr i (0..3), speed: -255..255
static void setMotor(int i, int speed) {
  int a = hwBoot.pin[2 * i], b = hwBoot.pin[2 * i + 1];
  uint8_t ca = MOTOR_CH[2 * i], cb = MOTOR_CH[2 * i + 1];
  pwmWrite(a, ca, speed > 0 ? speed : 0);
  pwmWrite(b, cb, speed < 0 ? -speed : 0);
}

// Przelicza wartość -100..100 na wypełnienie PWM z uwzględnieniem minimalnej mocy
static int toDuty(int v) {
  if (v == 0) return 0;
  int mag = map(abs(v), 1, 100, hw.minDuty, 255);
  return v > 0 ? mag : -mag;
}

// Skaluje koła tak, żeby żadne nie przekroczyło 100% (zachowuje proporcje)
static void normalize(int *w, int n) {
  int m = 0;
  for (int i = 0; i < n; i++) m = max(m, abs(w[i]));
  if (m > 100) for (int i = 0; i < n; i++) w[i] = w[i] * 100 / m;
}

static volatile int wheelOut[4];   // ostatnia moc kół w % (do podglądu na stronie)

// x – skręt/obrót, y – przód/tył, s – jazda bokiem (Mecanum); wszystko -100..100
static void drive(int x, int y, int s = 0) {
  if (hwBoot.mode == MODE_MECANUM) {
    // kolejność: lewe przednie, prawe przednie, lewe tylne, prawe tylne
    int w[4] = { y + s + x, y - s - x, y - s + x, y + s - x };
    normalize(w, 4);
    for (int i = 0; i < 4; i++) {
      wheelOut[i] = w[i];
      setMotor(i, toDuty(hw.inv[i] ? -w[i] : w[i]));
    }
  } else {
    // "arcade drive": skręt czołgowy, lewa i prawa strona
    int w[2] = { y + x, y - x };
    normalize(w, 2);
    for (int i = 0; i < 2; i++) {
      wheelOut[i] = w[i];
      setMotor(i, toDuty(hw.inv[i] ? -w[i] : w[i]));
    }
    wheelOut[2] = wheelOut[3] = 0;
  }
}

static void motorsInit() {
  for (int i = 0; i < motorPinCount(hwBoot); i++) {
    pinMode(hwBoot.pin[i], OUTPUT);
    digitalWrite(hwBoot.pin[i], LOW);
    pwmAttach(hwBoot.pin[i], MOTOR_CH[i], PWM_FREQ);
  }
  drive(0, 0);

  if (lightActive) {
    pwmAttach(hwBoot.lightPin, CH_LIGHT, 5000);
    pwmWrite(hwBoot.lightPin, CH_LIGHT, 0);
  }
}

// Zmiana z menu "Napęd" / "ELRS", np. hwSet("p3", 13). Piny i tryb działają po restarcie.
static bool hwSet(const char *key, int v) {
  if      (!strcmp(key, "mode")) hw.mode = v ? MODE_MECANUM : MODE_TANK;
  else if (!strcmp(key, "mot"))  hw.motors = v == 4 ? 4 : 2;
  else if (key[0] == 'p' && key[1] >= '0' && key[1] <= '7' && !key[2]) hw.pin[key[1] - '0'] = isFreePin(v) ? v : -1;
  else if (!strncmp(key, "inv", 3) && key[3] >= '0' && key[3] <= '3' && !key[4]) hw.inv[key[3] - '0'] = v ? 1 : 0;
  else if (!strcmp(key, "md"))   hw.minDuty = constrain(v, 0, 200);
  else if (!strcmp(key, "lp"))   hw.lightPin = isFreePin(v) ? v : -1;
  else if (!strcmp(key, "rp"))   hw.rcPin = isFreePin(v) ? v : -1;
  else if (key[0] == 'c' && key[1] >= '0' && key[1] < '0' + RCF_COUNT && !key[2]) hw.ch[key[1] - '0'] = constrain(v, 0, 16);
  else if (!strcmp(key, "reset")) hw = HW_DEFAULTS;
  else if (!strcmp(key, "reboot")) { rebootRequested = true; return true; }
  else return false;
  hwDirty = true;
  hwDirtyMs = millis();
  return true;
}

// Czy zmiany w menu wymagają restartu (piny, tryb napędu, piny diody / odbiornika)
static bool hwPending() {
  if (hw.mode != hwBoot.mode || hw.lightPin != hwBoot.lightPin || hw.rcPin != hwBoot.rcPin) return true;
  for (int i = 0; i < 8; i++) if (hw.pin[i] != hwBoot.pin[i]) return true;
  return false;
}

// ---------- Kamera: ustawienia ----------

// Do czujnika wysyłamy tylko to, co różni się od stanu po jego starcie – przy
// domyślnych ustawieniach kamera pracuje dokładnie tak jak w v1.0.
static CamSettings camApplied;

static void camResetApplied() {      // stan czujnika zaraz po esp_camera_init()
  camApplied = CAM_DEFAULTS;
  camApplied.fs = -1;                 // rozdzielczość i jakość ustawiamy zawsze
  camApplied.q = -1;
  camApplied.hm = 0;
  camApplied.vf = 0;
}

static void camApplyAll() {
  if (!cameraOk) return;
  sensor_t *s = esp_camera_sensor_get();
  CamSettings &a = camApplied;
  // nigdy większa niż bufor z inicjalizacji; bez PSRAM tylko najmniejsza
  int fs = hasPsram ? min((int)cam.fs, camInitFs) : 0;
  if (fs != a.fs)        s->set_framesize(s, FRAME_SIZES[fs]);
  if (cam.q != a.q)      s->set_quality(s, cam.q);
  if (cam.br != a.br)    s->set_brightness(s, cam.br);
  if (cam.ct != a.ct)    s->set_contrast(s, cam.ct);
  if (cam.sa != a.sa)    s->set_saturation(s, cam.sa);
  if (cam.wb != a.wb)    s->set_wb_mode(s, cam.wb);
  if (cam.ae != a.ae)    s->set_exposure_ctrl(s, cam.ae);
  if (cam.al != a.al)    s->set_ae_level(s, cam.al);
  if (!cam.ae && (cam.ex != a.ex || cam.ae != a.ae)) s->set_aec_value(s, cam.ex);
  if (cam.fx != a.fx)    s->set_special_effect(s, cam.fx);
  if (cam.hm != a.hm)    s->set_hmirror(s, cam.hm);
  if (cam.vf != a.vf)    s->set_vflip(s, cam.vf);
  a = cam;
  a.fs = fs;
}

// Ustawia jeden parametr, np. camSet("br", 1). Zwraca false dla nieznanej nazwy.
static bool camSet(const char *key, int v) {
  if      (!strcmp(key, "fs")) cam.fs = constrain(v, 0, FRAME_SIZE_COUNT - 1);
  else if (!strcmp(key, "q"))  cam.q  = constrain(v, 10, 40);
  else if (!strcmp(key, "br")) cam.br = constrain(v, -2, 2);
  else if (!strcmp(key, "ct")) cam.ct = constrain(v, -2, 2);
  else if (!strcmp(key, "sa")) cam.sa = constrain(v, -2, 2);
  else if (!strcmp(key, "wb")) cam.wb = constrain(v, 0, 4);
  else if (!strcmp(key, "ae")) cam.ae = v ? 1 : 0;
  else if (!strcmp(key, "al")) cam.al = constrain(v, -2, 2);
  else if (!strcmp(key, "ex")) cam.ex = constrain(v, 0, 1200);
  else if (!strcmp(key, "fx")) cam.fx = constrain(v, 0, 6);
  else if (!strcmp(key, "hm")) cam.hm = v ? 1 : 0;
  else if (!strcmp(key, "vf")) cam.vf = v ? 1 : 0;
  else if (!strcmp(key, "rot")) cam.rot = ((v / 90) & 3) * 90;
  else if (!strcmp(key, "reset")) cam = CAM_DEFAULTS;
  else return false;
  if (camWantedInitFs() != camInitFs) camNeedReinit = true;   // inny bufor -> restart kamery w loop()
  camApplyAll();
  camDirty = true;                    // zapis do pamięci w loop(), z opóźnieniem
  camDirtyMs = millis();
  return true;
}

static void camLoad() {
  prefs.begin("cam", true);
  CamSettings tmp;
  if (prefs.getBytesLength("s") == sizeof(tmp) &&
      prefs.getBytes("s", &tmp, sizeof(tmp)) == sizeof(tmp) &&
      tmp.ver == CAM_SETTINGS_VERSION) {
    cam = tmp;
  }
  prefs.end();
}

static void camSave() {
  prefs.begin("cam", false);
  prefs.putBytes("s", &cam, sizeof(cam));
  prefs.end();
  Serial.println("Zapisano ustawienia obrazu");
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
static int rcX = 0, rcY = 0, rcS = 0;
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
  if (!rcActive) return;
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
  uart_set_pin(RC_UART, UART_PIN_NO_CHANGE, hwBoot.rcPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
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
  if (!rcActive) return;
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

static bool rcConnected() {
  return rcActive && rcHaveFrame && rcLinkQuality > 0 && millis() - rcLastFrameMs < RC_TIMEOUT_MS;
}

static void rcUpdate() {
  if (!rcConnected()) {
    rcState = 0;
    rcArmLatched = false;
    rcLastLightSw = -1;
    return;
  }

  int turn = rcAxis(hw.ch[RCF_TURN]);
  int fwd  = rcAxis(hw.ch[RCF_DRIVE]);
  int side = hwBoot.mode == MODE_MECANUM ? rcAxis(hw.ch[RCF_STRAFE]) : 0;
  int speed = RC_DEFAULT_SPEED;
  if (hw.ch[RCF_SPEED]) {
    int pct = constrain((rcRaw(hw.ch[RCF_SPEED]) - 172) * 100 / 1639, 0, 100);
    speed = RC_MIN_SPEED + pct * (100 - RC_MIN_SPEED) / 100;
  }

  // Uzbrojenie tylko przy drążku w środku – robot nie wystrzeli po przełączeniu AUX
  if (!rcSwitch(hw.ch[RCF_ARM])) rcArmLatched = false;
  else if (!rcArmLatched && abs(turn) < 10 && abs(fwd) < 10 && abs(side) < 10) rcArmLatched = true;

  rcX = turn * speed / 100 * RC_TURN_GAIN / 100;
  rcY = fwd * speed / 100;
  rcS = side * speed / 100;
  rcState = rcArmLatched ? 2 : 1;

  // Światło: przełącznik działa przy każdej zmianie, telefon też może je przełączać
  if (hw.ch[RCF_LIGHT]) {
    int sw = rcSwitch(hw.ch[RCF_LIGHT]);
    if (sw != rcLastLightSw) lightOn = sw;
    rcLastLightSw = sw;
  }
}

// ---------- Komendy z przeglądarki ----------
// "M x y [s]" – ruch: skręt/obrót, przód/tył, bok (Mecanum); -100..100
// "L 0|1"  – światło
// "C k v"  – ustawienie obrazu, np. "C br 1" (nazwy w struct CamSettings)
// "H k v"  – napęd / piny / kanały aparatury, np. "H p0 14" (patrz hwSet)
// "V"      – podgląd kanałów aparatury (odpowiedź "V ...")
// "W k v"  – Wi-Fi (wartość zakodowana jak w URL), np. "W ss Moja%20siec", "W apply 1"
// "P"      – ping (podtrzymanie failsafe)

// zdefiniowane niżej, w części Wi-Fi
static void wifiSet(const char *key, const char *val);
static esp_err_t wifiHandler(httpd_req_t *req);
static esp_err_t scanHandler(httpd_req_t *req);

static void handleCommand(const char *cmd) {
  int a = 0, b = 0, c = 0;
  switch (cmd[0]) {
    case 'M': {
      int n = sscanf(cmd + 1, "%d %d %d", &a, &b, &c);
      if (n >= 2) {
        targetX = constrain(a, -100, 100);
        targetY = constrain(b, -100, 100);
        targetS = n == 3 ? constrain(c, -100, 100) : 0;
      }
      break;
    }
    case 'L':
      if (sscanf(cmd + 1, "%d", &a) == 1) lightOn = (a != 0);
      break;
    case 'C': {
      char key[8];
      if (sscanf(cmd + 1, "%7s %d", key, &a) == 2) camSet(key, a);
      break;
    }
    case 'H': {
      char key[8];
      if (sscanf(cmd + 1, "%7s %d", key, &a) == 2) hwSet(key, a);
      break;
    }
    case 'W': {
      char key[8], val[192];
      val[0] = 0;
      if (sscanf(cmd + 1, "%7s %191s", key, val) >= 1) wifiSet(key, val);
      break;
    }
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

// Aktualne ustawienia obrazu jako JSON – strona pobiera je przy starcie
static esp_err_t settingsHandler(httpd_req_t *req) {
  char json[256];
  snprintf(json, sizeof(json),
           "{\"fs\":%d,\"q\":%d,\"br\":%d,\"ct\":%d,\"sa\":%d,\"wb\":%d,\"ae\":%d,"
           "\"al\":%d,\"ex\":%d,\"fx\":%d,\"hm\":%d,\"vf\":%d,\"rot\":%d,\"psram\":%d}",
           cam.fs, cam.q, cam.br, cam.ct, cam.sa, cam.wb, cam.ae, cam.al, cam.ex, cam.fx,
           cam.hm, cam.vf, cam.rot, hasPsram ? 1 : 0);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

// Konfiguracja napędu i aparatury jako JSON (menu "Napęd" / "ELRS")
static esp_err_t hwHandler(httpd_req_t *req) {
  char json[400];
  const HwSettings &h = hw;
  snprintf(json, sizeof(json),
           "{\"mode\":%d,\"mot\":%d,\"p\":[%d,%d,%d,%d,%d,%d,%d,%d],\"inv\":[%d,%d,%d,%d],"
           "\"md\":%d,\"lp\":%d,\"rp\":%d,\"c\":[%d,%d,%d,%d,%d,%d],"
           "\"bootMode\":%d,\"pending\":%d,\"lightOk\":%d,\"rcOk\":%d,\"serialOff\":%d}",
           h.mode, h.motors, h.pin[0], h.pin[1], h.pin[2], h.pin[3], h.pin[4], h.pin[5], h.pin[6], h.pin[7],
           h.inv[0], h.inv[1], h.inv[2], h.inv[3], h.minDuty, h.lightPin, h.rcPin,
           h.ch[0], h.ch[1], h.ch[2], h.ch[3], h.ch[4], h.ch[5],
           hwBoot.mode, hwPending() ? 1 : 0, lightActive ? 1 : 0, rcActive ? 1 : 0, hwUsesUart0() ? 1 : 0);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
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

  uint8_t buf[200];                   // hasła Wi-Fi bywają długie
  if (frame.len >= sizeof(buf)) return ESP_FAIL;
  frame.payload = buf;
  ret = httpd_ws_recv_frame(req, &frame, frame.len);
  if (ret != ESP_OK) return ret;
  buf[frame.len] = 0;

  if (frame.type != HTTPD_WS_TYPE_TEXT) return ESP_OK;
  handleCommand((const char *)buf);

  char out[160];
  int olen;
  if (buf[0] == 'V') {
    // Podgląd aparatury: "V <aktywny> <połączona> <LQ> <kanał1 us> ... <kanał16 us>"
    olen = snprintf(out, sizeof(out), "V %d %d %d", rcActive ? 1 : 0, rcConnected() ? 1 : 0, rcLinkQuality);
    for (int i = 0; i < 16 && olen < (int)sizeof(out) - 6; i++)
      olen += snprintf(out + olen, sizeof(out) - olen, " %d", rcHaveFrame ? (rcCh[i] - CRSF_MID) * 5 / 8 + 1500 : 0);
  } else {
    // Stan: "R <aparatura> <światło> <fps> <KB na klatkę> <koła %: LP PP LT PT>"
    olen = snprintf(out, sizeof(out), "R %d %d %d %d %d %d %d %d", rcState, lightOn ? 1 : 0, streamFps, streamKB,
                    wheelOut[0], wheelOut[1], wheelOut[2], wheelOut[3]);
  }
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
    // Strumień ma wyższy priorytet niż loop(): bez tej pauzy od razu brałby mutex
    // z powrotem i restart kamery w loop() czekałby w nieskończoność.
    if (camPause) {
      delay(20);
      continue;
    }
    xSemaphoreTake(camMutex, portMAX_DELAY);  // restart kamery czeka, aż klatka zostanie wysłana
    if (!cameraOk) {
      xSemaphoreGive(camMutex);
      delay(100);
      continue;
    }
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      xSemaphoreGive(camMutex);
      Serial.println("Blad pobrania klatki");
      res = ESP_FAIL;
      break;
    }

    size_t hlen = snprintf(partBuf, sizeof(partBuf), STREAM_PART, fb->len);
    res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, partBuf, hlen);
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);

    size_t len = fb->len;
    esp_camera_fb_return(fb);
    xSemaphoreGive(camMutex);
    if (res != ESP_OK) break;         // klient się rozłączył
    frameCount = frameCount + 1;
    frameBytes = frameBytes + len;
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

  httpd_uri_t settingsUri;
  memset(&settingsUri, 0, sizeof(settingsUri));
  settingsUri.uri = "/settings";
  settingsUri.method = HTTP_GET;
  settingsUri.handler = settingsHandler;

  httpd_uri_t hwUri;
  memset(&hwUri, 0, sizeof(hwUri));
  hwUri.uri = "/hw";
  hwUri.method = HTTP_GET;
  hwUri.handler = hwHandler;

  httpd_uri_t wsUri;
  memset(&wsUri, 0, sizeof(wsUri));
  wsUri.uri = "/ws";
  wsUri.method = HTTP_GET;
  wsUri.handler = wsHandler;
  wsUri.is_websocket = true;

  if (httpd_start(&webHttpd, &config) == ESP_OK) {
    httpd_register_uri_handler(webHttpd, &indexUri);
    httpd_register_uri_handler(webHttpd, &settingsUri);
    httpd_register_uri_handler(webHttpd, &hwUri);
    httpd_uri_t wifiUri = { "/wifi", HTTP_GET, wifiHandler, NULL };
    httpd_uri_t scanUri = { "/scan", HTTP_GET, scanHandler, NULL };
    httpd_register_uri_handler(webHttpd, &wifiUri);
    httpd_register_uri_handler(webHttpd, &scanUri);
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

static bool cameraStart() {
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

  camInitFs = camWantedInitFs();
  if (hasPsram) {
    c.frame_size = FRAME_SIZES[camInitFs];
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

  cameraOk = true;
  camResetApplied();
  camApplyAll();
  Serial.printf("Kamera gotowa, bufor dla rozdzielczosci nr %d\n", camInitFs);
  return true;
}

// ---------- Wi-Fi ----------

struct WifiSettings {
  uint8_t mode;     // 0 – własna sieć robota (AP), 1 – sieć domowa (STA)
  uint8_t apKeep;   // w trybie sieci domowej zostaw też własną sieć robota
  uint8_t apCh;     // kanał własnej sieci 1..13
  String apSsid, apPass, staSsid, staPass, host;
};
static WifiSettings wifiCfg, wifiNew;      // aktywne / edytowane w menu
static volatile bool wifiApply = false;
static volatile bool wifiScanReq = false;
static bool wifiStaOk = false;

static void wifiDefaults(WifiSettings &w) {
  w.mode = 0; w.apKeep = 1; w.apCh = 1;
  w.apSsid = DEF_AP_SSID; w.apPass = DEF_AP_PASS;
  w.staSsid = ""; w.staPass = ""; w.host = DEF_HOSTNAME;
}

static void wifiLoad() {
  wifiDefaults(wifiCfg);
  prefs.begin("wifi", true);
  if (prefs.isKey("mode")) {
    wifiCfg.mode = prefs.getUChar("mode", 0);
    wifiCfg.apKeep = prefs.getUChar("keep", 1);
    wifiCfg.apCh = prefs.getUChar("ch", 1);
    wifiCfg.apSsid = prefs.getString("as", DEF_AP_SSID);
    wifiCfg.apPass = prefs.getString("ap", DEF_AP_PASS);
    wifiCfg.staSsid = prefs.getString("ss", "");
    wifiCfg.staPass = prefs.getString("sp", "");
    wifiCfg.host = prefs.getString("hn", DEF_HOSTNAME);
  }
  prefs.end();
  wifiNew = wifiCfg;
}

static void wifiSave(const WifiSettings &w) {
  prefs.begin("wifi", false);
  prefs.putUChar("mode", w.mode);
  prefs.putUChar("keep", w.apKeep);
  prefs.putUChar("ch", w.apCh);
  prefs.putString("as", w.apSsid);
  prefs.putString("ap", w.apPass);
  prefs.putString("ss", w.staSsid);
  prefs.putString("sp", w.staPass);
  prefs.putString("hn", w.host);
  prefs.end();
}

// Awaryjny reset Wi-Fi: 3 starty z rzędu, każdy krótszy niż 10 s
static void wifiBootCounter() {
  prefs.begin("boot", false);
  uint8_t n = prefs.getUChar("n", 0) + 1;
  prefs.putUChar("n", n >= 3 ? 0 : n);
  prefs.end();
  if (n >= 3) {
    prefs.begin("wifi", false);
    prefs.clear();
    prefs.end();
    Serial.println("Awaryjny reset Wi-Fi - ustawienia domyslne");
  }
}

static void wifiStartAp() {
  const char *pass = wifiCfg.apPass.length() >= 8 ? wifiCfg.apPass.c_str() : NULL;   // krótsze = sieć otwarta
  WiFi.softAP(wifiCfg.apSsid.c_str(), pass, wifiCfg.apCh);
  Serial.printf("Siec robota: %s, otworz http://%s\n", wifiCfg.apSsid.c_str(), WiFi.softAPIP().toString().c_str());
}

static void wifiInit() {
  WiFi.persistent(false);
  WiFi.setHostname(wifiCfg.host.c_str());
  bool sta = wifiCfg.mode == 1 && wifiCfg.staSsid.length() > 0;

  if (sta) {
    WiFi.mode(wifiCfg.apKeep ? WIFI_AP_STA : WIFI_STA);
    if (wifiCfg.apKeep) wifiStartAp();
    WiFi.begin(wifiCfg.staSsid.c_str(), wifiCfg.staPass.c_str());
    Serial.printf("Laczenie z siecia %s", wifiCfg.staSsid.c_str());
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
      delay(300);
      Serial.print(".");
    }
    Serial.println();
    wifiStaOk = WiFi.status() == WL_CONNECTED;
    if (wifiStaOk) {
      Serial.printf("Polaczono, otworz http://%s albo http://%s.local\n",
                    WiFi.localIP().toString().c_str(), wifiCfg.host.c_str());
    } else if (!wifiCfg.apKeep) {
      Serial.println("Brak polaczenia - uruchamiam wlasna siec robota");
      WiFi.mode(WIFI_AP);
      wifiStartAp();
    }
  } else {
    WiFi.mode(WIFI_AP);
    wifiStartAp();
  }
  WiFi.setSleep(false);               // mniejsze opóźnienia sterowania
  if (MDNS.begin(wifiCfg.host.c_str())) MDNS.addService("http", "tcp", 80);
}

// Zmiana z menu Wi-Fi: "W k wartość" (wartość zakodowana jak w URL). Działa po "W apply 1".
static String urlDecode(const char *s) {
  String out;
  while (*s) {
    if (*s == '%' && isxdigit((unsigned char)s[1]) && isxdigit((unsigned char)s[2])) {
      char hex[3] = { s[1], s[2], 0 };
      out += (char)strtol(hex, NULL, 16);
      s += 3;
    } else {
      out += *s++;
    }
  }
  return out;
}

static void wifiSet(const char *key, const char *val) {
  String v = urlDecode(val);
  int n = v.toInt();
  if      (!strcmp(key, "mode")) wifiNew.mode = n ? 1 : 0;
  else if (!strcmp(key, "keep")) wifiNew.apKeep = n ? 1 : 0;
  else if (!strcmp(key, "ch"))   wifiNew.apCh = constrain(n, 1, 13);
  else if (!strcmp(key, "as"))   { if (v.length() >= 1 && v.length() <= 32) wifiNew.apSsid = v; }
  else if (!strcmp(key, "ap"))   { if (v.length() >= 8 && v.length() <= 63) wifiNew.apPass = v; }
  else if (!strcmp(key, "ss"))   { if (v.length() <= 32) wifiNew.staSsid = v; }
  else if (!strcmp(key, "sp"))   { if (v.length() <= 63) wifiNew.staPass = v; }
  else if (!strcmp(key, "hn"))   { if (v.length() >= 1 && v.length() <= 24) wifiNew.host = v; }
  else if (!strcmp(key, "scan")) wifiScanReq = true;
  else if (!strcmp(key, "apply")) wifiApply = true;
  else if (!strcmp(key, "reset")) { wifiDefaults(wifiNew); wifiApply = true; }
}

static String jsonStr(const String &s) {
  String o = "\"";
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c < 0x20) o += ' ';
    else o += c;
  }
  return o + "\"";
}

// Ustawienia Wi-Fi i stan połączenia (hasła nie są wysyłane)
static esp_err_t wifiHandler(httpd_req_t *req) {
  const WifiSettings &w = wifiCfg;
  String j = "{\"mode\":" + String(w.mode) + ",\"keep\":" + String(w.apKeep) + ",\"ch\":" + String(w.apCh) +
             ",\"as\":" + jsonStr(w.apSsid) + ",\"ss\":" + jsonStr(w.staSsid) + ",\"hn\":" + jsonStr(w.host) +
             ",\"apOn\":" + String((WiFi.getMode() & WIFI_AP) ? 1 : 0) +
             ",\"apIp\":\"" + WiFi.softAPIP().toString() + "\",\"apClients\":" + String(WiFi.softAPgetStationNum()) +
             ",\"staOk\":" + String(WiFi.status() == WL_CONNECTED ? 1 : 0) +
             ",\"staIp\":\"" + WiFi.localIP().toString() + "\",\"rssi\":" + String(WiFi.RSSI()) + "}";
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, j.c_str(), j.length());
}

// Wynik skanowania sieci: {"busy":1} albo {"list":[{"s":"nazwa","r":-60,"o":0}, ...]}
static esp_err_t scanHandler(httpd_req_t *req) {
  int n = WiFi.scanComplete();
  String j;
  if (n == WIFI_SCAN_RUNNING || wifiScanReq) j = "{\"busy\":1}";
  else {
    j = "{\"list\":[";
    for (int i = 0; i < n && i < 20; i++) {
      if (i) j += ",";
      j += "{\"s\":" + jsonStr(WiFi.SSID(i)) + ",\"r\":" + String(WiFi.RSSI(i)) +
           ",\"o\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? 1 : 0) + "}";
    }
    j += "]}";
  }
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, j.c_str(), j.length());
}

// ============================================================

void setup() {
  // Wyłącz detektor brown-out – rozruch silników potrafi chwilowo obniżyć napięcie
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  Serial.println("\nESP32-CAM Robot");

  hwLoad();
  Serial.printf("Naped: %s, piny silnikow:", hwBoot.mode == MODE_MECANUM ? "Mecanum" : "czolg");
  for (int i = 0; i < motorPinCount(hwBoot); i++) Serial.printf(" %d", hwBoot.pin[i]);
  Serial.printf("; dioda: %d%s; ELRS: %d%s\n", hwBoot.lightPin, lightActive ? "" : " (wylaczona)",
                hwBoot.rcPin, rcActive ? "" : " (wylaczony)");
  if (hwUsesUart0()) {
    Serial.println("Piny 1/3 sa uzyte przez naped/diode/ELRS - Serial Monitor zostaje wylaczony");
    Serial.flush();
    Serial.end();
  }

  motorsInit();
  rcInit();

  camMutex = xSemaphoreCreateMutex();
  hasPsram = psramFound();
  camLoad();
  if (!cameraStart()) {
    Serial.println("Kamera nie dziala - sterowanie bedzie dzialac bez obrazu");
  }

  wifiBootCounter();
  wifiLoad();
  wifiInit();
  startServers();
}

void loop() {
  rcPoll();
  rcUpdate();

  int x = 0, y = 0, sd = 0;
  if (rcState == 2) {
    // aparatura połączona i uzbrojona – ma pierwszeństwo
    x = rcX;
    y = rcY;
    sd = rcS;
  } else if (millis() - lastCmdMs <= FAILSAFE_MS) {
    // telefon; bez świeżych komend (failsafe) robot stoi
    x = targetX;
    y = targetY;
    sd = targetS;
  }
  drive(x, y, sd);

  bool l = lightOn;
  if (l != lightApplied) {
    if (lightActive) pwmWrite(hwBoot.lightPin, CH_LIGHT, l ? LIGHT_DUTY : 0);
    lightApplied = l;
  }

  // Konfiguracja napędu: zapis 1 s po ostatniej zmianie; restart na żądanie z menu
  if (hwDirty && (millis() - hwDirtyMs > 1000 || rebootRequested)) {
    hwDirty = false;
    hwSave();
  }
  // Robot działa 10 s – to nie był szybki restart, zeruj licznik awaryjnego resetu Wi-Fi
  static bool bootCleared = false;
  if (!bootCleared && millis() > 10000) {
    bootCleared = true;
    prefs.begin("boot", false);
    prefs.putUChar("n", 0);
    prefs.end();
  }
  if (wifiScanReq) {
    wifiScanReq = false;
    WiFi.scanNetworks(true);          // asynchronicznie, wynik pod /scan
  }
  if (wifiApply) {
    wifiSave(wifiNew);
    rebootRequested = true;
  }

  if (rebootRequested) {
    drive(0, 0);
    if (hwDirty) hwSave();            // nie zgub niezapisanych zmian
    if (camDirty) camSave();
    delay(300);                       // czas na wysłanie odpowiedzi do przeglądarki
    ESP.restart();
  }

  // Restart kamery z innym buforem (zmiana na / z 1024x768 lub HD)
  // loop() nigdy nie czeka dłużej niż 1 s – tu działają też silniki, światło i failsafe
  if (camNeedReinit) {
    drive(0, 0);                      // na czas restartu kamery robot stoi
    camPause = true;
    if (xSemaphoreTake(camMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
      camNeedReinit = false;
      cameraOk = false;
      esp_camera_deinit();
      cameraStart();
      xSemaphoreGive(camMutex);
    }
    camPause = false;
  }

  // FPS i średni rozmiar klatki strumienia
  static unsigned long fpsMs = 0;
  static uint32_t fpsFrames = 0, fpsBytes = 0;
  if (millis() - fpsMs >= 1000) {
    uint32_t f = frameCount, b = frameBytes;
    streamFps = f - fpsFrames;
    streamKB = streamFps ? (b - fpsBytes) / streamFps / 1024 : 0;
    fpsFrames = f;
    fpsBytes = b;
    fpsMs = millis();
    static int dbg = 0;
    if (streamFps && ++dbg % 5 == 0) Serial.printf("Strumien: %d fps, %d KB/klatke\n", streamFps, streamKB);
  }

  // Zapis ustawień obrazu 3 s po ostatniej zmianie (oszczędza pamięć flash)
  if (camDirty && millis() - camDirtyMs > 3000) {
    camDirty = false;
    camSave();
  }

  delay(2);
}
