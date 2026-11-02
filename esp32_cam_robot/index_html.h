#pragma once

// Strona sterowania: podgląd z kamery na całym ekranie + dwie wirtualne gałki
// (lewa = gaz przód/tył, prawa = skręt lewo/prawo), jak w grach wyścigowych.
// Przycisk ⚙ otwiera menu ustawień obrazu (zapisywane w pamięci robota).
// Trzymaj telefon poziomo. Na komputerze działa też WASD / strzałki, F = światło.
static const char INDEX_HTML[] = R"rawliteral(<!doctype html>
<html lang="pl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-capable" content="yes">
<title>ESP32 Robot</title>
<style>
  :root{--R:85px;--K:76px}   /* zakres ruchu i rozmiar gałki – JS dopasowuje je do ekranu */
  [hidden]{display:none!important}
  *{box-sizing:border-box;-webkit-tap-highlight-color:transparent;user-select:none;-webkit-user-select:none}
  html{color-scheme:dark}
  html,body{margin:0;height:100%;overflow:hidden;background:#000;color:#fff;
    font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;touch-action:none}
  #cam{position:fixed;inset:0;width:100%;height:100%;object-fit:contain;background:#111}
  #nocam{position:fixed;inset:0;display:flex;align-items:center;justify-content:center;
    color:#666;font-size:14px;pointer-events:none}
  .hud{position:fixed;padding:6px 10px;border-radius:10px;background:rgba(0,0,0,.45);
    backdrop-filter:blur(4px);-webkit-backdrop-filter:blur(4px);font-size:13px}
  #status{top:10px;left:10px;display:flex;align-items:center;gap:6px}
  #dot{width:10px;height:10px;border-radius:50%;background:#e33;box-shadow:0 0 6px #e33}
  #dot.ok{background:#2d6;box-shadow:0 0 6px #2d6}
  #rc{top:52px;left:10px;display:none;z-index:2}
  #rc.on{display:block;color:#fd5}
  #rc.armed{display:block;color:#000;background:rgba(80,220,120,.9)}
  .zone.locked{opacity:.3}
  #readout{bottom:10px;left:50%;transform:translateX(-50%);font-variant-numeric:tabular-nums}
  #top{position:fixed;top:8px;left:50%;transform:translateX(-50%);display:flex;gap:8px;
    align-items:center;z-index:2}
  .btn{width:44px;height:44px;border-radius:50%;border:2px solid rgba(255,255,255,.5);
    background:rgba(0,0,0,.45);color:#fff;font-size:20px;display:flex;align-items:center;
    justify-content:center;cursor:pointer}
  .btn.on{background:rgba(255,220,80,.85);color:#000;border-color:#fd5}
  .speed{display:flex;align-items:center;gap:6px;font-size:12px;
    background:rgba(0,0,0,.45);padding:4px;border-radius:12px}
  .speed button{width:40px;height:36px;border-radius:8px;border:1px solid rgba(255,255,255,.4);
    background:rgba(255,255,255,.1);color:#fff;font-size:20px}
  #spd{min-width:42px;text-align:center;font-size:15px;font-weight:600;font-variant-numeric:tabular-nums}
  /* menu ustawień obrazu */
  #menu{position:fixed;top:0;right:0;bottom:0;width:min(340px,100%);z-index:10;display:flex;flex-direction:column;
    background:rgba(16,19,23,.95);border-left:1px solid rgba(255,255,255,.12);touch-action:pan-y}
  #menu[hidden]{display:none}
  .mhead{display:flex;justify-content:space-between;align-items:center;padding:10px 14px;
    border-bottom:1px solid rgba(255,255,255,.1);font-size:15px;font-weight:600}
  .mhead button{width:36px;height:36px;border-radius:8px;border:1px solid rgba(255,255,255,.3);
    background:transparent;color:#fff;font-size:16px}
  .mbody{overflow-y:auto;padding:12px 14px 24px;display:flex;flex-direction:column;gap:14px;touch-action:pan-y}
  .grp{display:flex;flex-direction:column;gap:6px}
  .grp>label,.row>label{font-size:13px;color:#cfd6dc;display:flex;justify-content:space-between;gap:8px}
  .val{color:#fff;font-variant-numeric:tabular-nums}
  .seg{display:flex;gap:4px}
  .seg button{flex:1;height:34px;border-radius:8px;border:1px solid rgba(255,255,255,.3);
    background:rgba(255,255,255,.08);color:#fff;font-size:13px}
  .seg button.on{background:#3b8fe0;border-color:#5aa8f0}
  #menu select{height:36px;border-radius:8px;background:#22282e;color:#fff;
    border:1px solid rgba(255,255,255,.3);padding:0 8px;font-size:14px}
  #menu input[type=range]{width:100%;accent-color:#3b8fe0}
  .row{display:flex;justify-content:space-between;align-items:center}
  .row input[type=checkbox]{width:22px;height:22px;accent-color:#3b8fe0}
  .ends{display:flex;justify-content:space-between;font-size:11px;color:#8a949c}
  .sep{border-top:1px solid rgba(255,255,255,.1)}
  #mreset,.mreset{height:38px;border-radius:8px;border:1px solid rgba(255,140,120,.5);background:transparent;color:#ff9c8a;font-size:14px}
  .note{font-size:11px;color:#8a949c;margin:0}
  .tabs{display:flex;gap:4px;padding:8px 14px 0}
  .tabs button{flex:1;height:34px;border-radius:8px 8px 0 0;border:1px solid rgba(255,255,255,.2);border-bottom:0;
    background:transparent;color:#aab4bc;font-size:13px}
  .tabs button.on{background:rgba(255,255,255,.1);color:#fff}
  .pinrow{display:grid;grid-template-columns:1fr 96px;gap:8px;align-items:center;font-size:13px;color:#cfd6dc}
  .msg{font-size:12px;line-height:1.4;padding:8px 10px;border-radius:8px}
  .msg.warn{background:rgba(255,200,80,.12);color:#ffd27a}
  .msg.err{background:rgba(255,90,80,.15);color:#ff9c8a}
  .msg.ok{background:rgba(80,220,120,.12);color:#8fe3a8}
  .primary{height:40px;border-radius:8px;border:0;background:#3b8fe0;color:#fff;font-size:14px;font-weight:600}
  .primary:disabled{opacity:.4}
  .chv{display:grid;grid-template-columns:44px 1fr 44px;gap:6px;align-items:center;font-size:11px;color:#aab4bc}
  .chv .bar{height:8px;border-radius:4px;background:rgba(255,255,255,.1);position:relative;overflow:hidden}
  .chv .bar i{position:absolute;left:0;top:0;bottom:0;background:#3b8fe0}
  .chv.used{color:#fff}
  .chv .tag{grid-column:2 / 4;font-size:10px;color:#8fc6ff;margin-top:-4px}
  #menu input[type=text],#menu input[type=password]{width:100%;min-width:0;height:36px;border-radius:8px;background:#22282e;color:#fff;
    border:1px solid rgba(255,255,255,.3);padding:0 10px;font-size:14px;user-select:text;-webkit-user-select:text}
  .inrow{display:flex;gap:6px}
  .small{flex:none;height:36px;padding:0 12px;border-radius:8px;border:1px solid rgba(255,255,255,.3);background:rgba(255,255,255,.08);color:#fff;font-size:13px}
  .net{display:flex;justify-content:space-between;gap:8px;text-align:left;height:34px;padding:0 10px;border-radius:8px;
    border:1px solid rgba(255,255,255,.15);background:rgba(255,255,255,.05);color:#fff;font-size:13px;align-items:center}
  .net span{color:#8a949c;font-size:11px;white-space:nowrap}
  .chv{grid-template-columns:44px 1fr 44px 20px}
  .chv input{width:16px;height:16px;accent-color:#3b8fe0;margin:0}
  .chv .tag{grid-column:2 / 5}
  .kv{display:grid;grid-template-columns:max-content minmax(0,1fr);gap:4px 12px;font-size:13px;color:#aab4bc}
  .kv b{color:#fff;font-weight:500;text-align:right;font-variant-numeric:tabular-nums}
  .pbar{height:8px;border-radius:4px;background:rgba(255,255,255,.1);overflow:hidden}
  .pbar i{display:block;height:100%;width:0;background:#3b8fe0;transition:width .2s}
  #menu input[type=file]{font-size:12px;color:#cfd6dc;max-width:100%}
  /* schemat połączeń */
  #wiring{position:fixed;inset:0;z-index:20;background:#0e1215;display:flex;flex-direction:column;touch-action:pan-y}
  #wiring[hidden]{display:none}
  #wiring .mhead{flex:none}
  .wbody{overflow:auto;padding:12px 14px 24px;display:flex;flex-wrap:wrap;gap:14px;align-items:flex-start;touch-action:pan-y}
  .wbody svg{width:440px;max-width:100%;height:auto;flex:none}
  .wcards{display:flex;flex-direction:column;gap:10px;flex:1 1 280px;min-width:0}
  .wcard{border:1px solid rgba(255,255,255,.15);border-radius:10px;padding:10px 12px;background:rgba(255,255,255,.03)}
  .wcard h4{margin:0 0 6px;font-size:13px;font-weight:600}
  .wrow{display:flex;gap:8px;font-size:12px;line-height:1.7;color:#cfd6dc}
  .wrow b{min-width:92px;font-weight:600}
  .wrow .dot{width:8px;height:8px;border-radius:50%;margin-top:7px;flex:none}
  .msvg{width:100%;max-width:320px;height:auto;display:block;margin:0 auto 8px}
  .wnote{font-size:11px;color:#8a949c;margin:6px 0 0}
  .wbtn{height:38px;border-radius:8px;border:1px solid #5aa8f0;background:rgba(59,143,224,.15);color:#cfe6ff;font-size:14px}
  .base.r{width:calc(2 * var(--R) + var(--K) + 14px);height:calc(2 * var(--R) + var(--K) + 14px)}
  .base.r::before,.base.r::after{content:none}
  /* dwie strefy: lewa połowa = gaz, prawa = skręt; gałka pojawia się pod palcem */
  .zone{position:fixed;top:0;bottom:0;width:50%}
  #zl{left:0} #zr{right:0}
  .base{position:absolute;border-radius:999px;border:3px solid rgba(255,255,255,.55);
    background:rgba(255,255,255,.07);transform:translate(-50%,-50%);pointer-events:none;transition:opacity .2s}
  .base.v{width:calc(var(--K) + 14px);height:calc(2 * var(--R) + var(--K) + 14px)}
  .base.h{width:calc(2 * var(--R) + var(--K) + 14px);height:calc(var(--K) + 14px)}
  .base::before,.base::after{position:absolute;font-size:12px;line-height:1;color:rgba(255,255,255,.6)}
  .base.v::before{content:"\25B2";top:7px;left:50%;transform:translateX(-50%)}
  .base.v::after{content:"\25BC";bottom:7px;left:50%;transform:translateX(-50%)}
  .base.h::before{content:"\25C0";left:9px;top:50%;transform:translateY(-50%)}
  .base.h::after{content:"\25B6";right:9px;top:50%;transform:translateY(-50%)}
  .knob{position:absolute;left:50%;top:50%;width:var(--K);height:var(--K);margin:calc(var(--K) / -2) 0 0 calc(var(--K) / -2);
    border-radius:50%;background:radial-gradient(circle at 35% 35%,#fff,#9ad 70%);
    box-shadow:0 2px 10px rgba(0,0,0,.6);opacity:.9;z-index:1}
  #hint{position:fixed;bottom:10px;right:10px;font-size:11px;color:#bbb}
  @media (pointer:coarse){#hint{display:none}}
  @media (orientation:portrait){
    #top{top:56px}
  }
</style>
</head>
<body>
<div id="nocam">brak obrazu z kamery</div>
<img id="cam" alt="">

<div id="status" class="hud"><span id="dot"></span><span id="stxt">łączenie…</span></div>
<div id="rc" class="hud"></div>
<div id="readout" class="hud">L 0 · P 0</div>

<div class="zone" id="zl"><div class="base v"><div class="knob"></div></div></div>
<div class="zone" id="zr"><div class="base h"><div class="knob"></div></div></div>

<div id="top">
  <div class="btn" id="light" title="Światło (F)">&#128161;</div>
  <div class="btn" id="gear" title="Ustawienia">&#9881;</div>
  <div class="speed">
    <button id="down">&minus;</button>
    <span id="spd">70%</span>
    <button id="up">+</button>
  </div>
</div>

<div id="menu" hidden>
  <div class="mhead">Ustawienia <button id="mclose" aria-label="Zamknij">&#10005;</button></div>
  <div class="tabs"><button data-tab="obraz" class="on">Obraz</button><button data-tab="naped">Napęd</button><button data-tab="elrs">ELRS</button><button data-tab="wifi">Wi-Fi</button><button data-tab="sys">System</button></div>
  <div class="mbody" id="tab-obraz">
    <div class="grp"><label>Szybki wybór</label>
      <div class="seg" id="presets"><button data-p="0">Płynnie</button><button data-p="1">Balans</button><button data-p="2">Jakość</button></div>
    </div>
    <div class="grp"><label for="fs">Rozdzielczość</label>
      <select id="fs" data-k="fs">
        <option value="0">320×240</option><option value="1">400×296</option><option value="2">640×480</option>
        <option value="3">800×600</option><option value="4">1024×768</option><option value="5">1280×720 (HD)</option>
      </select>
    </div>
    <div class="grp"><label for="q">Kompresja JPEG <span class="val" data-show="q"></span></label>
      <input type="range" id="q" data-k="q" min="10" max="40" step="1">
      <div class="ends"><span>ładniejszy obraz</span><span>więcej klatek</span></div>
    </div>
    <div class="sep"></div>
    <div class="grp"><label>Obrót</label>
      <div class="seg" data-k="rot"><button data-v="0">0°</button><button data-v="90">90°</button><button data-v="180">180°</button><button data-v="270">270°</button></div>
    </div>
    <div class="row"><label for="hm">Odbicie lustrzane</label><input type="checkbox" id="hm" data-k="hm"></div>
    <div class="row"><label for="vf">Odwróć w pionie</label><input type="checkbox" id="vf" data-k="vf"></div>
    <div class="sep"></div>
    <div class="grp"><label for="br">Jasność <span class="val" data-show="br"></span></label>
      <input type="range" id="br" data-k="br" min="-2" max="2" step="1"></div>
    <div class="grp"><label for="ct">Kontrast <span class="val" data-show="ct"></span></label>
      <input type="range" id="ct" data-k="ct" min="-2" max="2" step="1"></div>
    <div class="grp"><label for="sa">Nasycenie kolorów <span class="val" data-show="sa"></span></label>
      <input type="range" id="sa" data-k="sa" min="-2" max="2" step="1"></div>
    <div class="grp"><label for="wb">Balans bieli</label>
      <select id="wb" data-k="wb">
        <option value="0">Automatyczny</option><option value="1">Słońce</option><option value="2">Pochmurno</option>
        <option value="3">Biuro (świetlówki)</option><option value="4">Dom (żarówki)</option>
      </select>
    </div>
    <div class="sep"></div>
    <div class="row"><label for="ae">Automatyczna ekspozycja</label><input type="checkbox" id="ae" data-k="ae"></div>
    <div class="grp" id="g-al"><label for="al">Korekta ekspozycji <span class="val" data-show="al"></span></label>
      <input type="range" id="al" data-k="al" min="-2" max="2" step="1"></div>
    <div class="grp" id="g-ex"><label for="ex">Czas naświetlania <span class="val" data-show="ex"></span></label>
      <input type="range" id="ex" data-k="ex" min="0" max="1200" step="10">
      <div class="ends"><span>ciemniej, ostrzej w ruchu</span><span>jaśniej</span></div>
    </div>
    <div class="sep"></div>
    <div class="grp"><label for="fx">Efekt</label>
      <select id="fx" data-k="fx">
        <option value="0">Brak</option><option value="1">Negatyw</option><option value="2">Czarno-biały</option>
        <option value="3">Czerwony</option><option value="4">Zielony</option><option value="5">Niebieski</option><option value="6">Sepia</option>
      </select>
    </div>
    <div class="sep"></div>
    <div class="grp"><label>Zakłócenia i szum</label>
      <p class="note">Poziome paski najczęściej biorą się z zasilania (patrz README). Te opcje mogą je ograniczyć.</p>
    </div>
    <div class="grp"><label>Taktowanie kamery</label>
      <div class="seg" data-k="xc"><button data-v="20">20 MHz</button><button data-v="10">10 MHz – mniej pasków</button></div>
    </div>
    <div class="grp"><label for="gc">Limit wzmocnienia</label>
      <select id="gc" data-k="gc">
        <option value="-1">Domyślny</option><option value="0">2× – najmniej szumu</option><option value="1">4×</option>
        <option value="2">8×</option><option value="3">16×</option><option value="4">32×</option><option value="5">64×</option>
        <option value="6">128× – najjaśniej w ciemności</option>
      </select>
    </div>
    <div class="row"><label for="bpc">Korekcja czarnych pikseli</label><input type="checkbox" id="bpc" data-k="bpc"></div>
    <div class="row"><label for="wpc">Korekcja białych pikseli</label><input type="checkbox" id="wpc" data-k="wpc"></div>
    <div class="row"><label for="lenc">Korekcja ciemnych rogów</label><input type="checkbox" id="lenc" data-k="lenc"></div>
    <button id="mreset">Przywróć domyślne</button>
    <p class="note" id="mnote">Ustawienia zapisują się w robocie i zostają po wyłączeniu.</p>
  </div>

  <div class="mbody" id="tab-naped" hidden>
    <button class="wbtn wshow">Schemat połączeń</button>
    <div class="grp"><label>Sterownik silników</label>
      <div class="seg" data-h="drv"><button data-v="0">L298N</button><button data-v="1">TB6612FNG</button></div>
    </div>
    <div class="grp"><label>Typ napędu</label>
      <div class="seg" data-h="mode"><button data-v="0">Czołg</button><button data-v="1">Mecanum</button></div>
    </div>
    <div class="grp"><label>Liczba silników</label>
      <div class="seg" data-h="mot"><button data-v="2">2</button><button data-v="4">4</button></div>
      <p class="note" id="mot-note"></p>
    </div>
    <div class="grp"><label>Piny sterownika silników</label>
      <p class="note">Nazwy jak na płytce ESP32-CAM: IO14 = GPIO 14, U0T = IO1, U0R = IO3.</p>
      <div class="grp" id="pins"></div></div>
    <div class="grp"><label>Odwróć kierunek</label><div class="grp" id="invs"></div></div>
    <div class="grp"><label for="md">Minimalna moc startowa <span class="val" id="mdv"></span></label>
      <input type="range" id="md" min="0" max="200" step="5">
      <div class="ends"><span>0</span><span>zwiększ, jeśli silniki buczą, ale nie ruszają</span></div>
    </div>
    <div class="sep"></div>
    <div class="grp"><label for="tg">Czułość skrętu <span class="val" data-hv="tg"></span></label>
      <input type="range" id="tg" data-hk="tg" min="20" max="100" step="5"></div>
    <div class="grp"><label for="mp">Maksymalna moc <span class="val" data-hv="mp"></span></label>
      <input type="range" id="mp" data-hk="mp" min="20" max="100" step="5"></div>
    <div class="grp"><label for="ac">Płynny rozruch <span class="val" data-hv="ac"></span></label>
      <input type="range" id="ac" data-hk="ac" min="0" max="30" step="1">
      <div class="ends"><span>wyłączony</span><span>3 s do pełnej mocy</span></div></div>
    <div class="grp"><label for="dz">Martwa strefa gałek <span class="val" data-hv="dz"></span></label>
      <input type="range" id="dz" data-hk="dz" min="0" max="30" step="1"></div>
    <div class="grp"><label for="fo">Zatrzymanie bez sygnału z telefonu po <span class="val" data-hv="fo"></span></label>
      <input type="range" id="fo" data-hk="fo" min="200" max="3000" step="100"></div>
    <div class="pinrow"><label for="pf">Częstotliwość PWM</label>
      <select id="pf" data-hk="pf">
        <option value="100">100 Hz</option><option value="250">250 Hz</option><option value="500">500 Hz</option>
        <option value="1000">1 kHz</option><option value="2000">2 kHz</option><option value="5000">5 kHz</option>
        <option value="10000">10 kHz</option><option value="20000">20 kHz</option>
      </select></div>
    <p class="note">Niższa częstotliwość: lepszy rozruch na małej mocy, ale silniki piszczą. Wyższa: ciszej, ale L298N słabiej ciągnie. Zmiana działa po ponownym uruchomieniu.</p>
    <div class="sep"></div>
    <div class="pinrow"><label for="lp">Pin diody flash</label><select id="lp" data-pin="lp"></select></div>
    <div class="grp"><label for="lt">Jasność diody <span class="val" data-hv="lt"></span></label>
      <input type="range" id="lt" data-hk="lt" min="1" max="100" step="1"></div>
    <div class="grp" id="hwmsg"></div>
    <button class="primary hwreboot">Zapisz i uruchom ponownie</button>
    <p class="note">Piny i typ napędu zaczną działać po ponownym uruchomieniu robota. Pozostałe ustawienia działają od razu.</p>
    <button id="hwreset" class="mreset">Przywróć domyślne</button>
  </div>

  <div class="mbody" id="tab-elrs" hidden>
    <button class="wbtn wshow">Schemat połączeń</button>
    <div class="pinrow"><label for="rp">Pin odbiornika (TX odbiornika)</label><select id="rp" data-pin="rp"></select></div>
    <div class="msg" id="rcst">…</div>
    <div class="grp"><label>Przypisanie kanałów</label><div class="grp" id="chmap"></div></div>
    <div class="grp"><label for="rm">Moc przy gazie na dole <span class="val" data-hv="rm"></span></label>
      <input type="range" id="rm" data-hk="rm" min="0" max="100" step="5"></div>
    <div class="grp"><label for="rd">Martwa strefa drążków <span class="val" data-hv="rd"></span></label>
      <input type="range" id="rd" data-hk="rd" min="0" max="30" step="1"></div>
    <div class="grp"><label for="rt">Utrata sygnału po <span class="val" data-hv="rt"></span></label>
      <input type="range" id="rt" data-hk="rt" min="100" max="2000" step="50"></div>
    <div class="grp"><label>Podgląd kanałów na żywo</label>
      <p class="note">Ostatnia kolumna odwraca kanał, np. gdy drążek działa w złą stronę.</p>
      <div class="grp" id="chview"></div></div>
    <div class="grp" id="rcmsg"></div>
    <button class="primary hwreboot">Zapisz i uruchom ponownie</button>
    <p class="note">Przypisanie kanałów działa od razu, zmiana pinu po ponownym uruchomieniu.</p>
  </div>

  <div class="mbody" id="tab-wifi" hidden>
    <div class="grp" id="wst"></div>
    <div class="grp"><label>Tryb</label>
      <div class="seg" id="wmode"><button data-v="0">Sieć robota</button><button data-v="1">Sieć domowa</button></div>
    </div>
    <div class="grp" id="w-sta">
      <label for="wss">Sieć domowa (router)</label>
      <div class="inrow"><input type="text" id="wss" maxlength="32" autocomplete="off"><button class="small" id="wscan">Szukaj</button></div>
      <div class="grp" id="wlist"></div>
      <label for="wsp">Hasło sieci domowej</label>
      <input type="password" id="wsp" maxlength="63" placeholder="bez zmian" autocomplete="new-password">
      <div class="row"><label for="wkeep">Zostaw też sieć robota (zalecane)</label><input type="checkbox" id="wkeep"></div>
    </div>
    <div class="grp" id="w-ap">
      <label for="was">Nazwa sieci robota</label>
      <input type="text" id="was" maxlength="32" autocomplete="off">
      <label for="wap">Hasło sieci robota (min. 8 znaków)</label>
      <input type="password" id="wap" maxlength="63" placeholder="bez zmian" autocomplete="new-password">
      <div class="pinrow"><label for="wch">Kanał</label><select id="wch"></select></div>
    </div>
    <div class="grp"><label for="whn">Nazwa robota w sieci domowej</label>
      <input type="text" id="whn" maxlength="24" autocomplete="off">
      <p class="note" id="whn-note"></p>
    </div>
    <div class="grp" id="wmsg"></div>
    <button class="primary" id="wapply">Zapisz i uruchom ponownie</button>
    <p class="note" id="wnote-reset">Nie możesz się połączyć z robotem? Włącz i wyłącz go 3 razy z rzędu, za każdym razem na mniej niż 10 sekund. Wi-Fi wróci do ustawień domyślnych: sieć ESP32-Robot, hasło robot1234.</p>
  </div>

  <div class="mbody" id="tab-sys" hidden>
    <div class="kv" id="sysinfo"></div>
    <div class="sep"></div>
    <div class="grp"><label>Aktualizacja przez Wi-Fi</label>
      <p class="note">Pobierz z GitHub (zakładka Releases) plik kończący się na <b>-app.bin</b> i wybierz go tutaj. Ustawienia zostają.</p>
      <input type="file" id="otafile" accept=".bin">
      <div class="pbar" id="otabar"><i></i></div>
      <button class="primary" id="otago" disabled>Wgraj aktualizację</button>
      <div class="grp" id="otamsg"></div>
    </div>
    <div class="sep"></div>
    <div class="grp"><label>Kopia ustawień</label>
      <div class="inrow"><button class="small" id="bkdown">Zapisz do pliku</button><button class="small" id="bkpick">Wczytaj z pliku</button></div>
      <input type="file" id="bkfile" accept=".json,application/json" hidden>
      <p class="note">Plik zawiera ustawienia obrazu, napędu, aparatury i Wi-Fi, bez haseł.</p>
      <div class="grp" id="bkmsg"></div>
    </div>
    <div class="sep"></div>
    <button class="mreset" id="sysreboot">Uruchom robota ponownie</button>
  </div>
</div>
<div id="wiring" hidden>
  <div class="mhead">Schemat połączeń <button id="wclose" aria-label="Zamknij">&#10005;</button></div>
  <div class="wbody"><div id="wsvg"></div><div class="wcards" id="wcards"></div></div>
</div>
<div id="hint" class="hud">WASD / strzałki · Q/E bokiem (Mecanum) · F światło</div>

<script>
(function(){
  var host = location.hostname || '192.168.4.1';
  var RADIUS = 85;          // zakres ruchu gałki (px)
  var DEADZONE = 0.08;      // martwa strefa gałek (z ustawień robota)
  var TURN = 1;             // czułość skrętu ustawia robot (menu Napęd)

  // ---------- kamera ----------
  var cam = document.getElementById('cam');
  function startCam(){ cam.src = 'http://' + host + ':81/stream?t=' + Date.now(); }
  cam.onload = function(){ cam.style.visibility = ''; document.getElementById('nocam').style.display = 'none'; };
  cam.onerror = function(){ cam.style.visibility = 'hidden'; document.getElementById('nocam').style.display = 'flex'; setTimeout(startCam, 2000); };
  startCam();

  // ---------- WebSocket ----------
  var ws = null, dot = document.getElementById('dot'), stxt = document.getElementById('stxt');
  function connect(){
    ws = new WebSocket('ws://' + host + '/ws');
    ws.onopen = function(){ dot.className = 'ok'; stxt.textContent = 'połączono'; loadSettings(); loadHw(); };
    ws.onmessage = function(e){
      var p = String(e.data).split(' ');
      if (p[0] === 'V') return showChannels(p);
      if (p[0] !== 'R') return;
      if (p.length >= 9) showWheels([+p[5], +p[6], +p[7], +p[8]]);
      showRc(+p[1]);
      var l = p[2] === '1';
      if (l !== light){ light = l; lightEl.classList.toggle('on', light); }
      if (p[3] !== undefined) stxt.textContent = 'połączono · ' + p[3] + ' fps' + (p[4] ? ' · ' + p[4] + ' KB' : '');
    };
    ws.onclose = function(){ showRc(0); dot.className = ''; stxt.textContent = 'rozłączono'; setTimeout(connect, 1000); };
    ws.onerror = function(){ try{ ws.close(); }catch(e){} };
  }
  var rcEl = document.getElementById('rc');
  function showRc(st){
    rcEl.className = 'hud' + (st === 2 ? ' armed' : st === 1 ? ' on' : '');
    rcEl.textContent = st === 2 ? 'Aparatura steruje' : st === 1 ? 'Aparatura połączona' : '';
    zl.classList.toggle('locked', st === 2);
    zr.classList.toggle('locked', st === 2);
  }
  function send(msg){ if (ws && ws.readyState === 1) ws.send(msg); }
  connect();

  // ---------- moc ----------
  var speed = 70, spdEl = document.getElementById('spd');
  function setSpeed(v){ speed = Math.max(20, Math.min(100, v)); spdEl.textContent = speed + '%'; }
  tap('up', function(){ setSpeed(speed + 10); });
  tap('down', function(){ setSpeed(speed - 10); });

  // ---------- światło ----------
  var light = false, lightEl = document.getElementById('light');
  function sendLight(){ send('L ' + (light ? 1 : 0)); }
  function toggleLight(){ light = !light; lightEl.classList.toggle('on', light); sendLight(); }
  tap('light', toggleLight);

  // ---------- ustawienia obrazu ----------
  var menu = document.getElementById('menu');
  var camS = {};
  var PRESETS = [ { fs: 0, q: 14 }, { fs: 2, q: 12 }, { fs: 3, q: 10 } ];   // Płynnie / Balans / Jakość
  tap('gear', function(){ menu.hidden = !menu.hidden; });
  var tab = 'obraz';
  var tabBtns = menu.querySelectorAll('.tabs button');
  for (var ti = 0; ti < tabBtns.length; ti++) tabBtns[ti].addEventListener('click', function(){
    tab = this.dataset.tab;
    for (var j = 0; j < tabBtns.length; j++) tabBtns[j].classList.toggle('on', tabBtns[j] === this);
    ['obraz', 'naped', 'elrs', 'wifi', 'sys'].forEach(function(t){ document.getElementById('tab-' + t).hidden = t !== tab; });
    if (tab === 'sys') loadSys();
    if (tab === 'naped' || tab === 'elrs') loadHw();
    if (tab === 'wifi') loadWifi();
  });
  document.getElementById('mclose').addEventListener('click', function(){ menu.hidden = true; });

  function loadSettings(){
    fetch('/settings', { cache: 'no-store' }).then(function(r){ return r.json(); })
      .then(function(j){ camS = j; renderSettings(); }).catch(function(){});
  }
  function setCam(k, v){
    camS[k] = v;
    send('C ' + k + ' ' + v);
    renderSettings();
  }
  function fmt(k, v){ return (k === 'q' || k === 'ex' || v <= 0) ? String(v) : '+' + v; }

  function renderSettings(){
    var els = menu.querySelectorAll('[data-k]');
    for (var i = 0; i < els.length; i++){
      var el = els[i], k = el.dataset.k, v = camS[k];
      if (v === undefined) continue;
      if (el.classList.contains('seg')){
        var bs = el.querySelectorAll('button');
        for (var j = 0; j < bs.length; j++) bs[j].classList.toggle('on', +bs[j].dataset.v === v);
      } else if (el.type === 'checkbox') el.checked = !!v;
      else el.value = v;
    }
    var shows = menu.querySelectorAll('[data-show]');
    for (var i2 = 0; i2 < shows.length; i2++){
      var k2 = shows[i2].dataset.show;
      if (camS[k2] !== undefined) shows[i2].textContent = fmt(k2, camS[k2]);
    }
    var pb = document.querySelectorAll('#presets button');
    for (var i3 = 0; i3 < pb.length; i3++){
      var pr = PRESETS[+pb[i3].dataset.p];
      pb[i3].classList.toggle('on', camS.fs === pr.fs && camS.q === pr.q);
    }
    document.getElementById('g-al').hidden = !camS.ae;
    document.getElementById('g-ex').hidden = !!camS.ae;
    if (camS.psram === 0){            // bez PSRAM kamera działa tylko w 320x240
      var opts = document.getElementById('fs').options;
      for (var i4 = 1; i4 < opts.length; i4++) opts[i4].disabled = true;
      document.getElementById('mnote').textContent = 'Brak PSRAM: dostępna tylko rozdzielczość 320×240.';
    }
    applyRotation();
  }

  // obrót robimy w przeglądarce (czujnik umie tylko odbicia)
  function applyRotation(){
    var r = camS.rot || 0, st = cam.style;
    if (r === 90 || r === 270){
      var w = window.innerWidth, h = window.innerHeight;
      st.inset = 'auto'; st.width = h + 'px'; st.height = w + 'px';
      st.left = (w - h) / 2 + 'px'; st.top = (h - w) / 2 + 'px';
    } else {
      st.inset = ''; st.width = ''; st.height = ''; st.left = ''; st.top = '';
    }
    st.transform = r ? 'rotate(' + r + 'deg)' : '';
  }
  window.addEventListener('resize', applyRotation);

  (function bindMenu(){
    var els = menu.querySelectorAll('[data-k]');
    for (var i = 0; i < els.length; i++) (function(el){
      var k = el.dataset.k;
      if (el.classList.contains('seg')){
        el.addEventListener('click', function(e){
          if (e.target.dataset.v !== undefined) setCam(k, +e.target.dataset.v);
        });
      } else if (el.type === 'checkbox'){
        el.addEventListener('change', function(){ setCam(k, el.checked ? 1 : 0); });
      } else if (el.type === 'range'){
        // podczas przesuwania tylko podgląd liczby, wysyłka po puszczeniu suwaka
        el.addEventListener('input', function(){
          var sh = menu.querySelector('[data-show="' + k + '"]');
          if (sh) sh.textContent = fmt(k, +el.value);
        });
        el.addEventListener('change', function(){ setCam(k, +el.value); });
      } else {
        el.addEventListener('change', function(){ setCam(k, +el.value); });
      }
    })(els[i]);
    document.getElementById('presets').addEventListener('click', function(e){
      var p = e.target.dataset.p;
      if (p === undefined) return;
      setCam('fs', PRESETS[+p].fs);
      setCam('q', PRESETS[+p].q);
    });
    document.getElementById('mreset').addEventListener('click', function(){
      send('C reset 1');
      setTimeout(loadSettings, 400);
    });
  })();
  loadSettings();

  function tap(id, fn){
    document.getElementById(id).addEventListener('pointerdown', function(e){ e.preventDefault(); e.stopPropagation(); fn(); });
  }

  // ---------- dwie gałki ----------
  // axis 'y' = gaz (góra/dół), axis 'x' = skręt (lewo/prawo)
  // rozmiar gałek dopasowany do ekranu (w poziomie telefon ma czasem tylko ~250 px wysokości)
  var KNOB = 76;
  function layoutSticks(){
    var h = window.innerHeight, w = window.innerWidth / 2;
    KNOB = h < 340 ? 60 : 76;
    RADIUS = Math.max(36, Math.min(85, Math.floor((Math.min(h, w) - KNOB - 30) / 2)));
    document.documentElement.style.setProperty('--R', RADIUS + 'px');
    document.documentElement.style.setProperty('--K', KNOB + 'px');
  }
  layoutSticks();
  window.addEventListener('resize', layoutSticks);

  // axis: 'y' – tylko góra/dół, 'x' – tylko lewo/prawo, 'xy' – okrągła (Mecanum: jazda + bok)
  function makeStick(zone, axis){
    var base = zone.querySelector('.base'), knob = zone.querySelector('.knob');
    var st = { id: null, cx: 0, cy: 0, vx: 0, vy: 0, edge: false };
    function home(){
      var r = zone.getBoundingClientRect();
      base.style.left = (r.width / 2) + 'px';
      base.style.top = (r.height / 2) + 'px';
      base.style.opacity = '.55';
    }
    function move(e){
      var dx = axis === 'y' ? 0 : e.clientX - st.cx;
      var dy = axis === 'x' ? 0 : e.clientY - st.cy;
      var d = Math.sqrt(dx * dx + dy * dy);
      if (d > RADIUS){ dx = dx / d * RADIUS; dy = dy / d * RADIUS; d = RADIUS; }
      knob.style.transform = 'translate(' + dx + 'px,' + dy + 'px)';
      var n = d / RADIUS;
      var k = n < DEADZONE ? 0 : (n - DEADZONE) / (1 - DEADZONE) / n;   // przeskalowanie poza martwą strefą
      st.vx = dx / RADIUS * k;
      st.vy = -dy / RADIUS * k;                                        // w górę = do przodu
      if (navigator.vibrate && n >= 1 && !st.edge) navigator.vibrate(8);
      st.edge = n >= 1;
    }
    function end(e){
      if (e.pointerId !== st.id) return;
      st.id = null; st.vx = 0; st.vy = 0; st.edge = false;
      knob.style.transform = '';
      home();
      pushNow();
    }
    zone.addEventListener('pointerdown', function(e){
      if (st.id !== null) return;
      e.preventDefault();
      st.id = e.pointerId;
      try { zone.setPointerCapture(e.pointerId); } catch (err) {}
      var r = zone.getBoundingClientRect();
      // środek toru pod palcem, ale przesunięty tak, żeby cały tor mieścił się na ekranie
      var m = RADIUS + KNOB / 2 + 10, cx = e.clientX, cy = e.clientY;
      if (axis !== 'x') cy = Math.max(r.top + m, Math.min(window.innerHeight - m, cy));
      if (axis !== 'y') cx = Math.max(r.left + m, Math.min(r.right - m, cx));
      st.cx = cx; st.cy = cy;                        // gałka pojawia się pod palcem
      base.style.left = (cx - r.left) + 'px';
      base.style.top = (cy - r.top) + 'px';
      base.style.opacity = '1';
      move(e);
    });
    zone.addEventListener('pointermove', function(e){ if (e.pointerId === st.id) move(e); });
    zone.addEventListener('pointerup', end);
    zone.addEventListener('pointercancel', end);
    window.addEventListener('resize', function(){ if (st.id === null) home(); });
    st.setAxis = function(a){
      axis = a;
      base.className = 'base ' + (a === 'xy' ? 'r' : a === 'y' ? 'v' : 'h');
    };
    home();
    return st;
  }
  var zl = document.getElementById('zl'), zr = document.getElementById('zr');
  var gas = makeStick(zl, 'y'), steer = makeStick(zr, 'x');

  // ---------- klawiatura ----------
  var keys = {};
  function typing(e){ var t = e.target.tagName; return t === 'INPUT' || t === 'SELECT' || t === 'TEXTAREA'; }
  window.addEventListener('keydown', function(e){
    if (typing(e)) return;            // pisanie w menu nie steruje robotem
    var k = e.key.toLowerCase();
    if (k === 'f' && !e.repeat) toggleLight();
    keys[k] = true;
  });
  window.addEventListener('keyup', function(e){ keys[e.key.toLowerCase()] = false; });
  window.addEventListener('blur', function(){ keys = {}; });
  function keyAxis(){
    var x = 0, y = 0;
    if (keys['w'] || keys['arrowup']) y += 1;
    if (keys['s'] || keys['arrowdown']) y -= 1;
    if (keys['d'] || keys['arrowright']) x += 1;
    if (keys['a'] || keys['arrowleft']) x -= 1;
    var sd = (keys['e'] ? 1 : 0) - (keys['q'] ? 1 : 0);   // Mecanum: Q/E – jazda bokiem
    return { x: x, y: y, s: sd };
  }

  // ---------- wysyłanie ----------
  var last = '', lastSent = 0, readout = document.getElementById('readout');
  function command(){
    var k = keyAxis();
    var x = steer.id !== null ? steer.vx : k.x;
    var y = gas.id !== null ? gas.vy : k.y;
    var sd = mecanum ? (gas.id !== null ? gas.vx : k.s) : 0;
    var s = speed / 100;
    var X = Math.round(x * TURN * s * 100), Y = Math.round(y * s * 100), S = Math.round(sd * s * 100);
    return mecanum ? 'M ' + X + ' ' + Y + ' ' + S : 'M ' + X + ' ' + Y;
  }
  // moc kół podawana przez robota (działa też przy sterowaniu z aparatury)
  function showWheels(w){
    readout.textContent = mecanum
      ? 'LP ' + w[0] + ' · PP ' + w[1] + ' · LT ' + w[2] + ' · PT ' + w[3]
      : 'L ' + w[0] + ' · P ' + w[1];
  }
  // ---------- napęd i ELRS (menu) ----------
  var mecanum = false, hwS = null;
  var FREE_PINS = [1, 2, 3, 4, 12, 13, 14, 15];
  function pinLabels(mec, tb){
    var IN = tb ? ['AIN1', 'AIN2', 'BIN1', 'BIN2'] : ['IN1', 'IN2', 'IN3', 'IN4'];
    if (!mec) return ['Lewa – ' + IN[0], 'Lewa – ' + IN[1], 'Prawa – ' + IN[2], 'Prawa – ' + IN[3]];
    var W = ['LP', 'LP', 'PP', 'PP', 'LT', 'LT', 'PT', 'PT'];
    return W.map(function(w, i){ return w + ' – ' + (i < 4 ? '#1 ' : '#2 ') + IN[i % 4]; });
  }
  var INV_LABELS = { tank: ['Lewa strona', 'Prawa strona'], mec: ['Lewe przednie', 'Prawe przednie', 'Lewe tylne', 'Prawe tylne'] };
  var CH_FUNCS = ['Skręt / obrót', 'Jazda przód/tył', 'Jazda bokiem (Mecanum)', 'Moc (gaz)', 'Przejęcie sterowania (uzbrojenie)', 'Światło'];

  function loadHw(){
    fetch('/hw', { cache: 'no-store' }).then(function(r){ return r.json(); })
      .then(function(j){ hwS = j; applyMode(); renderHw(); }).catch(function(){});
  }
  // tryb gałek i przycisk światła zależą od konfiguracji aktywnej od startu robota
  function applyMode(){
    mecanum = hwS.bootMode === 1;
    if (hwS.dz !== undefined) DEADZONE = hwS.dz / 100;
    gas.setAxis(mecanum ? 'xy' : 'y');
    lightEl.style.display = hwS.lightOk ? '' : 'none';
  }
  var hwTimer = null;
  function setHw(k, v){
    send('H ' + k + ' ' + v);
    clearTimeout(hwTimer);
    hwTimer = setTimeout(loadHw, 400);   // odśwież stan (np. czy potrzebny restart)
  }
  function pinOptions(sel, val, allowNone){
    var h = allowNone ? '<option value="-1">brak</option>' : '';
    FREE_PINS.forEach(function(p){ h += '<option value="' + p + '">' + gp(p) + (p === 4 ? ' – dioda' : '') + '</option>'; });
    sel.innerHTML = h;
    sel.value = String(val);
  }
  function hwMessages(){
    var err = [], warn = [], n = hwS.mode === 1 ? 8 : 4, mp = hwS.p.slice(0, n);
    mp.forEach(function(p, i){ if (mp.indexOf(p) !== i) err.push('Pin ' + gp(p) + ' jest użyty dla dwóch wejść.'); });
    if (mp.indexOf(-1) >= 0) err.push('Każde wejście L298N musi mieć wybrany pin.');
    if (hwS.lp >= 0 && mp.indexOf(hwS.lp) >= 0) warn.push('Dioda będzie wyłączona: pin ' + gp(hwS.lp) + ' zajmują silniki.');
    if (hwS.rp >= 0 && mp.indexOf(hwS.rp) >= 0) warn.push('Odbiornik ELRS będzie wyłączony: pin ' + gp(hwS.rp) + ' zajmują silniki.');
    if (hwS.rp >= 0 && hwS.rp === hwS.lp) warn.push('Dioda i odbiornik mają ten sam pin – odbiornik będzie wyłączony.');
    var all = mp.concat([hwS.lp, hwS.rp]);
    if (all.indexOf(1) >= 0 || all.indexOf(3) >= 0)
      warn.push('U0T (IO1) i U0R (IO3) to piny programatora: Serial Monitor zostanie wyłączony, a przy wgrywaniu programu odłącz przewody z tych pinów.');
    if (mp.indexOf(1) >= 0) warn.push('U0T (IO1) przy starcie wysyła dane – podłączony silnik może na chwilę drgnąć.');
    if (mp.indexOf(4) >= 0) warn.push('IO4 steruje też diodą flash – będzie świecić podczas jazdy.');
    if (all.indexOf(2) >= 0) warn.push('IO2: przy wgrywaniu programu odłącz od niego przewód.');
    if (all.indexOf(12) >= 0) warn.push('IO12 musi mieć stan niski przy starcie – jeśli robot nie startuje, dodaj rezystor 10 kΩ do GND.');
    if (hwS.drv === 1 && hwS.md > 70) warn.push('TB6612 nie traci napięcia jak L298N – zmniejsz minimalną moc startową do ok. 40–60.');
    if (hwS.mode === 1) warn.push('Mecanum potrzebuje 8 pinów, czyli wszystkich wolnych – nie zostaje pin dla odbiornika ELRS.');
    return { err: err, warn: warn };
  }
  function msgHtml(m){
    var h = '';
    m.err.forEach(function(t){ h += '<div class="msg err">' + t + '</div>'; });
    m.warn.forEach(function(t){ h += '<div class="msg warn">' + t + '</div>'; });
    if (hwS.pending && !m.err.length) h += '<div class="msg ok">Zmiany zapisane – uruchom robota ponownie, żeby zaczęły działać.</div>';
    return h;
  }
  function hwFmt(k, v){
    if (v === undefined) return '';
    if (k === 'ac') return v ? String(v / 10).replace('.', ',') + ' s' : 'wyłączony';
    if (k === 'fo' || k === 'rt') return String(v / 1000).replace('.', ',') + ' s';
    return v + '%';
  }
  function renderHw(){
    if (!hwS) return;
    var mec = hwS.mode === 1, key = mec ? 'mec' : 'tank';
    var segs = menu.querySelectorAll('[data-h]');
    for (var i = 0; i < segs.length; i++){
      var v = hwS[segs[i].dataset.h], bs = segs[i].querySelectorAll('button');
      for (var j = 0; j < bs.length; j++){
        bs[j].classList.toggle('on', +bs[j].dataset.v === (segs[i].dataset.h === 'mot' && mec ? 4 : v));
        bs[j].disabled = segs[i].dataset.h === 'mot' && mec;
      }
    }
    document.getElementById('mot-note').textContent = mec ? 'Mecanum: 4 silniki, każdy na osobnym kanale (dwa L298N, 8 pinów).'
      : hwS.mot === 4 ? '4 silniki: dwa L298N, wejścia IN obu modułów połączone równolegle (te same 4 piny).'
      : '2 silniki: jeden L298N.';
    var ph = '';
    pinLabels(mec, hwS.drv === 1).forEach(function(l, i){ ph += '<div class="pinrow"><span>' + l + '</span><select data-pin="p' + i + '"></select></div>'; });
    var pins = document.getElementById('pins'), pkey = key + hwS.drv;
    if (pins.dataset.key !== pkey){ pins.innerHTML = ph; pins.dataset.key = pkey; }
    var sels = pins.querySelectorAll('select');
    for (var k = 0; k < sels.length; k++) pinOptions(sels[k], hwS.p[k], false);
    pinOptions(document.getElementById('lp'), hwS.lp, true);
    pinOptions(document.getElementById('rp'), hwS.rp, true);
    var ih = '';
    INV_LABELS[key].forEach(function(l, i){
      ih += '<div class="row"><label for="inv' + i + '">' + l + '</label><input type="checkbox" id="inv' + i + '" data-inv="' + i + '"' + (hwS.inv[i] ? ' checked' : '') + '></div>';
    });
    document.getElementById('invs').innerHTML = ih;
    var hk = menu.querySelectorAll('[data-hk]');
    for (var h = 0; h < hk.length; h++) if (hwS[hk[h].dataset.hk] !== undefined) hk[h].value = String(hwS[hk[h].dataset.hk]);
    var hv = menu.querySelectorAll('[data-hv]');
    for (var h2 = 0; h2 < hv.length; h2++) hv[h2].textContent = hwFmt(hv[h2].dataset.hv, hwS[hv[h2].dataset.hv]);
    document.getElementById('md').value = hwS.md;
    document.getElementById('mdv').textContent = hwS.md;
    var m = hwMessages(), html = msgHtml(m);
    document.getElementById('hwmsg').innerHTML = html;
    document.getElementById('rcmsg').innerHTML = html;
    var rb = menu.querySelectorAll('.hwreboot');
    for (var b = 0; b < rb.length; b++) rb[b].disabled = !hwS.pending || m.err.length > 0;
    // przypisanie kanałów
    var ch = '';
    CH_FUNCS.forEach(function(f, i){
      if (i === 2 && !mec) return;
      ch += '<div class="pinrow"><span>' + f + '</span><select data-ch="' + i + '"><option value="0">brak</option>';
      for (var c = 1; c <= 16; c++) ch += '<option value="' + c + '">CH' + c + (c >= 5 && c <= 12 ? ' (AUX' + (c - 4) + ')' : '') + '</option>';
      ch += '</select></div>';
    });
    var chmap = document.getElementById('chmap');
    chmap.innerHTML = ch;
    var cs = chmap.querySelectorAll('select');
    for (var q = 0; q < cs.length; q++) cs[q].value = String(hwS.c[+cs[q].dataset.ch]);
    if (!document.getElementById('chview').children.length){
      var vh = '';
      for (var c2 = 1; c2 <= 16; c2++) vh += '<div class="chv" id="chv' + c2 + '"><span>CH' + c2 + '</span><span class="bar"><i></i></span><span class="us">–</span>' +
        '<input type="checkbox" data-rev="' + (c2 - 1) + '" title="Odwróć CH' + c2 + '"></div>';
      document.getElementById('chview').innerHTML = vh;
    }
    var rv = document.querySelectorAll('[data-rev]');
    for (var r2 = 0; r2 < rv.length; r2++) rv[r2].checked = !!((hwS.rv >> +rv[r2].dataset.rev) & 1);
  }
  // zmiany w zakładkach Napęd / ELRS
  menu.addEventListener('change', function(e){
    var t = e.target;
    if (t.dataset.pin){
      var v = +t.value;
      if (t.dataset.pin === 'lp' || t.dataset.pin === 'rp') hwS[t.dataset.pin] = v; else hwS.p[+t.dataset.pin.slice(1)] = v;
      setHw(t.dataset.pin, v); renderHw();
    } else if (t.dataset.inv !== undefined){
      hwS.inv[+t.dataset.inv] = t.checked ? 1 : 0;
      setHw('inv' + t.dataset.inv, t.checked ? 1 : 0);
    } else if (t.dataset.ch !== undefined){
      hwS.c[+t.dataset.ch] = +t.value;
      setHw('c' + t.dataset.ch, +t.value);
    } else if (t.id === 'md'){
      hwS.md = +t.value;
      setHw('md', hwS.md);
    } else if (t.dataset.hk){
      hwS[t.dataset.hk] = +t.value;
      setHw(t.dataset.hk, +t.value);
      if (t.dataset.hk === 'dz') DEADZONE = +t.value / 100;
    } else if (t.dataset.rev !== undefined){
      var bit = 1 << +t.dataset.rev;
      hwS.rv = t.checked ? (hwS.rv | bit) : (hwS.rv & ~bit);
      setHw('rv', hwS.rv);
    }
  });
  menu.addEventListener('input', function(e){
    var k = e.target.dataset.hk, sh = k && menu.querySelector('[data-hv="' + k + '"]');
    if (sh) sh.textContent = hwFmt(k, +e.target.value);
  });
  document.getElementById('md').addEventListener('input', function(){ document.getElementById('mdv').textContent = this.value; });
  menu.addEventListener('click', function(e){
    var seg = e.target.parentNode && e.target.parentNode.dataset ? e.target.parentNode.dataset.h : null;
    if (seg && e.target.dataset.v !== undefined && !e.target.disabled){
      var v = +e.target.dataset.v;
      if (seg === 'mode' && v !== hwS.mode){
        // typowe kanały: czołg – skręt na CH1; Mecanum – obrót na CH4 (lewy drążek), bok na CH1
        if (v === 1 && hwS.c[0] === 1 && hwS.c[2] === 4){ hwS.c[0] = 4; hwS.c[2] = 1; setHw('c0', 4); setHw('c2', 1); }
        if (v === 0 && hwS.c[0] === 4 && hwS.c[2] === 1){ hwS.c[0] = 1; hwS.c[2] = 4; setHw('c0', 1); setHw('c2', 4); }
      }
      hwS[seg] = v;
      setHw(seg, v);
      renderHw();
    }
    if (e.target.classList.contains('hwreboot')){
      send('H reboot 1');
      stxt.textContent = 'uruchamianie ponowne…';
      menu.hidden = true;
    }
  });
  document.getElementById('hwreset').addEventListener('click', function(){ setHw('reset', 1); });

  // podgląd kanałów aparatury: "V aktywny połączona LQ ch1..ch16" (us)
  setInterval(function(){ if (!menu.hidden && tab === 'elrs') send('V'); }, 250);
  function showChannels(p){
    var active = p[1] === '1', conn = p[2] === '1', rcst = document.getElementById('rcst');
    rcst.className = 'msg ' + (conn ? 'ok' : active ? 'warn' : 'err');
    rcst.textContent = !active ? 'Odbiornik wyłączony – brak pinu albo pin zajęty.'
      : conn ? 'Aparatura połączona · LQ ' + p[3] + '%' : 'Brak sygnału z aparatury.';
    var used = {};
    if (hwS) hwS.c.forEach(function(c, i){ if (c && (i !== 2 || hwS.mode === 1)) used[c] = (used[c] ? used[c] + ', ' : '') + CH_FUNCS[i]; });
    for (var c = 1; c <= 16; c++){
      var row = document.getElementById('chv' + c);
      if (!row) return;
      var us = +p[3 + c] || 0;
      row.querySelector('i').style.width = us ? Math.max(0, Math.min(100, (us - 988) / 10.24)) + '%' : '0';
      row.querySelector('.us').textContent = us || '–';
      row.classList.toggle('used', !!used[c]);
      var tag = row.querySelector('.tag');
      if (used[c] && !tag){ tag = document.createElement('span'); tag.className = 'tag'; row.appendChild(tag); }
      if (tag){ tag.textContent = used[c] || ''; tag.hidden = !used[c]; }
    }
  }
  loadHw();

  // ---------- schemat połączeń ----------
  // Układ pinów płytki ESP32-CAM (AI-Thinker), widok od strony kamery
  var BOARD_L = [['5V', 'p5v'], ['GND', 'gnd'], ['IO12', 12], ['IO13', 13], ['IO15', 15], ['IO14', 14], ['IO2', 2], ['IO4', 4]];
  var BOARD_R = [['3V3', 'p33'], ['IO16', 'x16'], ['IO0', 'x0'], ['GND', 'gnd'], ['VCC', 'vcc'], ['U0R', 3], ['U0T', 1], ['GND/R', 'gndr']];
  var WCOL = ['#5aa8f0', '#f0a35a', '#5ad08f', '#c58af0'];   // koła / strony
  var C_LED = '#ffd84d', C_RC = '#ff7eb6', C_PWR = '#ff6b5a', C_GND = '#9aa7b0', C_OFF = '#55606a';

  // co jest podłączone do każdego pinu, według ustawień z menu
  function wiringMap(){
    var m = {}, mec = hwS.mode === 1, tb = hwS.drv === 1, n = mec ? 8 : 4;
    var IN = tb ? ['AIN1', 'AIN2', 'BIN1', 'BIN2'] : ['IN1', 'IN2', 'IN3', 'IN4'];
    var WH = mec ? ['LP', 'PP', 'LT', 'PT'] : ['lewa', 'prawa'];   // skróty – pełne nazwy w kartach
    function add(pin, text, color){
      if (m[pin]){ m[pin] = { t: '⚠ ' + m[pin].t + ' + ' + text, c: '#ff4d4d', bad: true }; return; }
      m[pin] = { t: text, c: color };
    }
    for (var i = 0; i < n; i++){
      var mod = mec ? (i < 4 ? '#1 ' : '#2 ') : '';
      add(hwS.p[i], mod + IN[i % 4] + ' · ' + WH[i >> 1], WCOL[i >> 1]);
    }
    // dioda / odbiornik na pinie silnika robot po prostu wyłącza – to nie błąd
    var motorPins = hwS.p.slice(0, n);
    if (hwS.lp >= 0 && motorPins.indexOf(hwS.lp) < 0) add(hwS.lp, hwS.lp === 4 ? 'dioda (na płytce)' : 'dioda', C_LED);
    if (hwS.rp >= 0 && motorPins.indexOf(hwS.rp) < 0 && hwS.rp !== hwS.lp) add(hwS.rp, 'ELRS TX', C_RC);
    m.p5v = { t: tb ? '← przetwornica 5 V' : '← +5V z L298N', c: C_PWR };
    m.gnd = { t: 'masa wspólna', c: C_GND };
    m.p33 = tb ? { t: '→ VCC, STBY, PWMA/B', c: C_PWR } : { t: 'nie podłączaj', c: C_OFF };
    m.x16 = { t: 'PSRAM – nie podłączaj', c: C_OFF };
    m.x0 = { t: 'kamera – nie podłączaj', c: C_OFF };
    m.vcc = { t: 'wyjście – nie zasilaj', c: C_OFF };
    m.gndr = { t: 'nie używaj jako masy', c: C_OFF };
    [1, 2, 3, 4, 12, 13, 14, 15].forEach(function(p){
      if (!m[p]) m[p] = { t: p === 1 || p === 3 ? 'programator (' + (p === 3 ? 'RX' : 'TX') + ')' : 'wolny', c: C_OFF };
    });
    return m;
  }

  function wiringSvg(m){
    var W = 440, top = 62, step = 30, bx = 160, bw = 120, H = top + 8 * step + 14;
    var o = '<svg viewBox="0 0 ' + W + ' ' + H + '" xmlns="http://www.w3.org/2000/svg" font-family="system-ui,sans-serif">';
    o += '<rect x="' + bx + '" y="4" width="' + bw + '" height="' + (H - 8) + '" rx="8" fill="#1c2329" stroke="#3a4650"/>';
    o += '<rect x="' + (bx + 35) + '" y="12" width="50" height="40" rx="4" fill="#0a0c0e" stroke="#55606a"/>';
    o += '<circle cx="' + (bx + 60) + '" cy="32" r="11" fill="#151a1e" stroke="#7a8590"/>';
    o += '<text x="' + (bx + 60) + '" y="' + (H - 16) + '" fill="#8a949c" font-size="10" text-anchor="middle">ESP32-CAM</text>';
    o += '<text x="' + (bx + 60) + '" y="' + (H - 5) + '" fill="#55606a" font-size="9" text-anchor="middle">widok od strony kamery</text>';
    function pin(list, left){
      list.forEach(function(p, i){
        var y = top + i * step, x = left ? bx : bx + bw, f = m[p[1]] || { t: '', c: C_OFF };
        o += '<circle cx="' + x + '" cy="' + y + '" r="6" fill="' + f.c + '" stroke="#0e1215" stroke-width="2"/>';
        o += '<text x="' + (left ? x + 11 : x - 11) + '" y="' + (y + 4) + '" fill="#e6ebef" font-size="11" font-weight="600" text-anchor="' + (left ? 'start' : 'end') + '">' + p[0] + '</text>';
        o += '<text x="' + (left ? x - 11 : x + 11) + '" y="' + (y + 4) + '" fill="' + f.c + '" font-size="10.5" text-anchor="' + (left ? 'end' : 'start') + '">' + esc(f.t) + '</text>';
      });
    }
    pin(BOARD_L, true);
    pin(BOARD_R, false);
    return o + '</svg>';
  }

  function wrow(color, a, b){ return '<div class="wrow"><span class="dot" style="background:' + color + '"></span><b>' + a + '</b><span>' + b + '</span></div>'; }
  function gp(p){ return p >= 0 ? (p === 1 ? 'U0T (IO1)' : p === 3 ? 'U0R (IO3)' : 'IO' + p) : '–'; }

  // Rysunek modułu sterownika z opisem każdego pinu. pins: [x, y, strona l/r/b, nazwa, podłączenie, kolor]
  function moduleSvg(w, h, body, pins){
    var o = '<svg viewBox="0 0 ' + w + ' ' + h + '" class="msvg" xmlns="http://www.w3.org/2000/svg" font-family="system-ui,sans-serif">' + body;
    pins.forEach(function(p){
      var x = p[0], y = p[1], sd = p[2];
      o += '<circle cx="' + x + '" cy="' + y + '" r="5" fill="' + p[5] + '" stroke="#0e1215" stroke-width="2"/>';
      if (sd === 'b'){
        o += '<text transform="translate(' + (x + 4) + ',' + (y - 9) + ') rotate(-90)" fill="#e6ebef" font-size="10" font-weight="600">' + p[3] + '</text>';
        o += '<text transform="translate(' + (x + 4) + ',' + (y + 9) + ') rotate(-90)" fill="' + p[5] + '" font-size="10" text-anchor="end">' + esc(p[4]) + '</text>';
      } else {
        var l = sd === 'l';
        o += '<text x="' + (l ? x + 9 : x - 9) + '" y="' + (y + 3.5) + '" fill="#e6ebef" font-size="10" font-weight="600" text-anchor="' + (l ? 'start' : 'end') + '">' + p[3] + '</text>';
        o += '<text x="' + (l ? x - 9 : x + 9) + '" y="' + (y + 3.5) + '" fill="' + p[5] + '" font-size="10" text-anchor="' + (l ? 'end' : 'start') + '">' + esc(p[4]) + '</text>';
      }
    });
    return o + '</svg>';
  }

  // L298N widziany z góry: radiator u góry, zaciski silników po bokach, zasilanie i piny IN na dole
  function l298nSvg(ins, cols, motors, first){
    var body = '<rect x="80" y="10" width="160" height="150" rx="6" fill="#3a1d20" stroke="#a8454a"/>' +
      '<rect x="125" y="16" width="70" height="44" rx="3" fill="#2a2f34" stroke="#55606a"/>' +
      '<text x="160" y="42" fill="#8a949c" font-size="9" text-anchor="middle">radiator</text>' +
      '<text x="160" y="84" fill="#c9a0a3" font-size="11" font-weight="600" text-anchor="middle">L298N</text>' +
      '<rect x="82" y="38" width="14" height="44" rx="2" fill="#2b5fa8"/><rect x="224" y="38" width="14" height="44" rx="2" fill="#2b5fa8"/>' +
      '<rect x="88" y="130" width="62" height="16" rx="2" fill="#2b5fa8"/>';
    var pins = [
      [89, 50, 'l', 'OUT1', motors[0], cols[0]], [89, 70, 'l', 'OUT2', motors[0], cols[0]],
      [231, 50, 'r', 'OUT3', motors[1], cols[1]], [231, 70, 'r', 'OUT4', motors[1], cols[1]],
      [96, 138, 'b', '+12V', 'aku +', C_PWR], [119, 138, 'b', 'GND', 'masa', C_GND],
      [142, 138, 'b', '+5V', first ? '→ 5V ESP' : 'wolny', first ? C_PWR : C_OFF],
      [166, 150, 'b', 'ENA', 'zworka', C_OFF], [180, 150, 'b', 'IN1', ins[0], cols[0]], [194, 150, 'b', 'IN2', ins[1], cols[0]],
      [208, 150, 'b', 'IN3', ins[2], cols[1]], [222, 150, 'b', 'IN4', ins[3], cols[1]], [236, 150, 'b', 'ENB', 'zworka', C_OFF]
    ];
    return moduleSvg(320, 210, body, pins);
  }

  // TB6612FNG (moduł 16 pinów jak SparkFun): lewa i prawa listwa
  function tb6612Svg(ins, cols, motors){
    var body = '<rect x="110" y="6" width="100" height="204" rx="6" fill="#2a1f3a" stroke="#7a5fb0"/>' +
      '<text x="160" y="22" fill="#b7a6d8" font-size="10" font-weight="600" text-anchor="middle">TB6612FNG</text>';
    var L = [['PWMA', '3V3', C_PWR], ['AIN2', ins[1], cols[0]], ['AIN1', ins[0], cols[0]], ['STBY', '3V3', C_PWR],
             ['BIN1', ins[2], cols[1]], ['BIN2', ins[3], cols[1]], ['PWMB', '3V3', C_PWR], ['GND', 'masa', C_GND]];
    var R = [['VM', 'aku +', C_PWR], ['VCC', '3V3', C_PWR], ['GND', 'masa', C_GND], ['AO1', motors[0], cols[0]],
             ['AO2', motors[0], cols[0]], ['BO2', motors[1], cols[1]], ['BO1', motors[1], cols[1]], ['GND', 'masa', C_GND]];
    var pins = [];
    L.forEach(function(p, i){ pins.push([116, 40 + i * 22, 'l', p[0], p[1], p[2]]); });
    R.forEach(function(p, i){ pins.push([204, 40 + i * 22, 'r', p[0], p[1], p[2]]); });
    return moduleSvg(320, 216, body, pins);
  }

  function wiringCards(m){
    var mods4 = hwS.mode !== 1 && hwS.mot === 4;
    var mec = hwS.mode === 1, tb = hwS.drv === 1, name = tb ? 'TB6612FNG' : 'L298N';
    var IN = tb ? ['AIN1', 'AIN2', 'BIN1', 'BIN2'] : ['IN1', 'IN2', 'IN3', 'IN4'];
    var OUT = tb ? ['AO1/AO2', 'BO1/BO2'] : ['OUT1/OUT2', 'OUT3/OUT4'];
    var mods = mec ? 2 : (hwS.mot === 4 ? 2 : 1), h = '';
    for (var k = 0; k < mods; k++){
      var base = mec ? k * 4 : 0, title = name + (mods > 1 ? (k ? ' #2 (tył)' : ' #1 (przód)') : '');
      var wheels = mec ? ['koło ' + (k ? 'lewe tylne (LT)' : 'lewe przednie (LP)'), 'koło ' + (k ? 'prawe tylne (PT)' : 'prawe przednie (PP)')]
        : (mods > 1 ? [k ? 'silnik lewy tylny' : 'silnik lewy przedni', k ? 'silnik prawy tylny' : 'silnik prawy przedni'] : ['silnik lewy', 'silnik prawy']);
      var ins = [0, 1, 2, 3].map(function(i){ return gp(hwS.p[base + i]); });
      var cols = mec ? [WCOL[base >> 1], WCOL[(base >> 1) + 1]] : [WCOL[0], WCOL[1]];
      var short = mods > 1 ? (k ? ['silnik LT', 'silnik PT'] : ['silnik LP', 'silnik PP']) : ['silnik L', 'silnik P'];
      h += '<div class="wcard"><h4>' + title + '</h4>' + (tb ? tb6612Svg(ins, cols, short) : l298nSvg(ins, cols, short, k === 0));
      for (var i = 0; i < 4; i++){
        var gi = base + i, col = WCOL[mec ? (base + i) >> 1 : i >> 1];
        h += wrow(col, IN[i] + ' ←', gp(hwS.p[gi]) + (!mec && k ? ' (razem z #1)' : ''));
      }
      h += wrow(WCOL[mec ? base >> 1 : 0], OUT[0] + ' →', wheels[0]);
      h += wrow(WCOL[mec ? (base >> 1) + 1 : 1], OUT[1] + ' →', wheels[1]);
      if (tb){
        h += wrow(C_PWR, 'VM ←', 'akumulator + (4,5–13,5 V)');
        h += wrow(C_PWR, 'VCC, STBY ←', '3V3 z ESP32-CAM');
        h += wrow(C_PWR, 'PWMA, PWMB ←', '3V3 (na stałe)');
      } else {
        h += wrow(C_PWR, '+12V ←', 'akumulator + (7–12 V)');
        h += wrow(C_PWR, 'ENA, ENB', 'zworki założone');
        h += wrow(C_PWR, '+5V →', k ? 'nie podłączaj (tylko z #1)' : '5V ESP32-CAM, zworka 5V-EN założona');
      }
      h += wrow(C_GND, 'GND', 'minus akumulatora i GND ESP32-CAM');
      h += '</div>';
    }
    if (tb) h += '<div class="wcard"><h4>Przetwornica 5 V (step-down)</h4>' +
      wrow(C_PWR, 'IN+ / IN− ←', 'akumulator') + wrow(C_PWR, 'OUT+ →', '5V ESP32-CAM') + wrow(C_GND, 'OUT− →', 'GND ESP32-CAM') + '</div>';
    var mp = hwS.p.slice(0, mec ? 8 : 4);
    if (hwS.rp >= 0 && (mp.indexOf(hwS.rp) >= 0 || hwS.rp === hwS.lp))
      h += '<div class="wcard"><h4>Odbiornik ELRS</h4><p class="wnote">Wyłączony – pin ' + gp(hwS.rp) + ' jest zajęty' + (mec ? ' (Mecanum używa wszystkich wolnych pinów).' : '.') + '</p></div>';
    else if (hwS.rp >= 0) h += '<div class="wcard"><h4>Odbiornik ELRS</h4>' +
      wrow(C_RC, 'TX →', gp(hwS.rp)) + wrow(C_OFF, 'RX', 'niepodłączony') + wrow(C_PWR, 'VCC ←', '5V') + wrow(C_GND, 'GND', 'masa wspólna') + '</div>';
    var notes = [];
    if (hwS.pending) notes.push('Schemat pokazuje ustawienia z menu – zaczną działać po ponownym uruchomieniu robota.');
    for (var key in m) if (m[key].bad) { notes.push('Na schemacie jest konflikt pinów – popraw go w zakładce Napęd.'); break; }
    if (hwS.lp >= 0 && mp.indexOf(hwS.lp) >= 0) notes.push('Dioda jest wyłączona – jej pin ' + gp(hwS.lp) + ' steruje silnikiem' + (hwS.lp === 4 ? ', więc dioda na płytce będzie świecić podczas jazdy.' : '.'));
    if (mp.indexOf(1) >= 0 || mp.indexOf(3) >= 0) notes.push('U0T/U0R (IO1/IO3) to piny programatora: przy wgrywaniu kablem odłącz od nich przewody.');
    if (tb) notes.push('Rysunek TB6612 pokazuje najczęstszy układ pinów (jak w module SparkFun). Na niektórych modułach kolejność jest inna – sprawdź napisy na płytce.');
    if (mec || mods4) notes.push('Skróty: LP – lewe przednie, PP – prawe przednie, LT – lewe tylne, PT – prawe tylne.');
    if (!mec && hwS.mot === 4) notes.push('4 silniki: wejścia obu modułów łączysz równolegle z tymi samymi pinami ESP32.');
    notes.push('Jeśli koło kręci się w złą stronę, zamień jego dwa przewody albo użyj „Odwróć kierunek”.');
    h += '<div class="wcard"><h4>Uwagi</h4>' + notes.map(function(t){ return '<p class="wnote">' + t + '</p>'; }).join('') + '</div>';
    return h;
  }

  function showWiring(){
    if (!hwS) return;
    var m = wiringMap();
    document.getElementById('wsvg').innerHTML = wiringSvg(m);
    document.getElementById('wcards').innerHTML = wiringCards(m);
    document.getElementById('wiring').hidden = false;
  }
  menu.addEventListener('click', function(e){ if (e.target.classList.contains('wshow')) showWiring(); });
  document.getElementById('wclose').addEventListener('click', function(){ document.getElementById('wiring').hidden = true; });

  // ---------- Wi-Fi (menu) ----------
  var wS = null, wMode = 0;
  function $(id){ return document.getElementById(id); }
  var chs = '';
  for (var wc = 1; wc <= 13; wc++) chs += '<option value="' + wc + '">' + wc + '</option>';
  $('wch').innerHTML = chs;
  function esc(t){ return String(t).replace(/[&<>"]/g, function(c){ return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]; }); }
  function loadWifi(){
    fetch('/wifi', { cache: 'no-store' }).then(function(r){ return r.json(); })
      .then(function(j){ wS = j; renderWifi(true); }).catch(function(){});
  }
  function renderWifi(fill){
    if (fill){
      wMode = wS.mode;
      $('was').value = wS.as; $('wss').value = wS.ss; $('whn').value = wS.hn;
      $('wkeep').checked = !!wS.keep; $('wch').value = String(wS.ch);
      $('wap').value = ''; $('wsp').value = '';
      var st = '';
      if (wS.apOn) st += '<div class="msg ok">Sieć robota „' + esc(wS.as) + '” · http://' + wS.apIp + ' · połączone urządzenia: ' + wS.apClients + '</div>';
      if (wS.mode === 1) st += wS.staOk
        ? '<div class="msg ok">Sieć domowa „' + esc(wS.ss) + '”: połączono · http://' + wS.staIp + ' · sygnał ' + wS.rssi + ' dBm</div>'
        : '<div class="msg warn">Brak połączenia z siecią domową „' + esc(wS.ss) + '”.</div>';
      $('wst').innerHTML = st;
    }
    var bs = $('wmode').querySelectorAll('button');
    for (var i = 0; i < bs.length; i++) bs[i].classList.toggle('on', +bs[i].dataset.v === wMode);
    $('w-sta').hidden = wMode !== 1;
    $('w-ap').hidden = wMode === 1 && !$('wkeep').checked;
    $('whn-note').textContent = 'W sieci domowej strona będzie też pod adresem http://' + ($('whn').value || 'robot') +
      '.local (nie każdy telefon to obsługuje – wtedy użyj adresu IP z routera).';
  }
  $('wmode').addEventListener('click', function(e){
    if (e.target.dataset.v === undefined) return;
    wMode = +e.target.dataset.v; renderWifi(false);
  });
  $('wkeep').addEventListener('change', function(){ renderWifi(false); });
  $('whn').addEventListener('input', function(){ renderWifi(false); });
  $('wscan').addEventListener('click', function(){
    send('W scan 1');
    $('wlist').innerHTML = '<p class="note">Szukam sieci…</p>';
    var tries = 0;
    (function poll(){
      setTimeout(function(){
        fetch('/scan', { cache: 'no-store' }).then(function(r){ return r.json(); }).then(function(j){
          if (j.busy && ++tries < 20) return poll();
          var list = (j.list || []).filter(function(n){ return n.s; });
          $('wlist').innerHTML = list.length ? list.map(function(n){
            return '<button class="net" data-s="' + esc(n.s) + '">' + esc(n.s) + '<span>' + n.r + ' dBm' + (n.o ? ' · otwarta' : '') + '</span></button>';
          }).join('') : '<p class="note">Nie znaleziono sieci.</p>';
        }).catch(function(){ $('wlist').innerHTML = ''; });
      }, 800);
    })();
  });
  $('wlist').addEventListener('click', function(e){
    var b = e.target.closest('.net');
    if (!b) return;
    $('wss').value = b.dataset.s; $('wlist').innerHTML = ''; $('wsp').focus();
  });
  $('wapply').addEventListener('click', function(){
    var err = [], as = $('was').value.trim(), ap = $('wap').value, ss = $('wss').value.trim(), sp = $('wsp').value, hn = $('whn').value.trim();
    if (!as) err.push('Podaj nazwę sieci robota.');
    if (ap && ap.length < 8) err.push('Hasło sieci robota musi mieć co najmniej 8 znaków.');
    if (wMode === 1 && !ss) err.push('Podaj nazwę sieci domowej.');
    if (!/^[a-zA-Z0-9-]{1,24}$/.test(hn)) err.push('Nazwa robota: tylko litery bez polskich znaków, cyfry i myślnik.');
    if (err.length){ $('wmsg').innerHTML = err.map(function(t){ return '<div class="msg err">' + t + '</div>'; }).join(''); return; }
    var e = encodeURIComponent;
    send('W mode ' + wMode); send('W keep ' + ($('wkeep').checked ? 1 : 0)); send('W ch ' + $('wch').value);
    send('W as ' + e(as)); if (ap) send('W ap ' + e(ap));
    send('W ss ' + e(ss));
    if (sp || (wS && ss !== wS.ss)) send('W sp ' + e(sp));     // nowa sieć bez hasła = sieć otwarta
    send('W hn ' + e(hn));
    send('W apply 1');
    var where = wMode === 1
      ? 'Połącz telefon z siecią „' + ss + '” i otwórz http://' + hn + '.local albo adres IP robota z routera.' +
        ($('wkeep').checked ? ' Sieć robota „' + as + '” też będzie działać (http://192.168.4.1).' : '')
      : 'Połącz telefon z siecią „' + as + '” i otwórz http://192.168.4.1.';
    $('wmsg').innerHTML = '<div class="msg ok">Robot uruchamia się ponownie. ' + esc(where) + '</div>';
    stxt.textContent = 'uruchamianie ponowne…';
  });

  // ---------- System (menu) ----------
  function kb(b){ return Math.round(b / 1024) + ' KB'; }
  function upTime(sec){
    var h = Math.floor(sec / 3600), m = Math.floor(sec / 60) % 60, s2 = sec % 60;
    return (h ? h + ' h ' : '') + m + ' min ' + s2 + ' s';
  }
  var sysTimer = null;
  function loadSys(){
    fetch('/sys', { cache: 'no-store' }).then(function(r){ return r.json(); }).then(function(j){
      var rows = [
        ['Wersja programu', j.ver], ['Kamera', j.cam || '–'], ['Czas pracy', upTime(j.up)], ['Ostatni restart', j.reset],
        ['Wolna pamięć RAM', kb(j.heap) + ' z ' + kb(j.heapSize)], ['Najmniej RAM od startu', kb(j.heapMin)],
        ['Wolna PSRAM', kb(j.psram) + ' z ' + kb(j.psramSize)], ['Rozmiar programu', kb(j.sketch)],
        ['Miejsce na program', j.ota ? kb(j.otaSize) + ', wolne ' + kb(j.otaSize - j.sketch) : 'brak aktualizacji przez Wi-Fi']
      ];
      $('sysinfo').innerHTML = rows.map(function(r){ return '<span>' + r[0] + '</span><b>' + esc(r[1]) + '</b>'; }).join('');
      if (!j.ota) $('otamsg').innerHTML = '<div class="msg warn">Ten układ pamięci nie obsługuje aktualizacji przez Wi-Fi. Wgraj raz kablem plik full.bin z wersji 1.3.0 lub nowszej.</div>';
    }).catch(function(){});
    clearTimeout(sysTimer);
    sysTimer = setTimeout(function(){ if (!menu.hidden && tab === 'sys') loadSys(); }, 2000);
  }
  $('otafile').addEventListener('change', function(){
    var f = this.files[0], bad = f && /full/i.test(f.name);
    $('otago').disabled = !f || bad;
    $('otamsg').innerHTML = bad ? '<div class="msg err">To plik do wgrywania kablem. Wybierz plik kończący się na -app.bin.</div>' : '';
  });
  $('otago').addEventListener('click', function(){
    var f = $('otafile').files[0];
    if (!f) return;
    $('otago').disabled = true;
    $('otamsg').innerHTML = '<div class="msg warn">Wgrywanie… Nie zamykaj strony. Robot w tym czasie stoi, a obraz jest wstrzymany.</div>';
    var x = new XMLHttpRequest();
    x.open('POST', '/update');
    x.upload.onprogress = function(e){ if (e.lengthComputable) $('otabar').firstChild.style.width = (e.loaded / e.total * 100) + '%'; };
    x.onload = function(){
      $('otamsg').innerHTML = x.status === 200
        ? '<div class="msg ok">Gotowe. Robot uruchamia się ponownie – strona połączy się sama za kilka sekund.</div>'
        : '<div class="msg err">Nie udało się: ' + esc(x.responseText) + '</div>';
      if (x.status !== 200) $('otago').disabled = false;
    };
    x.onerror = function(){ $('otamsg').innerHTML = '<div class="msg err">Przerwane połączenie. Spróbuj ponownie bliżej robota.</div>'; $('otago').disabled = false; };
    x.send(f);
  });

  // kopia ustawień: plik JSON z obrazem, napędem, aparaturą i Wi-Fi (bez haseł)
  var CAM_KEYS = ['fs', 'q', 'br', 'ct', 'sa', 'wb', 'ae', 'al', 'ex', 'fx', 'hm', 'vf', 'rot', 'xc', 'gc', 'bpc', 'wpc', 'lenc'];
  var HW_KEYS = ['mode', 'mot', 'md', 'lp', 'rp', 'pf', 'fo', 'rt', 'rv', 'tg', 'mp', 'ac', 'lt', 'dz', 'rd', 'rm', 'drv'];
  function getJson(u){ return fetch(u, { cache: 'no-store' }).then(function(r){ return r.json(); }); }
  $('bkdown').addEventListener('click', function(){
    Promise.all([getJson('/settings'), getJson('/hw'), getJson('/wifi'), getJson('/sys')]).then(function(r){
      var out = { app: 'esp32-cam-robot', ver: r[3].ver, cam: {}, hw: { p: r[1].p, inv: r[1].inv, c: r[1].c }, wifi: {} };
      CAM_KEYS.forEach(function(k){ out.cam[k] = r[0][k]; });
      HW_KEYS.forEach(function(k){ out.hw[k] = r[1][k]; });
      ['mode', 'keep', 'ch', 'as', 'ss', 'hn'].forEach(function(k){ out.wifi[k] = r[2][k]; });
      var a = document.createElement('a');
      a.href = URL.createObjectURL(new Blob([JSON.stringify(out, null, 2)], { type: 'application/json' }));
      a.download = 'robot-ustawienia.json';
      document.body.appendChild(a); a.click(); a.remove();
      $('bkmsg').innerHTML = '<div class="msg ok">Zapisano plik robot-ustawienia.json.</div>';
    }).catch(function(){ $('bkmsg').innerHTML = '<div class="msg err">Nie udało się odczytać ustawień z robota.</div>'; });
  });
  $('bkpick').addEventListener('click', function(){ $('bkfile').click(); });
  $('bkfile').addEventListener('change', function(){
    var f = this.files[0];
    if (!f) return;
    f.text().then(function(t){
      var d = JSON.parse(t);
      if (d.app !== 'esp32-cam-robot' || !d.cam || !d.hw) throw new Error();
      CAM_KEYS.forEach(function(k){ if (d.cam[k] !== undefined) send('C ' + k + ' ' + d.cam[k]); });
      HW_KEYS.forEach(function(k){ if (d.hw[k] !== undefined) send('H ' + k + ' ' + d.hw[k]); });
      (d.hw.p || []).forEach(function(v, i){ send('H p' + i + ' ' + v); });
      (d.hw.inv || []).forEach(function(v, i){ send('H inv' + i + ' ' + v); });
      (d.hw.c || []).forEach(function(v, i){ send('H c' + i + ' ' + v); });
      if (d.wifi) ['mode', 'keep', 'ch', 'as', 'ss', 'hn'].forEach(function(k){
        if (d.wifi[k] !== undefined) send('W ' + k + ' ' + encodeURIComponent(d.wifi[k]));
      });
      send('W apply 1');                  // zapisuje wszystko i uruchamia robota ponownie
      $('bkmsg').innerHTML = '<div class="msg ok">Wczytano. Robot uruchamia się ponownie. Hasła Wi-Fi zostały bez zmian.</div>';
    }).catch(function(){ $('bkmsg').innerHTML = '<div class="msg err">To nie jest plik ustawień robota.</div>'; });
    this.value = '';
  });
  $('sysreboot').addEventListener('click', function(){ send('H reboot 1'); stxt.textContent = 'uruchamianie ponowne…'; menu.hidden = true; });

  function pushNow(){ var c = command(); send(c); last = c; lastSent = Date.now(); }
  setInterval(function(){
    if (document.hidden) return;      // karta w tle -> brak sygnału -> failsafe zatrzyma robota
    var c = command(), now = Date.now();
    // wysyłaj przy zmianie (max 20/s) albo co 200 ms jako sygnał "żyję" dla failsafe
    if (c !== last || now - lastSent > 200) pushNow();
  }, 50);

  document.addEventListener('visibilitychange', function(){ if (document.hidden) send('M 0 0'); });
  document.addEventListener('contextmenu', function(e){ e.preventDefault(); });
})();
</script>
</body>
</html>
)rawliteral";
