Firmware dla robota na **ESP32-CAM (AI-Thinker) + L298N**: sterowanie z telefonu (dwie gałki na ekranie + podgląd z kamery) oraz opcjonalnie z aparatury ExpressLRS.

## Co nowego w v1.5.1

- **pełny ekran**: przycisk ⛶ obok ⚙ chowa pasek adresu i obraca ekran do poziomu (Chrome/Edge na Androidzie); opcja „Włączaj po pierwszym dotknięciu”
- iPhone: „Do ekranu początkowego” otwiera stronę jak aplikację bez paska adresu (manifest i ikona)

## Co nowego w v1.5.0

- **edytor układu ekranu** jak w emulatorach gier: przesuwanie elementów palcem, zmiana rozmiaru (suwak albo dwa palce), przezroczystość, osobne układy dla telefonu trzymanego poziomo i pionowo; układ zapisuje się w robocie
- **gałki z przypisywanymi funkcjami osi**: skręt, gaz, jazda bokiem, kamera pan/tilt albo dowolne serwo; pływające albo stałe; z powrotem na środek albo bez (tempomat, kamera zostaje w miejscu)
- przyciski, pole kamery, światło i moc to elementy układu – dodajesz, przesuwasz i ustawiasz ich funkcję w edytorze
- kopia ustawień obejmuje układ ekranu

## Co nowego w v1.4.0

- **serwa** (do 6): zakres, środek, odwrócenie, prędkość; sterowanie przeciąganiem obrazu (kamera pan/tilt, dwuklik = środek), kanałem aparatury albo przyciskami
- **moduł PCA9685** (16 kanałów przez I2C na 2 pinach): serwa, ale też silniki, dioda i wyjścia przycisków – koniec z brakiem pinów (np. Mecanum + ELRS + serwa)
- **własne przyciski na ekranie** (do 8) z przypisywanymi funkcjami: światło, wyjście (przełącznik / przytrzymanie), serwo A/B, kamera na środek, moc, turbo; również z przełączników aparatury
- schemat połączeń: karta PCA9685 z opisem kanałów, serwa i wyjścia; nowe zakładki menu w dwóch rzędach
- kopia ustawień obejmuje serwa, przyciski i PCA9685

## Co nowego w v1.3.2

- schemat połączeń: **rysunki modułów L298N i TB6612FNG** z opisem każdego pinu i zacisku (w kolorach zgodnych z rysunkiem ESP32-CAM)
- nazwy pinów jak na nadruku płytki: IO14, U0T (IO1), U0R (IO3) zamiast „GPIO 14”

## Co nowego w v1.3.1

- **schemat połączeń** w menu Napęd i ELRS: rysunek płytki ESP32-CAM z opisem każdego pinu i karty modułów (sterownik, przetwornica, odbiornik), liczone na żywo z ustawień
- wybór sterownika silników: **L298N** albo **TB6612FNG** (nazwy pinów i zasilanie na schemacie)
- wykrywanie modelu kamery (widoczny w ⚙ → System); dla **OV3660** przy pierwszym starcie obraz jest odwracany w pionie, a kolory mniej przesycone (jak w przykładzie Espressif)
- polecenia, których dana kamera nie obsługuje, są pomijane zamiast ryzykować zawieszenie
- README: podłączenie sterownika **TB6612FNG** i opis pinów płytki ESP32-CAM

## Co nowego w v1.3.0

**Ważne – jednorazowo kablem:** ta wersja zmienia układ pamięci, żeby dało się aktualizować przez Wi-Fi. Wgraj `…-full.bin` programatorem, tak jak dotąd. Wgranie `full.bin` kasuje ustawienia, więc wcześniej zanotuj piny i ustawienia Wi-Fi. Następne wersje wgrasz już przez Wi-Fi (plik `…-app.bin`), a ustawienia zostaną.

- **zakładka System:** wersja, czas pracy, przyczyna ostatniego restartu, wolna pamięć RAM i PSRAM, miejsce na program
- **aktualizacja przez Wi-Fi:** wgranie pliku `…-app.bin` z poziomu strony, z paskiem postępu
- **kopia ustawień:** zapis wszystkich ustawień do pliku i wczytanie z pliku (bez haseł Wi-Fi)
- **Napęd:** czułość skrętu, maksymalna moc, płynny rozruch, martwa strefa gałek, czas failsafe, częstotliwość PWM, jasność diody
- **ELRS:** moc przy gazie na dole, martwa strefa drążków, czas utraty sygnału, odwracanie kanałów
- **Obraz – zakłócenia:** taktowanie kamery 10/20 MHz, limit wzmocnienia, korekcja białych/czarnych pikseli i ciemnych rogów
- **zakładka Wi-Fi:** sieć robota (nazwa, hasło, kanał) albo sieć domowa z wyszukiwaniem sieci; opcja utrzymania sieci robota równolegle; adres http://robot.local (nazwa do zmiany)
- awaryjny reset Wi-Fi: 3 włączenia robota z rzędu, każde krótsze niż 10 s
- pisanie w polach menu nie steruje już robotem (WASD/F)

## Co nowego w v1.2.0

- **menu ⚙ z zakładkami Obraz / Napęd / ELRS**
- **Napęd:** czołg albo **koła Mecanum** (jazda bokiem, 4 silniki, 8 pinów), liczba silników, wybór pinów L298N z ostrzeżeniami o konfliktach, odwrócenie kierunku każdej strony lub koła, minimalna moc startowa, pin diody
- **ELRS:** pin odbiornika, przypisanie kanałów do funkcji i podgląd 16 kanałów na żywo z LQ
- w trybie Mecanum lewa gałka jest okrągła (przód/tył + bok), prawa obraca robota; na komputerze Q/E to jazda bokiem
- pod gałkami widać rzeczywistą moc każdego koła, także przy sterowaniu z aparatury
- uwaga: piny i kanały ustawione wcześniej w kodzie (`#define`) są teraz w menu; przy pierwszym starcie obowiązują domyślne (silniki 14/15/13/12, dioda 4, ELRS 2)

## Co nowego w v1.1.2

- **ważna poprawka:** po wybraniu w menu 1024×768 lub HD (albo powrocie z nich) robot mógł przestać reagować na diodę i sterowanie, w tym failsafe. Restart kamery już nie blokuje sterowania, a na czas restartu (ok. 1 s) silniki stają.

## Co nowego w v1.1.1

- poprawka płynności obrazu: kamera znów startuje z buforem 800×600, tak jak w v1.0. W v1.1.0 startowała z buforem HD, co mogło obniżać FPS. Przy wyborze 1024×768 lub HD kamera sama uruchamia się ponownie z większym buforem; obraz znika wtedy na ok. sekundę.
- przy domyślnych ustawieniach obrazu kamera dostaje te same polecenia co w v1.0; pozostałe ustawienia są wysyłane tylko wtedy, gdy różnią się od domyślnych
- obok FPS strona pokazuje średni rozmiar klatki w KB, co pomaga ocenić, czy ogranicza kamera, czy Wi-Fi
- Serial Monitor co 5 s wypisuje FPS i rozmiar klatki

## Co nowego w v1.1.0

- **sterowanie na dwa kciuki:** lewa gałka to gaz, prawa to skręt; gałki dopasowują się do wysokości ekranu
- **menu ustawień obrazu (⚙):** rozdzielczość do 1280×720, kompresja JPEG, obrót 0/90/180/270°, odbicia, jasność, kontrast, nasycenie, balans bieli, ekspozycja, efekty; zapisywane w pamięci robota
- **licznik FPS** obok statusu połączenia
- trzy przyciski jakości zastąpił szybki wybór w menu (Płynnie / Balans / Jakość)

## Co potrafi

- domyślnie własna sieć Wi-Fi **`ESP32-Robot`** (hasło `robot1234`), strona sterowania pod **http://192.168.4.1**; w menu można przełączyć na sieć domową
- dwie gałki jak w grach (lewa: gaz, prawa: skręt; w trybie Mecanum lewa okrągła z jazdą bokiem), regulacja mocy 20–100%, włącznik diody flash
- podgląd z kamery z menu ustawień obrazu (rozdzielczość do 1280×720, obrót, odbicia, jasność, kontrast, balans bieli, ekspozycja, efekty), licznik FPS
- napęd czołgowy 2WD/4WD albo koła Mecanum; piny i kanały aparatury ustawiane w menu
- odbiornik **ELRS (CRSF)** na GPIO 2: aparatura przejmuje sterowanie po włączeniu AUX1, a po utracie zasięgu wraca telefon
- failsafe: brak sygnału = silniki stop

## Pliki

| Plik | Do czego |
|---|---|
| `esp32-cam-robot-…-full.bin` | **ten wgrywasz**: kompletny obraz (bootloader + partycje + program), adres **0x0** |
| `esp32-cam-robot-…-app.bin` | sam program – do **aktualizacji przez Wi-Fi** (⚙ → System) |
| `bootloader.bin`, `partitions.bin`, `boot_app0.bin` | osobne części (0x1000, 0x8000, 0xe000), gdy wolisz wgrywać je pojedynczo |

## Wgrywanie bez Arduino IDE

**Z przeglądarki (Chrome / Edge):**
1. Załóż ESP32-CAM na programator ESP32-CAM-MB i podłącz go przez USB.
2. **Odłącz przewód od GPIO 2** (odbiornik ELRS), jeśli jest podłączony.
3. Wejdź na https://espressif.github.io/esptool-js/, kliknij **Connect** i wybierz port.
4. Ustaw *Flash Address* na `0x0`, wybierz plik `…-full.bin` i kliknij **Program**.
5. Po zakończeniu naciśnij **RST** na płytce.

**Z linii poleceń:**
```
esptool.py --chip esp32 --baud 460800 write_flash 0x0 esp32-cam-robot-vX.Y.Z-full.bin
```

Jeśli wgrywanie się nie zaczyna, przytrzymaj **IO0** na programatorze, naciśnij i puść **RST**, a potem puść IO0.

W tym firmware ustawienia są domyślne: sieć `ESP32-Robot`/`robot1234`, piny silników GPIO 14/15/13/12, ELRS na GPIO 2. Żeby coś zmienić (nazwę sieci, kierunek kół, kanały aparatury), skompiluj kod ze źródeł według README.
