Firmware dla robota na **ESP32-CAM (AI-Thinker) + L298N**: sterowanie z telefonu (dwie gałki na ekranie + podgląd z kamery) oraz opcjonalnie z aparatury ExpressLRS.

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

- własna sieć Wi-Fi **`ESP32-Robot`** (hasło `robot1234`), strona sterowania pod **http://192.168.4.1**
- dwie gałki jak w grach (lewa: gaz, prawa: skręt; w trybie Mecanum lewa okrągła z jazdą bokiem), regulacja mocy 20–100%, włącznik diody flash
- podgląd z kamery z menu ustawień obrazu (rozdzielczość do 1280×720, obrót, odbicia, jasność, kontrast, balans bieli, ekspozycja, efekty), licznik FPS
- napęd czołgowy 2WD/4WD albo koła Mecanum; piny i kanały aparatury ustawiane w menu
- odbiornik **ELRS (CRSF)** na GPIO 2: aparatura przejmuje sterowanie po włączeniu AUX1, a po utracie zasięgu wraca telefon
- failsafe: brak sygnału = silniki stop

## Pliki

| Plik | Do czego |
|---|---|
| `esp32-cam-robot-…-full.bin` | **ten wgrywasz**: kompletny obraz (bootloader + partycje + program), adres **0x0** |
| `esp32-cam-robot-…-app.bin` | sam program (adres 0x10000), np. do aktualizacji |
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
