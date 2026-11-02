Firmware dla robota na **ESP32-CAM (AI-Thinker) + L298N**: sterowanie z telefonu (analog na ekranie + podgląd z kamery) oraz opcjonalnie z aparatury ExpressLRS.

## Co potrafi

- własna sieć Wi-Fi **`ESP32-Robot`** (hasło `robot1234`), strona sterowania pod **http://192.168.4.1**
- dwie gałki jak w grach (lewa: gaz, prawa: skręt), regulacja mocy 20–100%, włącznik diody flash
- podgląd z kamery z menu ustawień obrazu (rozdzielczość do 1280×720, obrót, odbicia, jasność, kontrast, balans bieli, ekspozycja, efekty), licznik FPS
- napęd 2WD lub 4WD (skręt czołgowy), opcjonalnie dwa L298N
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
