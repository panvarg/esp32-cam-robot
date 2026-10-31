# ESP32-CAM Robot

Robot sterowany z telefonu przez przeglądarkę: obraz z kamery na pełnym ekranie i wirtualny analog jak w grach.

**Sprzęt:** ESP32-CAM (AI-Thinker) z programatorem ESP32-CAM-MB, sterownik L298N, 2 lub 4 silniki DC (2WD albo 4WD, patrz niżej), akumulator 7–12 V (np. 2× 18650).

## Jak to działa

- ESP32 tworzy własną sieć Wi-Fi **`ESP32-Robot`** (hasło `robot1234`).
- Na telefonie łączysz się z tą siecią i otwierasz **http://192.168.4.1**.
- Lewa część ekranu to analog. Dotknij w dowolnym miejscu, a gałka pojawi się pod palcem. Przód/tył to jazda, lewo/prawo to skręt; przy wychyleniu w samo lewo lub prawo robot obraca się w miejscu.
- Po prawej są przycisk światła (dioda flash) i regulacja mocy (20–100%).
- W prawym górnym rogu wybierasz jakość obrazu. Wybór jest zapamiętywany w przeglądarce:
  - **Płynnie**: 320×240, najwięcej klatek i najmniejsze opóźnienie, najlepsze do szybkiej jazdy
  - **Balans**: 640×480 (domyślnie)
  - **Jakość**: 800×600, ładniejszy obraz, ale mniej klatek i większe opóźnienie
- Na komputerze możesz sterować klawiszami WASD lub strzałkami, a `F` włącza światło.
- **Failsafe:** jeśli przez 0,6 s nie przyjdzie żadna komenda (zerwane Wi-Fi, zamknięta karta), silniki się zatrzymują.

Sterowanie idzie przez WebSocket (port 80), a obraz jako strumień MJPEG (port 81).

## Połączenia

```
 L298N                ESP32-CAM
 ─────                ─────────
 IN1  ─────────────── GPIO 14
 IN2  ─────────────── GPIO 15
 IN3  ─────────────── GPIO 13
 IN4  ─────────────── GPIO 12
 ENA / ENB            zworki ZAŁOŻONE (prędkość regulowana PWM-em na INx)
 GND  ─────────────── GND   ◄── wspólna masa jest obowiązkowa!
 +5V  ─────────────── 5V    (zasilanie ESP32-CAM ze stabilizatora L298N)
 +12V ◄── akumulator +  (7–12 V)
 GND  ◄── akumulator −

 OUT1/OUT2 ── silnik LEWY
 OUT3/OUT4 ── silnik PRAWY
```

Uwagi:

- Zworka **5V-EN** na L298N musi być założona (wtedy stabilizator daje 5 V). Przy zasilaniu powyżej 12 V zdejmij ją i zasil ESP32-CAM z osobnej przetwornicy 5 V.
- **GPIO 12** jest pinem konfiguracyjnym. Jeśli w chwili startu ma stan wysoki, ESP32 się nie uruchomi (błąd flash/boot). Wejścia L298N zwykle tego nie robią, ale jeśli robot nie startuje, odłącz na chwilę przewód z IN4 przy resecie albo dodaj rezystor 10 kΩ z GPIO 12 do GND.
- Programator ESP32-CAM-MB zakłada się tylko do wgrywania programu. W robocie ESP32-CAM zasilasz z L298N.
- Silniki przy ruszaniu powodują spadki napięcia. Kondensator 470–1000 µF na zasilaniu 5 V ESP32-CAM i kondensatory 100 nF na zaciskach silników bardzo pomagają.

## Wersja 4×4: 4 silniki, dwa L298N

Przy 4 nieskrętnych kołach robot skręca jak czołg: koła lewej strony kręcą się inaczej niż koła prawej strony. Kod już to robi, więc nic w nim nie zmieniasz. Zmienia się tylko okablowanie.

Najlepiej użyć **obu modułów L298N**, tak żeby każdy silnik miał własny kanał. Jeden kanał L298N daje realnie ok. 1–1,5 A, a dwa silniki na jednym kanale przy skręcie w miejscu łatwo go przegrzewają. Wejścia obu modułów łączysz równolegle z tymi samymi pinami ESP32:

```
 ESP32-CAM      L298N #1 (przód)     L298N #2 (tył)
 ─────────      ────────────────     ──────────────
 GPIO 14  ───┬─ IN1                ┌─ IN1
             └─────────────────────┘
 GPIO 15  ───┬─ IN2                ┌─ IN2
             └─────────────────────┘
 GPIO 13  ───┬─ IN3                ┌─ IN3
             └─────────────────────┘
 GPIO 12  ───┬─ IN4                ┌─ IN4
             └─────────────────────┘
 GND      ───── GND ─────────────────── GND ── akumulator −
                +12V ────────────────── +12V ── akumulator +
 5V       ───── +5V    (tylko z JEDNEGO modułu!)

 L298N #1: OUT1/OUT2 = lewy przedni    OUT3/OUT4 = prawy przedni
 L298N #2: OUT1/OUT2 = lewy tylny      OUT3/OUT4 = prawy tylny
```

- ESP32-CAM zasilaj z wyjścia +5V **tylko jednego** modułu. Pinów +5V obu modułów nie łącz ze sobą.
- Po podłączeniu podnieś robota i dla każdego koła sprawdź, czy przy wychyleniu analogu do przodu kręci się do przodu. Jeśli któreś kręci się odwrotnie, zamień przewody tylko tego silnika na zaciskach OUTx. Koła po jednej stronie muszą kręcić się zgodnie, inaczej będą ze sobą walczyć.
- `INVERT_LEFT` / `INVERT_RIGHT` odwracają całą stronę naraz, czyli oba moduły.
- Obrót w miejscu przy 4 kołach wymaga sporo siły, bo koła ślizgają się bokiem. Jeśli robot przy małym wychyleniu tylko buczy, zwiększ `MIN_DUTY` (np. do 110–130). Pomaga też zasilanie 2S Li-ion (7,4–8,4 V), a nie 4×AA.
- Wersja oszczędna: jeden L298N, a dwa silniki z każdej strony połączone równolegle na jednym kanale (lewe na OUT1/OUT2, prawe na OUT3/OUT4). Działa, ale moduł mocno się grzeje.

## Odbiornik ELRS (opcjonalnie)

Robota można też prowadzić aparaturą RC z odbiornikiem ExpressLRS, takim jak do dronów. Odbiornik wysyła dane w protokole CRSF.

```
 Odbiornik ELRS        ESP32-CAM
 ──────────────        ─────────
 TX  ───────────────── GPIO 2
 RX                    (niepodłączony – robot nic nie odsyła)
 VCC ───────────────── 5V
 GND ───────────────── GND
```

**Kto steruje:**

| Stan | Steruje |
|---|---|
| odbiornik niepodłączony albo aparatura wyłączona | telefon |
| aparatura połączona, AUX1 wyłączony | telefon (na stronie widać napis „Aparatura połączona”) |
| aparatura połączona, AUX1 włączony | **aparatura** (na stronie: „Aparatura steruje”, analog na ekranie jest wyszarzony) |
| zerwane połączenie z aparaturą | po 0,25 s sterowanie wraca do telefonu |

Kanały (tryb 2, ustawienia na górze `.ino`, `RC_CH_*`):
- **CH1** (prawy drążek lewo/prawo) – skręt
- **CH2** (prawy drążek góra/dół) – jazda przód/tył
- **CH3** (gaz) – ograniczenie mocy, od 25% przy gazie na dole do 100% przy gazie na górze
- **CH5 / AUX1** – przejęcie sterowania przez aparaturę. Uzbrojenie zadziała tylko wtedy, gdy drążek jazdy jest w środku.
- **CH6 / AUX2** – światło. Przełącznik zmienia stan przy każdym przestawieniu, a przycisk na stronie działa nadal.

Ważne:
- W ustawieniach odbiornika ELRS (Lua lub WebUI) ustaw **Failsafe = No Pulses** (domyślne). W trybie „Last Position” robot po utracie zasięgu jechałby dalej.
- Odbiornik powinien pracować w trybie CRSF/Serial (domyślny) na 420000 bodów.
- GPIO 2 jest pinem rozruchowym. **Przy wgrywaniu programu odłącz przewód z GPIO 2**, inaczej ESP32 może nie wejść w tryb programowania. W normalnej pracy to nie przeszkadza.
- Chcesz wyłączyć obsługę odbiornika? Ustaw `RC_ENABLED false`.

## Wgrywanie (Arduino IDE)

1. W *Preferencje → Dodatkowe adresy URL menedżera płytek* wpisz:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
2. W *Menedżerze płytek* zainstaluj **esp32 by Espressif Systems** (wersja 2.x lub 3.x).
3. Otwórz `esp32_cam_robot/esp32_cam_robot.ino`.
4. Wybierz *Płytka*: **AI Thinker ESP32-CAM** i *Partition Scheme*: **Huge APP**.
5. Podłącz ESP32-CAM na programatorze ESP32-CAM-MB przez USB, wybierz port i kliknij *Wgraj*.
   Jeśli wgrywanie się nie zaczyna, przytrzymaj **IO0** na programatorze, naciśnij i puść **RST**, a potem puść IO0.
6. Otwórz Serial Monitor (115200), naciśnij RST i sprawdź komunikaty.

Gotowe pliki `.bin` buduje też GitHub Actions (zakładka *Actions*, artefakt `firmware-…`).

## Konfiguracja

Wszystkie ustawienia są na górze pliku `esp32_cam_robot.ino`:

| Ustawienie | Opis |
|---|---|
| `USE_AP_MODE` | `true`: robot tworzy własną sieć. `false`: łączy się z routerem (`STA_SSID`/`STA_PASS`), a gdy się nie uda, wraca do trybu AP |
| `AP_SSID`, `AP_PASS` | nazwa i hasło sieci robota |
| `INVERT_LEFT`, `INVERT_RIGHT` | odwraca kierunek koła, gdy kręci się w złą stronę |
| `MIN_DUTY` | minimalne wypełnienie PWM, przy którym silniki ruszają (zwiększ, jeśli robot „buczy”, ale nie jedzie) |
| `CAM_VFLIP`, `CAM_HMIRROR` | obrót i odbicie obrazu z kamery |
| `VIDEO_PRESETS` | rozdzielczość i kompresja JPEG dla przycisków Płynnie/Balans/Jakość (quality: 10 = najlepsza, 63 = najgorsza) |
| `LIGHT_DUTY` | jasność diody flash |
| `FAILSAFE_MS` | po ilu ms bez sygnału robot staje |

Jeśli robot skręca w lewo, gdy wychylasz analog w prawo, zamień miejscami przewody silnika lewego i prawego (OUT1/2 ↔ OUT3/4) albo ustaw oba `INVERT_*`.

**Wskazówka:** na telefonie dodaj stronę do ekranu głównego („Dodaj do ekranu początkowego”) i trzymaj telefon poziomo. Pasek adresu wtedy znika.
