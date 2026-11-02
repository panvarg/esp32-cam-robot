/*
 * ESP32-CAM + L298N – robot zdalnie sterowany z telefonu.
 *
 *  - ESP32 tworzy własną sieć Wi-Fi lub łączy się z domową (menu ⚙ → Wi-Fi)
 *  - strona www z wirtualnym analogiem (joystick jak w grach) + podgląd z kamery
 *  - sterowanie przez WebSocket (niskie opóźnienia), obraz MJPEG na porcie 81
 *  - failsafe: silniki stają, gdy przez ustawiony czas nie przyjdzie żadna komenda
 *  - opcjonalnie odbiornik ExpressLRS (CRSF) – aparatura ma pierwszeństwo,
 *    gdy jest połączona i uzbrojona; w pozostałych przypadkach steruje telefon
 *  - serwa (np. kamera pan/tilt) i przyciski z przypisywanymi funkcjami;
 *    opcjonalny moduł PCA9685 (I2C) daje 16 dodatkowych wyjść
 *  - napęd czołgowy (2 lub 4 silniki) albo Mecanum (4 silniki, 8 pinów);
 *    piny, napęd i kanały aparatury ustawia się na stronie (⚙ → Napęd / ELRS)
 *
 * Płytka w Arduino IDE: "AI Thinker ESP32-CAM", Partition Scheme: "Huge APP".
 * Wymagany pakiet esp32 by Espressif w wersji 2.x lub 3.x.
 */

#include <WiFi.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <Wire.h>
#include "esp_ota_ops.h"
#include "esp_arduino_version.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include "driver/uart.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#if __has_include("version.h")
#include "version.h"                     // tworzony przy wydaniu (GitHub Actions)
#endif
#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

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

// Domyślne odbicie obrazu (później można je zmienić w menu ustawień na stronie)
#define CAM_VFLIP   false
#define CAM_HMIRROR false

#define PWM_RES      8     // rozdzielczość 8 bit -> 0..255
#define RC_DEFAULT_SPEED 70  // moc z aparatury, gdy kanał mocy nie jest przypisany, %
// Pozostałe parametry jazdy, diody i aparatury są w menu (HW_DEFAULTS niżej)

// ============================================================

// ---------- Napęd i piny (menu "Napęd" / "ELRS", zapisywane w pamięci ESP) ----------
// Wolne piny ESP32-CAM (AI-Thinker); reszta jest zajęta przez kamerę i PSRAM.
static const int8_t FREE_PINS[] = { 1, 2, 3, 4, 12, 13, 14, 15 };

#define HW_SETTINGS_VERSION 2
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
  // --- od wersji 2 (nowe pola zawsze na końcu – starsze zapisy dostają tu wartości domyślne) ---
  int16_t pwmFreq;    // częstotliwość PWM silników, Hz (po restarcie)
  int16_t failsafeMs; // stop, gdy z telefonu nie przychodzą komendy przez tyle ms
  int16_t rcTimeout;  // brak ramek z odbiornika przez tyle ms = brak połączenia
  uint16_t rcRev;     // odwrócone kanały aparatury, bit 0 = CH1
  uint8_t turnGain;   // czułość skrętu / obrotu, %
  uint8_t maxPower;   // ograniczenie mocy kół, %
  uint8_t accel;      // płynny rozruch: czas od 0 do 100% mocy w 0,1 s (0 = wyłączony)
  uint8_t lightPct;   // jasność diody, %
  uint8_t stickDz;    // martwa strefa gałek na telefonie, %
  uint8_t rcDz;       // martwa strefa drążków aparatury, %
  uint8_t rcMinSpeed; // moc z aparatury przy gazie na dole, %
  uint8_t driver;     // sterownik silników: 0 – L298N, 1 – TB6612FNG (tylko opisy i schemat na stronie)
  int8_t  sda, scl;   // piny I2C modułu PCA9685 (-1 = brak modułu)
  uint8_t pcaAddr;    // adres I2C PCA9685
  uint16_t pcaFreq;   // częstotliwość PWM PCA9685, Hz – wspólna dla 16 kanałów (serwa: 50 Hz)
};
#define HW_V1_SIZE offsetof(HwSettings, pwmFreq)
static const HwSettings HW_DEFAULTS = {
  HW_SETTINGS_VERSION, MODE_TANK, 2,
  { 14, 15, 13, 12, 2, 4, 1, 3 },
  { 0, 0, 0, 0 },
  90,
  4,    // dioda flash na płytce
  2,    // TX odbiornika ELRS
  { 1, 2, 4, 3, 5, 6 },  // skręt CH1, jazda CH2, bok CH4, moc CH3 (gaz), uzbrojenie CH5, światło CH6
  1000,  // PWM Hz – dla L298N 500..2000 Hz działa dobrze
  600,   // failsafe telefonu, ms
  250,   // utrata sygnału aparatury, ms
  0,     // brak odwróconych kanałów
  75,    // czułość skrętu, %
  100,   // maks. moc, %
  0,     // płynny rozruch wyłączony
  25,    // jasność diody, % (pełna moc mocno grzeje i oślepia)
  8,     // martwa strefa gałek, %
  5,     // martwa strefa aparatury, %
  25,    // moc z aparatury przy gazie na dole, %
  0,     // L298N
  -1, -1, 0x40, 50   // bez modułu PCA9685
};
static HwSettings hw = HW_DEFAULTS;   // edytowane z menu
static HwSettings hwBoot;             // aktywne od startu (piny i tryb zmieniają się po restarcie)
static volatile bool hwDirty = false;
static volatile unsigned long hwDirtyMs = 0;
static volatile bool rebootRequested = false;
static bool lightActive = false;      // pin diody jest ustawiony i nie koliduje z silnikami
static bool rcActive = false;         // jw. dla odbiornika

// ---------- Wi-Fi (menu "Wi-Fi", zapisywane w pamięci ESP) ----------
// Struktura musi być tutaj, przed pierwszą funkcją – Arduino IDE wstawia tam deklaracje funkcji.
struct WifiSettings {
  uint8_t mode;     // 0 – własna sieć robota (AP), 1 – sieć domowa (STA)
  uint8_t apKeep;   // w trybie sieci domowej zostaw też własną sieć robota
  uint8_t apCh;     // kanał własnej sieci 1..13
  String apSsid, apPass, staSsid, staPass, host;
};

// Kanały LEDC: 0/1 (timer 0) zajmuje kamera (XCLK); 8/9 omijamy na wszelki wypadek
// ---------- Serwa i przyciski (menu "Serwa" / "Przyciski", zapisywane w pamięci ESP) ----------
// Wyjście (silnik, dioda, serwo, przycisk) to pin ESP32 albo kanał PCA9685 zapisany jako PCA_BASE + kanał.
#define PCA_BASE    100
#define SERVO_COUNT 6
#define ACT_COUNT   8
enum { SC_NONE, SC_PAN, SC_TILT, SC_RC };                 // sterowanie serwem
enum { ACT_NONE, ACT_LIGHT, ACT_OUTPUT, ACT_SERVO, ACT_CENTER, ACT_POWER, ACT_TURBO, ACT_TYPES };

struct ServoCfg {
  int8_t  out;       // -1 = brak
  uint8_t minA, maxA, center;   // stopnie 0..180
  uint8_t inv;       // odwrócony kierunek
  uint8_t speed;     // prędkość ruchu w 10°/s (0 = natychmiast)
  uint8_t ctrl;      // SC_*: przeciąganie po obrazie poziomo / pionowo albo kanał aparatury
  uint8_t ch;        // kanał aparatury dla SC_RC
  char    name[12];
};
struct ActCfg {
  uint8_t type;      // ACT_*
  uint8_t mode;      // 0 – przełącznik, 1 – działa tylko przytrzymany
  int8_t  out;       // wyjście dla ACT_OUTPUT
  uint8_t servo;     // serwo dla ACT_SERVO
  uint8_t a, b;      // ACT_SERVO: pozycja po włączeniu / wyłączeniu (°); ACT_POWER: moc %
  uint8_t ch;        // kanał aparatury, który też wyzwala akcję (0 = brak)
  uint8_t show;      // przycisk na ekranie
  char    label[8];  // napis na przycisku (UTF-8, np. emoji)
};
#define IO_SETTINGS_VERSION 1
struct IoSettings {
  int8_t   ver;
  ServoCfg s[SERVO_COUNT];
  ActCfg   a[ACT_COUNT];
};

static const uint8_t MOTOR_CH[8] = { 4, 5, 6, 7, 10, 11, 12, 13 };
static const uint8_t SERVO_CH[2] = { 14, 15 };   // kanały LEDC dla serw na pinach ESP32 (50 Hz, osobny timer)
#define CH_LIGHT 2

static volatile int targetX = 0;            // -100..100 (skręt / obrót)
static volatile int targetY = 0;            // -100..100 (przód/tył)
static volatile int targetS = 0;            // -100..100 (jazda bokiem, tylko Mecanum)
static volatile unsigned long lastCmdMs = 0;
static volatile bool lightOn = false;

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

#define CAM_SETTINGS_VERSION 2
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
  // --- od wersji 2 (nowe pola na końcu – starsze zapisy dostają tu wartości domyślne) ---
  int8_t  xclk; // taktowanie kamery, MHz: 20 lub 10 (mniej zakłóceń, mniej klatek)
  int8_t  gc;   // limit wzmocnienia 0..6 (2x..128x), -1 = domyślny czujnika
  int8_t  bpc;  // korekcja „martwych” (czarnych) pikseli
  int8_t  wpc;  // korekcja „gorących” (białych) pikseli
  int8_t  lenc; // korekcja winietowania obiektywu
};
#define CAM_V1_SIZE offsetof(CamSettings, xclk)
static const CamSettings CAM_DEFAULTS = {
  CAM_SETTINGS_VERSION, 2, 12, 0, 0, 0, 0, 1, 0, 300, 0, CAM_HMIRROR, CAM_VFLIP, 0,
  20, -1, 0, 1, 1     // wartości jak po starcie czujnika OV2640
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
static int camInitXclk = 0;
static volatile bool camForceReinit = false;   // przywrócenie ustawień czujnika (np. limit wzmocnienia na „domyślny”)
static volatile bool camNeedReinit = false;
static volatile bool camPause = false;     // strumień nie bierze nowych klatek (restart kamery)
static SemaphoreHandle_t camMutex = NULL;

static int camWantedInitFs() { return hasPsram ? max((int)cam.fs, FS_BASE_INIT) : 0; }

httpd_handle_t webHttpd = NULL;
httpd_handle_t streamHttpd = NULL;

// ---------- PWM (kompatybilne z pakietem esp32 2.x i 3.x) ----------

static void pwmAttach(uint8_t pin, uint8_t ch, uint32_t freq, uint8_t res = PWM_RES) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttachChannel(pin, freq, res, ch);
#else
  ledcSetup(ch, freq, res);
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

static bool isPcaOut(int p) { return p >= PCA_BASE && p < PCA_BASE + 16; }
static bool pcaConfigured(const HwSettings &h) { return isFreePin(h.sda) && isFreePin(h.scl) && h.sda != h.scl; }
static bool isOutPin(const HwSettings &h, int p) { return isFreePin(p) || (isPcaOut(p) && pcaConfigured(h)); }

static bool usedByMotors(const HwSettings &h, int p) {
  for (int i = 0; i < motorPinCount(h); i++) if (h.pin[i] == p) return true;
  return false;
}

// Piny silników: tylko wolne i każdy inny. Złe ustawienia -> domyślne (robot zawsze ruszy).
static bool hwValid(const HwSettings &h) {
  if (h.ver != HW_SETTINGS_VERSION || h.mode < 0 || h.mode > 1) return false;
  int n = motorPinCount(h);
  for (int i = 0; i < n; i++) {
    if (!isOutPin(h, h.pin[i])) return false;
    for (int j = 0; j < i; j++) if (h.pin[i] == h.pin[j]) return false;
    if (pcaConfigured(h) && (h.pin[i] == h.sda || h.pin[i] == h.scl)) return false;
  }
  for (int i = 0; i < RCF_COUNT; i++) if (h.ch[i] < 0 || h.ch[i] > 16) return false;
  return true;
}

// Zakresy nowych parametrów – poza zakresem zostają przycięte, nie odrzucamy całości
static void hwClamp(HwSettings &h) {
  h.pwmFreq = constrain(h.pwmFreq, 100, 20000);
  h.failsafeMs = constrain(h.failsafeMs, 200, 3000);
  h.rcTimeout = constrain(h.rcTimeout, 100, 2000);
  h.turnGain = constrain(h.turnGain, 20, 100);
  h.maxPower = constrain(h.maxPower, 20, 100);
  h.accel = constrain(h.accel, 0, 30);
  h.lightPct = constrain(h.lightPct, 1, 100);
  h.stickDz = constrain(h.stickDz, 0, 30);
  h.rcDz = constrain(h.rcDz, 0, 30);
  h.rcMinSpeed = constrain(h.rcMinSpeed, 0, 100);
  if (h.driver > 1) h.driver = 0;
  if (h.pcaAddr < 0x40 || h.pcaAddr > 0x7F) h.pcaAddr = 0x40;
  h.pcaFreq = constrain(h.pcaFreq, 24, 1500);
}

// ---------- Przydział pinów i kanałów PCA (każde wyjście może mieć tylko jedną funkcję) ----------
static bool gpioUsed[40];
static bool pcaUsed[16];

static bool claimOut(int p) {
  if (isFreePin(p)) {
    if (gpioUsed[p]) return false;
    gpioUsed[p] = true;
    return true;
  }
  if (isPcaOut(p) && pcaConfigured(hwBoot)) {
    if (pcaUsed[p - PCA_BASE]) return false;
    pcaUsed[p - PCA_BASE] = true;
    return true;
  }
  return false;
}

static void hwLoad() {
  prefs.begin("hw", true);
  HwSettings tmp = HW_DEFAULTS;
  size_t len = prefs.getBytesLength("s");
  if (len > 0 && len <= sizeof(tmp)) prefs.getBytes("s", &tmp, len);
  if (len == HW_V1_SIZE && tmp.ver == 1) tmp.ver = HW_SETTINGS_VERSION;   // zapis z v1.2: nowe pola domyślne
  if (len > 0 && hwValid(tmp)) {
    hwClamp(tmp);
    hw = tmp;
  } else if (len > 0) {
    Serial.println("Bledna konfiguracja napedu - uzywam domyslnej");
  }
  prefs.end();
  hwBoot = hw;
  // kolejność ważności: I2C modułu PCA, silniki, dioda, odbiornik (serwa i przyciski – w ioLoad)
  memset(gpioUsed, 0, sizeof(gpioUsed));
  memset(pcaUsed, 0, sizeof(pcaUsed));
  if (pcaConfigured(hwBoot)) { claimOut(hwBoot.sda); claimOut(hwBoot.scl); }
  for (int i = 0; i < motorPinCount(hwBoot); i++) claimOut(hwBoot.pin[i]);
  lightActive = claimOut(hwBoot.lightPin);
  rcActive = isFreePin(hwBoot.rcPin) && claimOut(hwBoot.rcPin);
}

static void hwSave() {
  prefs.begin("hw", false);
  prefs.putBytes("s", &hw, sizeof(hw));
  prefs.end();
  Serial.println("Zapisano konfiguracje napedu");
}

// Czy jakiś aktywny pin to 1 lub 3 (UART0) – wtedy Serial Monitor musi zostać wyłączony
static bool hwUsesUart0() { return gpioUsed[1] || gpioUsed[3]; }

// ---------- Wyjścia: pin ESP32 (PWM LEDC) albo kanał modułu PCA9685 ----------
static bool pcaOk = false;            // moduł odpowiedział na I2C
static uint16_t pcaCache[16];

static void pcaReg(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(hwBoot.pcaAddr);
  Wire.write(reg);
  Wire.write(v);
  Wire.endTransmission();
}

// v: 0..4095 wypełnienia, 4096 = stale włączone. Zapis tylko przy zmianie (I2C jest wolne).
static void pcaSet(int ch, uint16_t v) {
  if (!pcaOk || ch < 0 || ch > 15 || pcaCache[ch] == v) return;
  pcaCache[ch] = v;
  uint16_t on = 0, off = v;
  if (v >= 4096) { on = 0x1000; off = 0; }
  else if (v == 0) off = 0x1000;
  Wire.beginTransmission(hwBoot.pcaAddr);
  Wire.write(0x06 + 4 * ch);          // LEDn_ON_L
  Wire.write(on & 0xFF);
  Wire.write(on >> 8);
  Wire.write(off & 0xFF);
  Wire.write(off >> 8);
  Wire.endTransmission();
}

static bool pcaInit() {
  if (!pcaConfigured(hwBoot)) return false;
  Wire.begin(hwBoot.sda, hwBoot.scl, 400000);
  Wire.beginTransmission(hwBoot.pcaAddr);
  if (Wire.endTransmission() != 0) {
    Serial.printf("PCA9685 nie odpowiada pod adresem 0x%02X\n", hwBoot.pcaAddr);
    return false;
  }
  int pre = constrain((int)roundf(25000000.0f / (4096.0f * hwBoot.pcaFreq)) - 1, 3, 255);
  pcaReg(0x00, 0x10);                 // MODE1: uśpienie – wymagane przy zmianie preskalera
  pcaReg(0xFE, pre);                  // PRE_SCALE
  pcaReg(0x00, 0x20);                 // MODE1: praca, auto-inkrementacja rejestrów
  delay(1);
  pcaReg(0x00, 0xA0);                 // restart PWM
  pcaReg(0x01, 0x04);                 // MODE2: wyjścia push-pull
  pcaOk = true;
  for (int i = 0; i < 16; i++) { pcaCache[i] = 0xFFFF; pcaSet(i, 0); }
  Serial.printf("PCA9685 gotowy, %d Hz\n", hwBoot.pcaFreq);
  return true;
}

// Wypełnienie 0..255 na wyjście: pin ESP32 (z kanałem LEDC) albo kanał PCA
static void outDuty(int p, uint8_t ledcCh, int duty) {
  if (isPcaOut(p)) pcaSet(p - PCA_BASE, duty >= 255 ? 4096 : duty * 16);
  else if (isFreePin(p)) pwmWrite(p, ledcCh, duty);
}

// ---------- Silniki ----------

// silnik nr i (0..3), speed: -255..255
static void setMotor(int i, int speed) {
  int a = hwBoot.pin[2 * i], b = hwBoot.pin[2 * i + 1];
  outDuty(a, MOTOR_CH[2 * i], speed > 0 ? speed : 0);
  outDuty(b, MOTOR_CH[2 * i + 1], speed < 0 ? -speed : 0);
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

static float wheelCur[4];          // aktualna moc kół (z płynnym rozruchem), %

// x – skręt/obrót, y – przód/tył, s – jazda bokiem (Mecanum); wszystko -100..100.
// now = true: bez płynnego rozruchu (failsafe, restart kamery).
static void drive(int x, int y, int s = 0, bool now = false) {
  static unsigned long lastMs = 0;
  unsigned long t = millis();
  float step = hw.accel ? (t - lastMs) / (float)hw.accel : 1000;   // % na ms: 100% w accel*100 ms
  lastMs = t;
  x = x * hw.turnGain / 100;

  int n = hwBoot.mode == MODE_MECANUM ? 4 : 2;
  int w[4] = { 0, 0, 0, 0 };
  if (n == 4) {
    // kolejność: lewe przednie, prawe przednie, lewe tylne, prawe tylne
    w[0] = y + s + x; w[1] = y - s - x; w[2] = y - s + x; w[3] = y + s - x;
  } else {
    // "arcade drive": skręt czołgowy, lewa i prawa strona
    w[0] = y + x; w[1] = y - x;
  }
  normalize(w, n);
  for (int i = 0; i < 4; i++) {
    float target = i < n ? w[i] * hw.maxPower / 100.0f : 0;
    if (now) wheelCur[i] = target;
    else wheelCur[i] += constrain(target - wheelCur[i], -step, step);
    int v = (int)roundf(wheelCur[i]);
    wheelOut[i] = v;
    if (i < n) setMotor(i, toDuty(hw.inv[i] ? -v : v));
  }
}

static void motorsInit() {
  for (int i = 0; i < motorPinCount(hwBoot); i++) {
    int p = hwBoot.pin[i];
    if (!isFreePin(p)) continue;      // kanał PCA – bez konfiguracji pinu
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);
    pwmAttach(p, MOTOR_CH[i], hwBoot.pwmFreq);
  }
  drive(0, 0, 0, true);

  if (lightActive && isFreePin(hwBoot.lightPin)) pwmAttach(hwBoot.lightPin, CH_LIGHT, 5000);
  if (lightActive) outDuty(hwBoot.lightPin, CH_LIGHT, 0);
}

// Zmiana z menu "Napęd" / "ELRS", np. hwSet("p3", 13). Piny i tryb działają po restarcie.
static bool hwSet(const char *key, int v) {
  if      (!strcmp(key, "mode")) hw.mode = v ? MODE_MECANUM : MODE_TANK;
  else if (!strcmp(key, "mot"))  hw.motors = v == 4 ? 4 : 2;
  else if (key[0] == 'p' && key[1] >= '0' && key[1] <= '7' && !key[2]) hw.pin[key[1] - '0'] = (isFreePin(v) || isPcaOut(v)) ? v : -1;
  else if (!strncmp(key, "inv", 3) && key[3] >= '0' && key[3] <= '3' && !key[4]) hw.inv[key[3] - '0'] = v ? 1 : 0;
  else if (!strcmp(key, "md"))   hw.minDuty = constrain(v, 0, 200);
  else if (!strcmp(key, "lp"))   hw.lightPin = (isFreePin(v) || isPcaOut(v)) ? v : -1;
  else if (!strcmp(key, "rp"))   hw.rcPin = isFreePin(v) ? v : -1;
  else if (key[0] == 'c' && key[1] >= '0' && key[1] < '0' + RCF_COUNT && !key[2]) hw.ch[key[1] - '0'] = constrain(v, 0, 16);
  else if (!strcmp(key, "pf"))   hw.pwmFreq = v;
  else if (!strcmp(key, "fo"))   hw.failsafeMs = v;
  else if (!strcmp(key, "rt"))   hw.rcTimeout = v;
  else if (!strcmp(key, "rv"))   hw.rcRev = v & 0xFFFF;
  else if (!strcmp(key, "tg"))   hw.turnGain = v;
  else if (!strcmp(key, "mp"))   hw.maxPower = v;
  else if (!strcmp(key, "ac"))   hw.accel = constrain(v, 0, 30);
  else if (!strcmp(key, "lt"))   hw.lightPct = constrain(v, 1, 100);
  else if (!strcmp(key, "dz"))   hw.stickDz = constrain(v, 0, 30);
  else if (!strcmp(key, "rd"))   hw.rcDz = constrain(v, 0, 30);
  else if (!strcmp(key, "rm"))   hw.rcMinSpeed = constrain(v, 0, 100);
  else if (!strcmp(key, "drv"))  hw.driver = v ? 1 : 0;
  else if (!strcmp(key, "sda"))  hw.sda = isFreePin(v) ? v : -1;
  else if (!strcmp(key, "scl"))  hw.scl = isFreePin(v) ? v : -1;
  else if (!strcmp(key, "pa"))   hw.pcaAddr = v;
  else if (!strcmp(key, "pq"))   hw.pcaFreq = v;
  else if (!strcmp(key, "reset")) hw = HW_DEFAULTS;
  else if (!strcmp(key, "reboot")) { rebootRequested = true; return true; }
  else return false;
  hwClamp(hw);
  hwDirty = true;
  hwDirtyMs = millis();
  return true;
}

// Czy zmiany w menu wymagają restartu (piny, tryb napędu, piny diody / odbiornika)
static bool hwPending() {
  if (hw.mode != hwBoot.mode || hw.lightPin != hwBoot.lightPin || hw.rcPin != hwBoot.rcPin ||
      hw.pwmFreq != hwBoot.pwmFreq || hw.sda != hwBoot.sda || hw.scl != hwBoot.scl ||
      hw.pcaAddr != hwBoot.pcaAddr || hw.pcaFreq != hwBoot.pcaFreq) return true;
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

// Wywołanie funkcji czujnika tylko wtedy, gdy dany model ją ma (OV2640, OV3660, OV5640 różnią się)
#define CAM_CALL(fn, ...) do { if (s->fn) s->fn(s, __VA_ARGS__); } while (0)

static void camApplyAll() {
  if (!cameraOk) return;
  sensor_t *s = esp_camera_sensor_get();
  if (!s) return;
  CamSettings &a = camApplied;
  // nigdy większa niż bufor z inicjalizacji; bez PSRAM tylko najmniejsza
  int fs = hasPsram ? min((int)cam.fs, camInitFs) : 0;
  if (fs != a.fs)        CAM_CALL(set_framesize, FRAME_SIZES[fs]);
  if (cam.q != a.q)      CAM_CALL(set_quality, cam.q);
  if (cam.br != a.br)    CAM_CALL(set_brightness, cam.br);
  if (cam.ct != a.ct)    CAM_CALL(set_contrast, cam.ct);
  if (cam.sa != a.sa)    CAM_CALL(set_saturation, cam.sa);
  if (cam.wb != a.wb)    CAM_CALL(set_wb_mode, cam.wb);
  if (cam.ae != a.ae)    CAM_CALL(set_exposure_ctrl, cam.ae);
  if (cam.al != a.al)    CAM_CALL(set_ae_level, cam.al);
  if (!cam.ae && (cam.ex != a.ex || cam.ae != a.ae)) CAM_CALL(set_aec_value, cam.ex);
  if (cam.fx != a.fx)    CAM_CALL(set_special_effect, cam.fx);
  if (cam.hm != a.hm)    CAM_CALL(set_hmirror, cam.hm);
  if (cam.vf != a.vf)    CAM_CALL(set_vflip, cam.vf);
  if (cam.gc != a.gc && cam.gc >= 0) CAM_CALL(set_gainceiling, (gainceiling_t)cam.gc);
  if (cam.bpc != a.bpc)  CAM_CALL(set_bpc, cam.bpc);
  if (cam.wpc != a.wpc)  CAM_CALL(set_wpc, cam.wpc);
  if (cam.lenc != a.lenc) CAM_CALL(set_lenc, cam.lenc);
  a = cam;
  a.fs = fs;
}

// ---------- Kamera: wykryty model ----------
static uint16_t camPid = 0;
static bool camFresh = false;         // brak zapisanych ustawień obrazu – użyj domyślnych dla modelu

static const char *camName() {
  switch (camPid) {
    case OV2640_PID: return "OV2640";
    case OV3660_PID: return "OV3660";
    case OV5640_PID: return "OV5640";
    case 0:          return "brak";
    default:         return "inna";
  }
}

// Domyślne ustawienia obrazu dla wykrytej kamery. OV3660 daje obraz odwrócony w pionie
// i przesycone kolory – poprawki jak w przykładzie CameraWebServer od Espressif.
static CamSettings camDefaultsFor() {
  CamSettings d = CAM_DEFAULTS;
  if (camPid == OV3660_PID) { d.vf = 1; d.br = 1; d.sa = -2; }
  return d;
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
  else if (!strcmp(key, "xc")) cam.xclk = v == 10 ? 10 : 20;
  else if (!strcmp(key, "gc")) {
    int nv = constrain(v, -1, 6);
    if (nv == -1 && cam.gc != -1) camForceReinit = true;   // „domyślny” = świeży start czujnika
    cam.gc = nv;
  }
  else if (!strcmp(key, "bpc")) cam.bpc = v ? 1 : 0;
  else if (!strcmp(key, "wpc")) cam.wpc = v ? 1 : 0;
  else if (!strcmp(key, "lenc")) cam.lenc = v ? 1 : 0;
  else if (!strcmp(key, "reset")) { cam = camDefaultsFor(); camForceReinit = true; }
  else return false;
  // inny bufor, taktowanie albo powrót do ustawień czujnika -> restart kamery w loop()
  if (camWantedInitFs() != camInitFs || cam.xclk != camInitXclk || camForceReinit) {
    camForceReinit = false;
    camNeedReinit = true;
  }
  camApplyAll();
  camDirty = true;                    // zapis do pamięci w loop(), z opóźnieniem
  camDirtyMs = millis();
  return true;
}

static void camLoad() {
  prefs.begin("cam", true);
  CamSettings tmp = CAM_DEFAULTS;
  size_t len = prefs.getBytesLength("s");
  if (len > 0 && len <= sizeof(tmp)) prefs.getBytes("s", &tmp, len);
  if (len == CAM_V1_SIZE && tmp.ver == 1) tmp.ver = CAM_SETTINGS_VERSION;   // zapis z v1.1/v1.2
  if (len > 0 && tmp.ver == CAM_SETTINGS_VERSION) {
    if (tmp.xclk != 10) tmp.xclk = 20;
    cam = tmp;
  } else {
    camFresh = true;                  // ustawienia domyślne dobierzemy po wykryciu kamery
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

// Wartość kanału z uwzględnieniem odwrócenia (lustrzane odbicie wokół środka)
static int rcRaw(int ch) {
  if (ch < 1 || ch > 16) return CRSF_MID;
  int v = rcCh[ch - 1];
  return (hw.rcRev >> (ch - 1)) & 1 ? 2 * CRSF_MID - v : v;
}

static int rcAxis(int ch) {               // -100..100 %
  if (ch == 0) return 0;
  int v = constrain((rcRaw(ch) - CRSF_MID) * 100 / CRSF_SPAN, -100, 100);
  return abs(v) < hw.rcDz ? 0 : v;
}

static bool rcSwitch(int ch) { return ch != 0 && rcRaw(ch) > 1300; }   // > ~1700 us

static bool rcConnected() {
  return rcActive && rcHaveFrame && rcLinkQuality > 0 && millis() - rcLastFrameMs < (unsigned long)hw.rcTimeout;
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
    speed = hw.rcMinSpeed + pct * (100 - hw.rcMinSpeed) / 100;
  }

  // Uzbrojenie tylko przy drążku w środku – robot nie wystrzeli po przełączeniu AUX
  if (!rcSwitch(hw.ch[RCF_ARM])) rcArmLatched = false;
  else if (!rcArmLatched && abs(turn) < 10 && abs(fwd) < 10 && abs(side) < 10) rcArmLatched = true;

  rcX = turn * speed / 100;           // czułość skrętu stosuje drive()
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

// ---------- Serwa i przyciski ----------

static String urlDecode(const char *s);   // zdefiniowane niżej, w części Wi-Fi

static IoSettings io;                 // edytowane z menu
static IoSettings ioBoot;             // aktywne od startu (wyjścia zmieniają się po restarcie)
static volatile bool ioDirty = false;
static volatile unsigned long ioDirtyMs = 0;
static bool servoActive[SERVO_COUNT];
static int8_t servoLedc[SERVO_COUNT];          // kanał LEDC serwa na pinie ESP32, -1 = PCA
static volatile float servoTarget[SERVO_COUNT];
static float servoPos[SERVO_COUNT];
static int servoUsOut[SERVO_COUNT];
static volatile bool actOn[ACT_COUNT];         // stan akcji: włączona / przytrzymana
static bool actPhoneHeld[ACT_COUNT];           // przytrzymana z telefonu – puszcza ją failsafe
static int8_t actRcLast[ACT_COUNT];            // ostatni stan przełącznika aparatury (-1 = nieznany)
static bool actOutActive[ACT_COUNT];
static int8_t actOutLevel[ACT_COUNT];

static void ioDefaults(IoSettings &d) {
  memset(&d, 0, sizeof(d));
  d.ver = IO_SETTINGS_VERSION;
  for (int i = 0; i < SERVO_COUNT; i++) {
    ServoCfg &c = d.s[i];
    c.out = -1; c.minA = 0; c.maxA = 180; c.center = 90;
    c.ctrl = i == 0 ? SC_PAN : i == 1 ? SC_TILT : SC_NONE;
    snprintf(c.name, sizeof(c.name), i == 0 ? "Pan" : i == 1 ? "Tilt" : "Serwo %d", i + 1);
  }
  for (int i = 0; i < ACT_COUNT; i++) { d.a[i].out = -1; d.a[i].a = 90; d.a[i].b = 0; }
}

static void ioLoad() {
  ioDefaults(io);
  prefs.begin("io", true);
  IoSettings tmp;
  if (prefs.getBytesLength("s") == sizeof(tmp) && prefs.getBytes("s", &tmp, sizeof(tmp)) == sizeof(tmp) &&
      tmp.ver == IO_SETTINGS_VERSION) {
    io = tmp;
    for (int i = 0; i < SERVO_COUNT; i++) io.s[i].name[sizeof(io.s[i].name) - 1] = 0;
    for (int i = 0; i < ACT_COUNT; i++) io.a[i].label[sizeof(io.a[i].label) - 1] = 0;
  }
  prefs.end();
  ioBoot = io;
  // przydział wyjść: serwa (na pinach ESP32 najwyżej dwa), potem wyjścia przycisków
  int gpioServos = 0;
  for (int i = 0; i < SERVO_COUNT; i++) {
    int p = ioBoot.s[i].out;
    servoLedc[i] = -1;
    servoActive[i] = false;
    if (isFreePin(p) && gpioServos >= 2) continue;
    if (!claimOut(p)) continue;
    servoActive[i] = true;
    if (isFreePin(p)) servoLedc[i] = SERVO_CH[gpioServos++];
  }
  for (int i = 0; i < ACT_COUNT; i++) {
    actOutActive[i] = ioBoot.a[i].type == ACT_OUTPUT && claimOut(ioBoot.a[i].out);
    actOutLevel[i] = -1;
    actRcLast[i] = -1;
  }
}

static void ioSave() {
  prefs.begin("io", false);
  prefs.putBytes("s", &io, sizeof(io));
  prefs.end();
}

static bool ioPending() {
  for (int i = 0; i < SERVO_COUNT; i++) if (io.s[i].out != ioBoot.s[i].out) return true;
  for (int i = 0; i < ACT_COUNT; i++)
    if ((io.a[i].type == ACT_OUTPUT) != (ioBoot.a[i].type == ACT_OUTPUT) || io.a[i].out != ioBoot.a[i].out) return true;
  return false;
}

static void ioChanged() { ioDirty = true; ioDirtyMs = millis(); }

static float servoClamp(int i, float a) { return constrain(a, (float)io.s[i].minA, (float)io.s[i].maxA); }

// Impuls 500..2500 us dla kąta 0..180°
static void servoWrite(int i, int us) {
  int p = ioBoot.s[i].out;
  if (isPcaOut(p)) pcaSet(p - PCA_BASE, (uint32_t)us * hwBoot.pcaFreq * 4096UL / 1000000UL);
  else if (servoLedc[i] >= 0) pwmWrite(p, servoLedc[i], (uint32_t)us * 65535UL / 20000UL);
}

static void servosInit() {
  for (int i = 0; i < SERVO_COUNT; i++) {
    servoPos[i] = io.s[i].center;
    servoTarget[i] = io.s[i].center;
    servoUsOut[i] = -1;
    if (!servoActive[i]) continue;
    if (servoLedc[i] >= 0) pwmAttach(ioBoot.s[i].out, servoLedc[i], 50, 16);
  }
}

// Co 20 ms: serwa sterowane aparaturą idą za kanałem, wszystkie dochodzą do celu z ustawioną prędkością
static void servoUpdate() {
  static unsigned long last = 0;
  unsigned long t = millis();
  if (t - last < 20) return;
  float dt = min((t - last) / 1000.0f, 0.2f);
  last = t;
  bool rc = rcConnected();
  for (int i = 0; i < SERVO_COUNT; i++) {
    if (!servoActive[i]) continue;
    const ServoCfg &c = io.s[i];
    if (c.ctrl == SC_RC && c.ch && rc) {
      int v = rcAxis(c.ch);           // -100..100: od min przez środek do max
      servoTarget[i] = v < 0 ? c.center + v * (c.center - c.minA) / 100.0f : c.center + v * (c.maxA - c.center) / 100.0f;
    }
    float tgt = servoClamp(i, servoTarget[i]);
    if (c.speed == 0) servoPos[i] = tgt;
    else servoPos[i] += constrain(tgt - servoPos[i], -c.speed * 10 * dt, c.speed * 10 * dt);
    float a = c.inv ? 180 - servoPos[i] : servoPos[i];
    int us = 500 + (int)(a * 2000 / 180);
    if (us != servoUsOut[i]) {
      servoUsOut[i] = us;
      servoWrite(i, us);
    }
  }
}

static void centerCamera() {
  for (int i = 0; i < SERVO_COUNT; i++)
    if (io.s[i].ctrl == SC_PAN || io.s[i].ctrl == SC_TILT) servoTarget[i] = io.s[i].center;
}

// Naciśnięcie (down) / puszczenie przycisku albo przełącznika aparatury
static void actPress(int i, bool down, bool fromPhone) {
  if (i < 0 || i >= ACT_COUNT) return;
  const ActCfg &a = io.a[i];
  bool st = a.mode ? down : (down ? !actOn[i] : actOn[i]);   // przytrzymanie albo przełącznik
  switch (a.type) {
    case ACT_LIGHT:  if (down) lightOn = !lightOn; break;
    case ACT_OUTPUT: actOn[i] = st; break;
    case ACT_SERVO:
      actOn[i] = st;
      if (a.servo < SERVO_COUNT) servoTarget[a.servo] = st ? a.a : a.b;
      break;
    case ACT_CENTER: if (down) centerCamera(); break;
    default: break;                   // moc i turbo obsługuje strona w telefonie
  }
  if (fromPhone) actPhoneHeld[i] = down && a.mode == 1;
}

// Kanały aparatury przypisane do przycisków: stan przełącznika = stan funkcji
static void actionsRc() {
  bool rc = rcConnected();
  for (int i = 0; i < ACT_COUNT; i++) {
    const ActCfg &a = io.a[i];
    if (!a.ch || a.type == ACT_NONE || !rc) { actRcLast[i] = -1; continue; }
    int sw = rcSwitch(a.ch) ? 1 : 0;
    if (sw == actRcLast[i]) continue;
    bool first = actRcLast[i] == -1;
    actRcLast[i] = sw;
    switch (a.type) {
      case ACT_LIGHT:  if (!first) lightOn = sw; break;
      case ACT_OUTPUT: actOn[i] = sw; break;
      case ACT_SERVO:
        actOn[i] = sw;
        if (a.servo < SERVO_COUNT) servoTarget[a.servo] = sw ? a.a : a.b;
        break;
      case ACT_CENTER: if (sw && !first) centerCamera(); break;
      default: break;
    }
  }
}

// Wyjścia przycisków (np. dodatkowe światła, buzzer, przekaźnik): włączone / wyłączone
static void actionsOutputs() {
  for (int i = 0; i < ACT_COUNT; i++) {
    if (!actOutActive[i]) continue;
    int8_t lv = actOn[i] ? 1 : 0;
    if (lv == actOutLevel[i]) continue;
    int p = ioBoot.a[i].out;
    if (actOutLevel[i] == -1 && isFreePin(p)) pinMode(p, OUTPUT);
    actOutLevel[i] = lv;
    if (isPcaOut(p)) pcaSet(p - PCA_BASE, lv ? 4096 : 0);
    else digitalWrite(p, lv ? HIGH : LOW);
  }
}

static void releasePhoneActions() {
  for (int i = 0; i < ACT_COUNT; i++) if (actPhoneHeld[i]) actPress(i, false, true);
}

static uint8_t actMask() {
  uint8_t m = 0;
  for (int i = 0; i < ACT_COUNT; i++) if (actOn[i]) m |= 1 << i;
  return m;
}

// "S i klucz wartość" – konfiguracja serwa albo ruch ("p" – kąt, np. z przeciągania po obrazie)
static void servoSet(int i, const char *key, const char *val) {
  if (!strcmp(key, "reset")) { ioDefaults(io); centerCamera(); ioChanged(); return; }
  if (i < 0 || i >= SERVO_COUNT) return;
  ServoCfg &c = io.s[i];
  int v = atoi(val);
  if (!strcmp(key, "p")) { servoTarget[i] = servoClamp(i, v); return; }   // ruch nie jest zapisywany
  if      (!strcmp(key, "o"))   c.out = (isFreePin(v) || isPcaOut(v)) ? v : -1;
  else if (!strcmp(key, "mn"))  c.minA = constrain(v, 0, c.maxA);
  else if (!strcmp(key, "mx"))  c.maxA = constrain(v, c.minA, 180);
  else if (!strcmp(key, "c"))   { c.center = constrain(v, 0, 180); servoTarget[i] = c.center; }
  else if (!strcmp(key, "inv")) c.inv = v ? 1 : 0;
  else if (!strcmp(key, "sp"))  c.speed = constrain(v, 0, 30);
  else if (!strcmp(key, "ct"))  c.ctrl = constrain(v, 0, 3);
  else if (!strcmp(key, "ch"))  c.ch = constrain(v, 0, 16);
  else if (!strcmp(key, "n"))   { String n = urlDecode(val); strlcpy(c.name, n.c_str(), sizeof(c.name)); }
  else return;
  c.center = constrain(c.center, c.minA, c.maxA);
  ioChanged();
}

// "A i klucz wartość" – konfiguracja przycisku / akcji
static void actSet(int i, const char *key, const char *val) {
  if (i < 0 || i >= ACT_COUNT) return;
  ActCfg &a = io.a[i];
  int v = atoi(val);
  if      (!strcmp(key, "t"))  { a.type = constrain(v, 0, ACT_TYPES - 1); actOn[i] = false; }
  else if (!strcmp(key, "m"))  a.mode = v ? 1 : 0;
  else if (!strcmp(key, "o"))  a.out = (isFreePin(v) || isPcaOut(v)) ? v : -1;
  else if (!strcmp(key, "sv")) a.servo = constrain(v, 0, SERVO_COUNT - 1);
  else if (!strcmp(key, "a"))  a.a = constrain(v, 0, 180);
  else if (!strcmp(key, "b"))  a.b = constrain(v, 0, 180);
  else if (!strcmp(key, "ch")) a.ch = constrain(v, 0, 16);
  else if (!strcmp(key, "sh")) a.show = v ? 1 : 0;
  else if (!strcmp(key, "l"))  { String l = urlDecode(val); strlcpy(a.label, l.c_str(), sizeof(a.label)); }
  else return;
  ioChanged();
}

// ---------- Komendy z przeglądarki ----------
// "M x y [s]" – ruch: skręt/obrót, przód/tył, bok (Mecanum); -100..100
// "L 0|1"  – światło
// "C k v"  – ustawienie obrazu, np. "C br 1" (nazwy w struct CamSettings)
// "H k v"  – napęd / piny / kanały aparatury, np. "H p0 14" (patrz hwSet)
// "V"      – podgląd kanałów aparatury (odpowiedź "V ...")
// "W k v"  – Wi-Fi (wartość zakodowana jak w URL), np. "W ss Moja%20siec", "W apply 1"
// "S i k v" – serwo nr i (konfiguracja albo ruch "p"), "A i k v" – przycisk nr i, "B i 1|0" – naciśnięcie / puszczenie
// "P"      – ping (podtrzymanie failsafe)

// zdefiniowane niżej, w części Wi-Fi
static void wifiSet(const char *key, const char *val);
static esp_err_t wifiHandler(httpd_req_t *req);
static esp_err_t scanHandler(httpd_req_t *req);
static esp_err_t sysHandler(httpd_req_t *req);
static esp_err_t ioHandler(httpd_req_t *req);
static esp_err_t updateHandler(httpd_req_t *req);

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
    case 'S':
    case 'A': {
      int idx;
      char key[8], val[64];
      val[0] = 0;
      if (sscanf(cmd + 1, "%d %7s %63s", &idx, key, val) >= 2) {
        if (cmd[0] == 'S') servoSet(idx, key, val);
        else actSet(idx, key, val);
      }
      break;
    }
    case 'B':
      if (sscanf(cmd + 1, "%d %d", &a, &b) == 2) actPress(a, b != 0, true);
      break;
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
  char json[320];
  snprintf(json, sizeof(json),
           "{\"fs\":%d,\"q\":%d,\"br\":%d,\"ct\":%d,\"sa\":%d,\"wb\":%d,\"ae\":%d,"
           "\"al\":%d,\"ex\":%d,\"fx\":%d,\"hm\":%d,\"vf\":%d,\"rot\":%d,"
           "\"xc\":%d,\"gc\":%d,\"bpc\":%d,\"wpc\":%d,\"lenc\":%d,\"psram\":%d}",
           cam.fs, cam.q, cam.br, cam.ct, cam.sa, cam.wb, cam.ae, cam.al, cam.ex, cam.fx,
           cam.hm, cam.vf, cam.rot, cam.xclk, cam.gc, cam.bpc, cam.wpc, cam.lenc, hasPsram ? 1 : 0);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

// Konfiguracja napędu i aparatury jako JSON (menu "Napęd" / "ELRS")
static esp_err_t hwHandler(httpd_req_t *req) {
  char json[700];
  const HwSettings &h = hw;
  snprintf(json, sizeof(json),
           "{\"mode\":%d,\"mot\":%d,\"p\":[%d,%d,%d,%d,%d,%d,%d,%d],\"inv\":[%d,%d,%d,%d],"
           "\"md\":%d,\"lp\":%d,\"rp\":%d,\"c\":[%d,%d,%d,%d,%d,%d],"
           "\"pf\":%d,\"fo\":%d,\"rt\":%d,\"rv\":%u,\"tg\":%d,\"mp\":%d,\"ac\":%d,\"lt\":%d,"
           "\"dz\":%d,\"rd\":%d,\"rm\":%d,\"drv\":%d,\"sda\":%d,\"scl\":%d,\"pa\":%d,\"pq\":%d,\"pcaOk\":%d,"
           "\"bootMode\":%d,\"pending\":%d,\"lightOk\":%d,\"rcOk\":%d,\"serialOff\":%d}",
           h.mode, h.motors, h.pin[0], h.pin[1], h.pin[2], h.pin[3], h.pin[4], h.pin[5], h.pin[6], h.pin[7],
           h.inv[0], h.inv[1], h.inv[2], h.inv[3], h.minDuty, h.lightPin, h.rcPin,
           h.ch[0], h.ch[1], h.ch[2], h.ch[3], h.ch[4], h.ch[5],
           h.pwmFreq, h.failsafeMs, h.rcTimeout, (unsigned)h.rcRev, h.turnGain, h.maxPower, h.accel, h.lightPct,
           h.stickDz, h.rcDz, h.rcMinSpeed, h.driver, h.sda, h.scl, h.pcaAddr, h.pcaFreq, pcaOk ? 1 : 0,
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
    // Stan: "R <aparatura> <światło> <fps> <KB na klatkę> <koła %: LP PP LT PT> <włączone przyciski – bity>"
    olen = snprintf(out, sizeof(out), "R %d %d %d %d %d %d %d %d %d", rcState, lightOn ? 1 : 0, streamFps, streamKB,
                    wheelOut[0], wheelOut[1], wheelOut[2], wheelOut[3], actMask());
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
  config.max_uri_handlers = 16;        // domyślnie 8 – za mało dla wszystkich adresów strony
  config.recv_wait_timeout = 10;       // s – przy wgrywaniu aktualizacji

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
    httpd_uri_t sysUri = { "/sys", HTTP_GET, sysHandler, NULL };
    httpd_uri_t ioUri = { "/io", HTTP_GET, ioHandler, NULL };
    httpd_register_uri_handler(webHttpd, &ioUri);
    httpd_uri_t updateUri = { "/update", HTTP_POST, updateHandler, NULL };
    httpd_register_uri_handler(webHttpd, &sysUri);
    httpd_register_uri_handler(webHttpd, &updateUri);
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
  c.xclk_freq_hz = cam.xclk * 1000000;
  camInitXclk = cam.xclk;
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
  sensor_t *sens = esp_camera_sensor_get();
  camPid = sens ? sens->id.PID : 0;
  if (camFresh) {
    camFresh = false;
    int8_t xclk = cam.xclk;
    cam = camDefaultsFor();
    cam.xclk = xclk;
    camDirty = true;
    camDirtyMs = millis();
  }
  Serial.printf("Kamera: %s\n", camName());
  camResetApplied();
  camApplyAll();
  Serial.printf("Kamera gotowa, bufor dla rozdzielczosci nr %d\n", camInitFs);
  return true;
}

// ---------- Wi-Fi ----------

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

// ---------- Serwa i przyciski: JSON dla menu ----------

static esp_err_t ioHandler(httpd_req_t *req) {
  String j = "{\"s\":[";
  for (int i = 0; i < SERVO_COUNT; i++) {
    const ServoCfg &c = io.s[i];
    if (i) j += ",";
    j += "{\"n\":" + jsonStr(c.name) + ",\"o\":" + String(c.out) + ",\"mn\":" + String(c.minA) +
         ",\"mx\":" + String(c.maxA) + ",\"c\":" + String(c.center) + ",\"inv\":" + String(c.inv) +
         ",\"sp\":" + String(c.speed) + ",\"ct\":" + String(c.ctrl) + ",\"ch\":" + String(c.ch) +
         ",\"pos\":" + String((int)roundf(servoTarget[i])) + ",\"ok\":" + String(servoActive[i] ? 1 : 0) + "}";
  }
  j += "],\"a\":[";
  for (int i = 0; i < ACT_COUNT; i++) {
    const ActCfg &a = io.a[i];
    if (i) j += ",";
    j += "{\"t\":" + String(a.type) + ",\"m\":" + String(a.mode) + ",\"o\":" + String(a.out) +
         ",\"sv\":" + String(a.servo) + ",\"a\":" + String(a.a) + ",\"b\":" + String(a.b) +
         ",\"ch\":" + String(a.ch) + ",\"sh\":" + String(a.show) + ",\"l\":" + jsonStr(a.label) +
         ",\"ok\":" + String(a.type != ACT_OUTPUT || actOutActive[i] ? 1 : 0) + "}";
  }
  j += "],\"pending\":" + String(ioPending() ? 1 : 0) + "}";
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, j.c_str(), j.length());
}

// ---------- System: stan i aktualizacja przez Wi-Fi ----------

static const char *resetReasonText() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "włączenie zasilania";
    case ESP_RST_SW:       return "restart z programu";
    case ESP_RST_PANIC:    return "błąd programu";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      return "zawieszenie (watchdog)";
    case ESP_RST_BROWNOUT: return "spadek napięcia";
    case ESP_RST_EXT:      return "przycisk RST";
    case ESP_RST_DEEPSLEEP: return "wybudzenie";
    default:               return "inna";
  }
}

// Stan systemu: wersja, czas pracy, pamięć, miejsce na aktualizację
static esp_err_t sysHandler(httpd_req_t *req) {
  const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
  char json[440];
  snprintf(json, sizeof(json),
           "{\"ver\":\"%s\",\"up\":%lu,\"reset\":\"%s\",\"heap\":%u,\"heapMin\":%u,\"heapSize\":%u,"
           "\"psram\":%u,\"psramSize\":%u,\"sketch\":%u,\"otaSize\":%u,\"ota\":%d,\"cpu\":%u,\"cam\":\"%s\"}",
           FW_VERSION, (unsigned long)(millis() / 1000), resetReasonText(),
           (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(), (unsigned)ESP.getHeapSize(),
           (unsigned)ESP.getFreePsram(), (unsigned)ESP.getPsramSize(),
           (unsigned)ESP.getSketchSize(), next ? (unsigned)next->size : 0, next ? 1 : 0,
           (unsigned)ESP.getCpuFreqMHz(), camName());
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static volatile bool otaActive = false;

static esp_err_t otaFail(httpd_req_t *req, const char *msg) {
  Update.abort();
  otaActive = false;
  camPause = false;
  Serial.printf("Aktualizacja nieudana: %s\n", msg);
  httpd_resp_set_status(req, "500 Internal Server Error");
  httpd_resp_set_type(req, "text/plain; charset=utf-8");
  return httpd_resp_send(req, msg, HTTPD_RESP_USE_STRLEN);
}

// Aktualizacja przez Wi-Fi: POST /update, treść = plik *-app.bin z wydania
static esp_err_t updateHandler(httpd_req_t *req) {   // komunikaty błędów trafiają na stronę
  size_t remaining = req->content_len;
  if (!esp_ota_get_next_update_partition(NULL))
    return otaFail(req, "Ten układ pamięci nie obsługuje aktualizacji przez Wi-Fi. Wgraj raz kablem plik full.bin z wersji 1.3.0 lub nowszej.");
  if (remaining == 0) return otaFail(req, "Pusty plik.");
  otaActive = true;                    // loop() zatrzymuje silniki
  camPause = true;                     // strumień obrazu czeka – cała przepustowość dla pliku
  if (!Update.begin(remaining)) return otaFail(req, "Plik jest za duży.");

  static uint8_t buf[2048];            // static – stos zadania serwera jest mały
  bool first = true;
  while (remaining > 0) {
    int r = httpd_req_recv(req, (char *)buf, min(remaining, sizeof(buf)));
    if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
    if (r <= 0) return otaFail(req, "Przerwane połączenie.");
    if (first && buf[0] != 0xE9)
      return otaFail(req, "To nie jest plik programu. Użyj pliku *-app.bin (nie full.bin).");
    first = false;
    if (Update.write(buf, r) != (size_t)r) return otaFail(req, Update.errorString());
    remaining -= r;
  }
  if (!Update.end(true)) return otaFail(req, Update.errorString());

  Serial.println("Aktualizacja wgrana - restart");
  httpd_resp_set_type(req, "text/plain; charset=utf-8");
  httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
  rebootRequested = true;              // restart w loop() po wysłaniu odpowiedzi
  return ESP_OK;
}

// ============================================================

void setup() {
  // Wyłącz detektor brown-out – rozruch silników potrafi chwilowo obniżyć napięcie
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  Serial.println("\nESP32-CAM Robot");

  hwLoad();
  ioLoad();                           // serwa i przyciski dostają piny, które zostały wolne
  Serial.printf("Naped: %s, piny silnikow:", hwBoot.mode == MODE_MECANUM ? "Mecanum" : "czolg");
  for (int i = 0; i < motorPinCount(hwBoot); i++) Serial.printf(" %d", hwBoot.pin[i]);
  Serial.printf("; dioda: %d%s; ELRS: %d%s\n", hwBoot.lightPin, lightActive ? "" : " (wylaczona)",
                hwBoot.rcPin, rcActive ? "" : " (wylaczony)");
  if (hwUsesUart0()) {
    Serial.println("Piny 1/3 sa uzyte przez naped/diode/ELRS - Serial Monitor zostaje wylaczony");
    Serial.flush();
    Serial.end();
  }

  pcaOk = pcaInit();
  motorsInit();
  servosInit();
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
  bool stopNow = false;
  if (otaActive) {
    stopNow = true;                   // wgrywanie aktualizacji – robot stoi
  } else if (rcState == 2) {
    // aparatura połączona i uzbrojona – ma pierwszeństwo
    x = rcX;
    y = rcY;
    sd = rcS;
  } else if (millis() - lastCmdMs <= (unsigned long)hw.failsafeMs) {
    // telefon; bez świeżych komend (failsafe) robot stoi
    x = targetX;
    y = targetY;
    sd = targetS;
  } else {
    stopNow = true;                   // failsafe: stop od razu, bez płynnego hamowania
  }
  drive(x, y, sd, stopNow);

  // przyciski i serwa
  if (millis() - lastCmdMs > (unsigned long)hw.failsafeMs) releasePhoneActions();   // telefon zamilkł
  actionsRc();
  servoUpdate();
  actionsOutputs();
  if (ioDirty && (millis() - ioDirtyMs > 1000 || rebootRequested)) {
    ioDirty = false;
    ioSave();
  }

  // światło (jasność z menu można zmieniać przy włączonej diodzie)
  static int lightDutyApplied = -1;
  int ld = lightOn ? hw.lightPct * 255 / 100 : 0;
  if (ld != lightDutyApplied) {
    if (lightActive) outDuty(hwBoot.lightPin, CH_LIGHT, ld);
    lightDutyApplied = ld;
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
    WiFi.scanNetworks(true);          // asynchronicznie, wynik pod /scan
    wifiScanReq = false;              // dopiero po starcie – /scan do tego czasu odpowiada "busy"
  }
  if (wifiApply) {
    wifiSave(wifiNew);
    rebootRequested = true;
  }

  if (rebootRequested) {
    drive(0, 0, 0, true);
    if (hwDirty) hwSave();            // nie zgub niezapisanych zmian
    if (ioDirty) ioSave();
    if (camDirty) camSave();
    delay(300);                       // czas na wysłanie odpowiedzi do przeglądarki
    ESP.restart();
  }

  // Restart kamery z innym buforem (zmiana na / z 1024x768 lub HD)
  // loop() nigdy nie czeka dłużej niż 1 s – tu działają też silniki, światło i failsafe
  if (camNeedReinit) {
    drive(0, 0, 0, true);             // na czas restartu kamery robot stoi
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
