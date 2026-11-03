# ESP32-CAM Robot

Robot sterowany z telefonu przez przeglądarkę: obraz z kamery na pełnym ekranie i wirtualny analog jak w grach.

**Sprzęt:** ESP32-CAM (AI-Thinker) z programatorem ESP32-CAM-MB, sterownik L298N, 2 lub 4 silniki DC (2WD albo 4WD, patrz niżej), akumulator 7–12 V (np. 2× 18650).

## Jak to działa

- ESP32 tworzy własną sieć Wi-Fi **`ESP32-Robot`** (hasło `robot1234`).
- Na telefonie łączysz się z tą siecią i otwierasz **http://192.168.4.1**.
- Domyślnie sterujesz dwoma kciukami, jak w grach wyścigowych: **lewa gałka to gaz** (góra = przód, dół = tył), **prawa to skręt** (lewo/prawo). Skręt bez gazu obraca robota w miejscu.
- **Układ ekranu ustawiasz sam**, jak w emulatorach gier: ⚙ → Przyciski (albo Napęd) → „Edytuj układ ekranu”. Szczegóły są niżej.
- Na górze ekranu są przycisk światła (dioda flash) i regulacja mocy (20–100%).
- Przycisk **⚙** otwiera **menu ustawień obrazu**. Ustawienia zapisują się w pamięci robota i zostają po wyłączeniu:
  - szybki wybór: **Płynnie** (320×240), **Balans** (640×480), **Jakość** (800×600);
  - rozdzielczość od 320×240 do 1280×720 i kompresja JPEG;
  - obrót o 0/90/180/270°, odbicie lustrzane i odwrócenie w pionie;
  - jasność, kontrast, nasycenie i balans bieli;
  - automatyczna albo ręczna ekspozycja;
  - efekty: negatyw, czarno-biały, sepia i inne.
- Obok statusu połączenia widać aktualną liczbę klatek na sekundę (FPS).
- Menu ⚙ ma siedem zakładek. Wszystko zapisuje się w robocie. Piny, typ napędu i częstotliwość PWM zaczynają działać po przycisku „Zapisz i uruchom ponownie”, reszta od razu.
  - **Obraz:** rozdzielczość, kompresja, obrót, kolory, ekspozycja, efekty, opcje na zakłócenia (taktowanie kamery 10/20 MHz, limit wzmocnienia, korekcja pikseli i rogów)
  - **Napęd:** sterownik (L298N albo TB6612FNG), **schemat połączeń** (rysunek płytki ESP32-CAM z opisem każdego pinu i karty modułów, liczone na żywo z ustawień), czołg albo Mecanum, liczba silników, piny L298N, odwrócenie kierunku, moc startowa, czułość skrętu, maksymalna moc, płynny rozruch, martwa strefa gałek, czas failsafe, częstotliwość PWM, pin i jasność diody
  - **ELRS:** pin odbiornika, przypisanie kanałów, moc przy gazie na dole, martwa strefa, czas utraty sygnału, podgląd 16 kanałów z odwracaniem
  - **Serwa:** moduł PCA9685 i do 6 serw (opisane niżej)
  - **Przyciski:** do 8 własnych przycisków na ekranie z przypisanymi funkcjami
  - **Wi-Fi:** sieć robota albo domowa (opisane niżej)
  - **System:** wersja, pamięć, aktualizacja przez Wi-Fi, kopia ustawień do pliku, restart
- Na komputerze możesz sterować klawiszami WASD lub strzałkami. Q/E to jazda bokiem (Mecanum), a `F` włącza światło.
- **Failsafe:** jeśli przez 0,6 s (do zmiany w menu) nie przyjdzie żadna komenda (zerwane Wi-Fi, zamknięta karta), silniki się zatrzymują.

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

Tak wygląda konfiguracja domyślna. Każdy pin możesz zmienić w menu ⚙ → Napęd. Do wyboru są wolne piny ESP32-CAM: 1, 2, 3, 4, 12, 13, 14 i 15. Strona ostrzega przed konfliktami. Jeśli piny silników zostaną źle ustawione, na przykład ten sam pin dwa razy, robot uruchomi się z ustawieniami domyślnymi.

Uwagi:

- Zworka **5V-EN** na L298N musi być założona (wtedy stabilizator daje 5 V). Przy zasilaniu powyżej 12 V zdejmij ją i zasil ESP32-CAM z osobnej przetwornicy 5 V.
- **GPIO 12** jest pinem konfiguracyjnym. Jeśli w chwili startu ma stan wysoki, ESP32 się nie uruchomi (błąd flash/boot). Wejścia L298N zwykle tego nie robią, ale jeśli robot nie startuje, odłącz na chwilę przewód z IN4 przy resecie albo dodaj rezystor 10 kΩ z GPIO 12 do GND.
- Programator ESP32-CAM-MB zakłada się tylko do wgrywania programu. W robocie ESP32-CAM zasilasz z L298N.
- Silniki przy ruszaniu powodują spadki napięcia. Kondensator 470–1000 µF na zasilaniu 5 V ESP32-CAM i kondensatory 100 nF na zaciskach silników bardzo pomagają.

## Sterownik TB6612FNG zamiast L298N

TB6612FNG działa z tym samym programem, bez zmian w kodzie. Silniki dostają pełne napięcie, bo nie ma spadku ok. 2 V jak w L298N, a sterownik prawie się nie grzeje. Oznaczenia pinów poniżej są takie, jak na płytce ESP32-CAM:

```
 TB6612FNG              ESP32-CAM / zasilanie
 ─────────              ─────────────────────
 AIN1  ──────────────── IO14   (lewa strona)
 AIN2  ──────────────── IO15
 BIN1  ──────────────── IO13   (prawa strona)
 BIN2  ──────────────── IO12
 PWMA, PWMB, STBY ───── 3V3 (na stałe – prędkością steruje PWM na AIN/BIN)
 VCC   ──────────────── 3V3 (logika)
 VM    ──────────────── akumulator + (4,5–13,5 V)
 GND   ──────────────── GND (pin obok 5V) i minus akumulatora – wspólna masa
 AO1/AO2 ── silnik lewy        BO1/BO2 ── silnik prawy

 Przetwornica step-down 5 V (np. MP1584, Mini360): wejście z akumulatora, wyjście na pin 5V ESP32-CAM
```

- TB6612 **nie ma stabilizatora 5 V**, więc ESP32-CAM trzeba zasilić z osobnej przetwornicy. Osobne zasilanie często zmniejsza też paski na obrazie.
- Prąd: 1,2 A ciągle (3,2 A chwilowo) na kanał. Przy 4 silnikach użyj dwóch TB6612, tak jak dwóch L298N niżej. Do Mecanum każdy moduł obsługuje 2 koła.
- W menu ⚙ → Napęd zmniejsz **Minimalną moc startową** do ok. 40–60. Wartość 90 była dobrana pod spadek napięcia na L298N.
- **Częstotliwość PWM:** zacznij od 1 kHz. Przy 20 kHz silniki nie piszczą, ale przy tym sposobie podłączenia mogą słabiej ruszać na małej mocy.

**Uwagi do pinów płytki ESP32-CAM:**
- **VCC** (po prawej) to wyjście 3,3 V albo 5 V, zależnie od zworki na płytce, a nie wejście zasilania. Zasilanie podawaj na pin **5V**.
- **IO16** (po prawej) obsługuje PSRAM. Nie podłączaj do niego niczego.
- **GND/R** (po prawej, na dole): na części klonów ten pin nie jest masą. Jako masy używaj **GND obok 5V**.
- Płytki z kamerą **OV3660** program rozpoznaje sam. Przy pierwszym starcie odwraca obraz w pionie i zmniejsza przesycenie kolorów. Model kamery widać w ⚙ → System.

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
- W menu ⚙ → Napęd ustaw **Liczba silników: 4**. Opcja **Odwróć kierunek** odwraca całą stronę naraz, czyli oba moduły.
- Obrót w miejscu przy 4 kołach wymaga sporo siły, bo koła ślizgają się bokiem. Jeśli robot przy małym wychyleniu tylko buczy, zwiększ w menu **Minimalną moc startową** (np. do 110–130). Pomaga też zasilanie 2S Li-ion (7,4–8,4 V), a nie 4×AA.
- Wersja oszczędna: jeden L298N, a dwa silniki z każdej strony połączone równolegle na jednym kanale (lewe na OUT1/OUT2, prawe na OUT3/OUT4). Działa, ale moduł mocno się grzeje.

## Koła Mecanum

Koła Mecanum pozwalają jeździć bokiem i po skosie. Każdy z 4 silników musi mieć osobny kanał, więc potrzebne są dwa L298N i **8 pinów**. W menu ⚙ → Napęd wybierz **Mecanum** i przypisz piny każdemu kołu. Domyślnie:

```
 L298N #1 (przód)                     L298N #2 (tył)
 IN1 ── GPIO 14  lewe przednie A      IN1 ── GPIO 2   lewe tylne A
 IN2 ── GPIO 15  lewe przednie B      IN2 ── GPIO 4   lewe tylne B
 IN3 ── GPIO 13  prawe przednie A     IN3 ── GPIO 1   prawe tylne A
 IN4 ── GPIO 12  prawe przednie B     IN4 ── GPIO 3   prawe tylne B
```

Ograniczenia: ESP32-CAM ma tylko 8 wolnych pinów, więc w trybie Mecanum zajęte są wszystkie.
- **Nie zostaje pin dla odbiornika ELRS ani dla diody.** Robot sterujesz wtedy tylko z telefonu.
- **GPIO 1 i 3** to port programatora: Serial Monitor jest wyłączony, a przy wgrywaniu programu trzeba odłączyć przewody z tych pinów. GPIO 1 przy starcie wysyła dane, więc podłączone koło może na chwilę drgnąć.
- **GPIO 4** steruje też diodą flash, więc dioda świeci podczas jazdy.

Sterowanie Mecanum z telefonu: **lewa gałka jest okrągła** (przód/tył i jazda bokiem), a **prawa obraca robota**. Pierwszy test zrób z robotem podniesionym:
1. Do przodu: wszystkie 4 koła kręcą się do przodu. Jeśli któreś kręci się odwrotnie, zaznacz je w menu „Odwróć kierunek”.
2. Bokiem: koła po przekątnej (lewe przednie i prawe tylne, prawe przednie i lewe tylne) kręcą się w tę samą stronę, a przednie i tylne po tej samej stronie w przeciwne. Jeśli na podłodze robot jedzie bokiem w złą stronę albo zamiast bokiem jedzie do przodu, zamień miejscami koła przednie z tylnymi, bo rolki muszą tworzyć właściwy wzór.

## Edytor układu ekranu

W ⚙ → Przyciski → **„Edytuj układ ekranu”** elementy na ekranie dostają ramki:
- **przesuwasz** je palcem;
- **dotknięcie** otwiera panel z rozmiarem i funkcją;
- **rozmiar** zmienisz też dwoma palcami.

Pasek u góry ma przyciski: dodaj gałkę, przycisk, pole kamery, światło, moc; krycie elementów; układ domyślny; „Gotowe”. „Gotowe” zapisuje układ w robocie, więc działa na każdym telefonie.

Robot przechowuje **dwa osobne układy: poziomy i pionowy**. Obróć telefon, żeby ustawić drugi.

- **Gałka:**
  - **funkcja każdej osi osobno:** lewo/prawo: skręt, jazda bokiem (Mecanum), kamera pan albo dowolne serwo; góra/dół: gaz, kamera tilt albo dowolne serwo;
  - **kształt dobiera się sam:** jedna oś to tor, dwie osie to okrągła gałka;
  - **pływająca** (pojawia się pod palcem) albo stała;
  - **powrót na środek:** bez powrotu gaz działa jak tempomat, a kamera zostaje tam, gdzie ją ustawisz.
- **Przycisk:** funkcję wybierasz od razu w panelu (te same funkcje co w zakładce Przyciski).
- **Pole kamery:** przeciąganie palcem obraca kamerę na serwach pan/tilt, dwuklik ją centruje.
- **Światło i moc:** można je przesunąć, zmienić ich rozmiar albo usunąć.

## Serwa, PCA9685 i własne przyciski

**Serwa** (menu ⚙ → Serwa), np. kamera na uchwycie pan/tilt, chwytak albo ramię:
- Do 6 serw: 2 na pinach ESP32 (zwykle U0T/U0R albo IO2, jeśli nie ma ELRS), reszta na module PCA9685.
- Każde ma nazwę, zakres ruchu (min/max), środek, odwrócenie, prędkość ruchu i przyciski testowe.
- **Sterowanie:**
  - **Przeciąganie obrazu:** serwo z rolą *pan* albo *tilt* jest sterowane polem kamery (element układu) albo gałką z osią „kamera – pan/tilt”. Pole kamery pojawia się w układzie domyślnym samo.
  - **Kanał aparatury:** np. pokrętło albo suwak w nadajniku.
  - **Tylko przyciski.**
- Serwa zasilaj z przetwornicy 5 V (min. 2 A), nie ze stabilizatora L298N. Każde serwo pobiera chwilowo do 0,7 A.

**Moduł PCA9685** (16 kanałów PWM przez I2C) zajmuje 2 piny ESP32 (SDA, SCL), np. U0T i U0R. Na jego kanały można przypisać serwa, a także **silniki, diodę i wyjścia przycisków**. Dzięki temu Mecanum, ELRS, dioda i serwa działają jednocześnie.
- Podłączenie: VCC do 3V3, GND do masy, SDA i SCL do wybranych pinów, V+ do 5–6 V z przetwornicy (zasilanie serw).
- Częstotliwość PWM jest wspólna dla wszystkich kanałów. Zwykłe serwa potrzebują 50 Hz.
- Silniki na kanałach PCA działają z tą samą częstotliwością co serwa (np. 50 Hz). Jadą, ale mogą buczeć.

**Własne przyciski** (menu ⚙ → Przyciski). Do 8 przycisków. Na ekran dodajesz je w edytorze układu. Każdy ma swój napis (może być emoji) i jedną z funkcji:
- **Światło:** włącza i wyłącza diodę.
- **Wyjście:** włącza i wyłącza pin ESP32 albo kanał PCA, np. dodatkowe światła, buzzer albo przekaźnik przez moduł z tranzystorem. Działa jako przełącznik albo tylko na przytrzymanie.
- **Serwo:** ustawia serwo w pozycji A, a po wyłączeniu w pozycji B, np. otwiera i zamyka chwytak.
- **Kamera na środek.**
- **Moc:** ustawia wybraną moc.
- **Turbo:** daje 100% mocy, dopóki trzymasz przycisk.

Światło, wyjścia, serwa i centrowanie kamery można też przypisać do przełącznika w aparaturze. Wtedy stan przełącznika jest stanem funkcji.

Schemat połączeń (⚙ → Napęd albo Serwa) pokazuje moduł PCA9685 z opisem każdego kanału, serwa i wyjścia przycisków.

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

Domyślne kanały (tryb 2). Każdy można zmienić w menu ⚙ → ELRS, gdzie jest też podgląd wszystkich 16 kanałów na żywo:
- **CH1** (prawy drążek lewo/prawo): skręt. W trybie Mecanum to jazda bokiem, a obrót przechodzi na **CH4** (lewy drążek lewo/prawo).
- **CH2** (prawy drążek góra/dół) – jazda przód/tył
- **CH3** (gaz) – ograniczenie mocy, od 25% przy gazie na dole do 100% przy gazie na górze
- **CH5 / AUX1** – przejęcie sterowania przez aparaturę. Uzbrojenie zadziała tylko wtedy, gdy drążek jazdy jest w środku.
- **CH6 / AUX2** – światło. Przełącznik zmienia stan przy każdym przestawieniu, a przycisk na stronie działa nadal.

Ważne:
- W ustawieniach odbiornika ELRS (Lua lub WebUI) ustaw **Failsafe = No Pulses** (domyślne). W trybie „Last Position” robot po utracie zasięgu jechałby dalej.
- Odbiornik powinien pracować w trybie CRSF/Serial (domyślny) na 420000 bodów.
- GPIO 2 jest pinem rozruchowym. **Przy wgrywaniu programu odłącz przewód z GPIO 2**, inaczej ESP32 może nie wejść w tryb programowania. W normalnej pracy to nie przeszkadza.
- Pin odbiornika zmienisz w menu ⚙ → ELRS. Ustaw „brak”, jeśli nie używasz odbiornika.

## Wi-Fi

W menu ⚙ → Wi-Fi wybierasz:
- **Sieć robota** (domyślnie): robot tworzy własną sieć. Ustawiasz jej nazwę, hasło (min. 8 znaków) i kanał. Strona jest pod http://192.168.4.1.
- **Sieć domowa**: robot łączy się z routerem. Przycisk „Szukaj” pokazuje sieci w zasięgu. Strona jest pod adresem IP nadanym przez router albo pod http://robot.local; nazwę `robot` możesz zmienić. Nie każdy telefon obsługuje adresy `.local`, w szczególności część telefonów z Androidem.
  - Opcja **„Zostaw też sieć robota”** (zalecana) utrzymuje równolegle sieć robota, więc zawsze da się połączyć przez 192.168.4.1.
  - Jeśli robot nie połączy się z routerem w 15 s, a sieć robota jest wyłączona, uruchomi ją awaryjnie.

**Awaryjny reset Wi-Fi:** włącz i wyłącz robota 3 razy z rzędu, za każdym razem na mniej niż 10 sekund. Wi-Fi wróci do ustawień domyślnych: sieć `ESP32-Robot`, hasło `robot1234`.

## Wgrywanie (Arduino IDE)

1. W *Preferencje → Dodatkowe adresy URL menedżera płytek* wpisz:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
2. W *Menedżerze płytek* zainstaluj **esp32 by Espressif Systems** (wersja 2.x lub 3.x).
3. Otwórz `esp32_cam_robot/esp32_cam_robot.ino`.
4. Wybierz *Płytka*: **AI Thinker ESP32-CAM**. Ustawienie *Partition Scheme* nie ma znaczenia, bo plik `partitions.csv` w folderze szkicu wymusza układ pamięci z aktualizacją przez Wi-Fi.
5. Podłącz ESP32-CAM na programatorze ESP32-CAM-MB przez USB, wybierz port i kliknij *Wgraj*.
   Jeśli wgrywanie się nie zaczyna, przytrzymaj **IO0** na programatorze, naciśnij i puść **RST**, a potem puść IO0.
6. Otwórz Serial Monitor (115200), naciśnij RST i sprawdź komunikaty.

**Bez Arduino IDE:** gotowy firmware znajdziesz w zakładce **Releases**. Plik `…-full.bin` wgrywasz pod adres `0x0` przez https://espressif.github.io/esptool-js/ (Chrome/Edge). Szczegóły są w opisie wydania. Wgranie `full.bin` kasuje ustawienia robota.

**Aktualizacja przez Wi-Fi (od wersji 1.3.0):** w menu ⚙ → System wybierz plik `…-app.bin` z Releases i kliknij „Wgraj aktualizację”. Nie potrzebujesz kabla ani programatora, a ustawienia zostają. Pierwszy raz, przy przejściu ze starszej wersji, trzeba wgrać `full.bin` kablem, bo zmienia się układ pamięci.

**Kopia ustawień:** w menu ⚙ → System zapiszesz wszystkie ustawienia do pliku, a potem wczytasz je z powrotem, np. po wgraniu `full.bin` albo na drugim robocie. Hasła Wi-Fi nie trafiają do pliku.

## Konfiguracja

Obraz, napęd, piny i kanały aparatury ustawiasz na stronie, w menu ⚙. W pliku `esp32_cam_robot.ino` zostały tylko:

| Ustawienie | Opis |
|---|---|
| `DEF_AP_SSID`, `DEF_AP_PASS`, `DEF_HOSTNAME` | domyślne Wi-Fi (pierwszy start i awaryjny reset); zwykłe ustawienia są w menu ⚙ → Wi-Fi |
| `HW_DEFAULTS` | domyślne piny, napęd i kanały aparatury (używane przy pierwszym uruchomieniu i po „Przywróć domyślne”) |
| `CAM_VFLIP`, `CAM_HMIRROR` | domyślne odbicie obrazu (potem zmienisz je w menu ⚙ na stronie) |

Jeśli robot skręca w lewo, gdy wychylasz gałkę w prawo, zamień miejscami przewody silnika lewego i prawego (OUT1/2 ↔ OUT3/4) albo zamień piny lewej i prawej strony w menu ⚙ → Napęd.

**Pełny ekran:** przycisk ⛶ obok ⚙ chowa pasek adresu i pasek systemu, a ekran obraca się do poziomu, nawet gdy trzymasz telefon pionowo. Działa w Chrome i Edge na Androidzie oraz na komputerze. W ⚙ → Przyciski możesz włączyć „Włączaj po pierwszym dotknięciu”, bo przeglądarka nie pozwala włączyć pełnego ekranu bez dotknięcia. Opcja „Obracaj ekran do poziomu” decyduje o obrocie. Oba ustawienia zapisują się osobno na każdym telefonie.

**iPhone:** Safari nie pozwala stronom na pełny ekran. Użyj Udostępnij → „Do ekranu początkowego”: strona otworzy się jak aplikacja, bez paska adresu.

## Nowe wydanie (dla autora)

*Actions → Release → Run workflow* i podajesz numer wersji (np. `v1.1.0`). Workflow skompiluje firmware i utworzy Release z plikami `.bin` oraz opisem z `.github/release-notes.md`.
