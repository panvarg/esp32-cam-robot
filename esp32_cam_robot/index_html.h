#pragma once

// Strona sterowania: podgląd z kamery na całym ekranie + gałki i przyciski, których położenie,
// rozmiar i funkcje ustawia się w edytorze układu (jak w emulatorach gier).
// Przycisk ⚙ otwiera menu ustawień (zapisywane w pamięci robota).
// Trzymaj telefon poziomo. Na komputerze działa też WASD / strzałki, F = światło.
static const char INDEX_HTML[] = R"rawliteral(<!doctype html>
<html lang="pl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<meta name="theme-color" content="#000000">
<link rel="manifest" href="/manifest.json">
<link rel="icon" href="/icon.svg">
<title>ESP32 Robot</title>
<style>
  :root{--R:85px;--K:76px}   /* zakres ruchu i rozmiar gałki – JS dopasowuje je do ekranu */
  [hidden]{display:none!important}
  /* zapasowy obrót do poziomu, gdy telefon nie pozwala zablokować orientacji (iPhone, ramka) */
  body.rot #app{position:fixed;top:0;transform-origin:0 0;transform:rotate(90deg);overflow:hidden}
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
  #menu select,#epanel select{height:36px;border-radius:8px;background:#22282e;color:#fff;
    border:1px solid rgba(255,255,255,.3);padding:0 8px;font-size:14px}
  #menu input[type=range],#epanel input[type=range]{width:100%;accent-color:#3b8fe0}
  .row{display:flex;justify-content:space-between;align-items:center}
  .row input[type=checkbox]{width:22px;height:22px;accent-color:#3b8fe0}
  .ends{display:flex;justify-content:space-between;font-size:11px;color:#8a949c}
  .sep{border-top:1px solid rgba(255,255,255,.1)}
  #mreset,.mreset{height:38px;border-radius:8px;border:1px solid rgba(255,140,120,.5);background:transparent;color:#ff9c8a;font-size:14px}
  .note{font-size:11px;color:#8a949c;margin:0}
  .tabs{display:flex;flex-wrap:wrap;gap:4px;padding:8px 14px;border-bottom:1px solid rgba(255,255,255,.1)}
  .tabs button{flex:1 1 auto;padding:0 8px;height:30px;border-radius:8px;border:1px solid rgba(255,255,255,.2);
    background:transparent;color:#aab4bc;font-size:13px}
  .tabs button.on{background:rgba(255,255,255,.1);color:#fff}
  .pinrow{display:grid;grid-template-columns:minmax(0,1fr) minmax(96px,48%);gap:8px;align-items:center;font-size:13px;color:#cfd6dc}
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
  #menu input[type=text],#menu input[type=password],#epanel input[type=text]{width:100%;min-width:0;height:36px;border-radius:8px;background:#22282e;color:#fff;
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
  /* układ ekranu: elementy w % ekranu, rozmiar w % krótszego boku */
  #lay{position:fixed;inset:0;z-index:1}
  #lay.locked .li{opacity:.3}
  .li{position:absolute;transform:translate(-50%,-50%);touch-action:none}
  .sbase{position:absolute;left:50%;top:50%;transform:translate(-50%,-50%);border-radius:999px;
    border:3px solid rgba(255,255,255,.55);background:rgba(255,255,255,.07);pointer-events:none}
  .sbase::before,.sbase::after{position:absolute;font-size:12px;line-height:1;color:rgba(255,255,255,.6)}
  .sbase.v::before{content:"\25B2";top:7px;left:50%;transform:translateX(-50%)}
  .sbase.v::after{content:"\25BC";bottom:7px;left:50%;transform:translateX(-50%)}
  .sbase.h::before{content:"\25C0";left:9px;top:50%;transform:translateY(-50%)}
  .sbase.h::after{content:"\25B6";right:9px;top:50%;transform:translateY(-50%)}
  .sknob{position:absolute;left:50%;top:50%;border-radius:50%;background:radial-gradient(circle at 35% 35%,#fff,#9ad 70%);
    box-shadow:0 2px 10px rgba(0,0,0,.6);opacity:.9;pointer-events:none}
  .lb{display:flex;align-items:center;justify-content:center;border-radius:24%;border:2px solid rgba(255,255,255,.5);
    background:rgba(0,0,0,.45);color:#fff;font-weight:600;overflow:hidden;white-space:nowrap}
  .lb.round{border-radius:50%}
  .lb.on{background:rgba(80,170,255,.85);border-color:#8cc8ff}
  .lb.lgt.on{background:rgba(255,220,80,.85);color:#000;border-color:#fd5}
  .lspd{display:flex;align-items:center;gap:4%;background:rgba(0,0,0,.45);border-radius:12px;padding:0 4%}
  .lspd button{flex:1;height:76%;border-radius:8px;border:1px solid rgba(255,255,255,.4);background:rgba(255,255,255,.1);color:#fff;font-size:inherit;padding:0}
  .lspd span{flex:1.3;text-align:center;font-weight:600;font-variant-numeric:tabular-nums}
  .lpad{border:2px dashed rgba(255,255,255,.18);border-radius:16px}
  .padhud{position:absolute;left:50%;bottom:6px;transform:translateX(-50%);font-size:11px;color:rgba(255,255,255,.7);
    background:rgba(0,0,0,.35);padding:3px 8px;border-radius:8px;white-space:nowrap;pointer-events:none}
  /* edytor układu */
  body.editing .li{outline:2px dashed rgba(120,190,255,.75);outline-offset:2px;background:rgba(40,80,140,.18);border-radius:12px}
  body.editing .li.sel{outline:2px solid #5aa8f0;background:rgba(40,80,140,.4)}
  body.editing .li *{pointer-events:none}
  body.editing #top,body.editing #readout,body.editing #hint,body.editing #rc{display:none}
  #etb{position:fixed;top:6px;left:50%;transform:translateX(-50%);z-index:15;display:flex;flex-wrap:wrap;gap:6px;
    justify-content:center;align-items:center;width:max-content;max-width:96vw;background:rgba(16,19,23,.94);padding:6px;border-radius:12px;font-size:12px}
  #etb button{height:32px;padding:0 9px;border-radius:8px;border:1px solid rgba(255,255,255,.3);background:rgba(255,255,255,.08);color:#fff;font-size:13px}
  #etb .done{background:#3b8fe0;border-color:#5aa8f0;font-weight:600}
  #etb input{width:80px;accent-color:#3b8fe0}
  #epanel{position:fixed;left:50%;bottom:6px;transform:translateX(-50%);width:min(380px,96vw);max-height:52vh;overflow:auto;z-index:15;
    background:rgba(16,19,23,.96);border:1px solid rgba(255,255,255,.15);border-radius:12px;padding:10px 12px;
    display:flex;flex-direction:column;gap:10px;touch-action:pan-y;color:#fff}
  #hint{position:fixed;bottom:10px;right:10px;font-size:11px;color:#bbb}
  @media (pointer:coarse){#hint{display:none}}
  @media (orientation:portrait){
    #top{top:56px}
  }
  #top{z-index:4}
</style>
</head>
<body>
<div id="app">
<div id="nocam">brak obrazu z kamery</div>
<img id="cam" alt="">

<div id="status" class="hud"><span id="dot"></span><span id="stxt">łączenie…</span></div>
<div id="rc" class="hud"></div>
<div id="readout" class="hud">L 0 · P 0</div>

<div id="lay"></div>

<div id="top">
  <div class="btn" id="fsb" title="Pełny ekran">&#9974;</div>
  <div class="btn" id="gear" title="Ustawienia">&#9881;</div>
</div>

<div id="etb" hidden>
  <button data-add="stick">+ Gałka</button><button data-add="btn">+ Przycisk</button><button data-add="pad">+ Kamera</button>
  <button data-add="light">+ Światło</button><button data-add="speed">+ Moc</button>
  <label>Krycie <input type="range" id="eop" min="20" max="100" step="5"></label>
  <button id="edef">Domyślny</button><button class="done" id="edone">Gotowe</button>
</div>
<div id="epanel" hidden></div>

<div id="menu" hidden>
  <div class="mhead">Ustawienia <button id="mclose" aria-label="Zamknij">&#10005;</button></div>
  <div class="tabs"><button data-tab="obraz" class="on">Obraz</button><button data-tab="naped">Napęd</button><button data-tab="elrs">ELRS</button><button data-tab="serwa">Serwa</button><button data-tab="przyciski">Przyciski</button><button data-tab="wifi">Wi-Fi</button><button data-tab="sys">System</button></div>
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
    <div class="inrow"><button class="wbtn wshow" style="flex:1">Schemat połączeń</button><button class="wbtn eopen" style="flex:1">Układ ekranu</button></div>
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

  <div class="mbody" id="tab-serwa" hidden>
    <button class="wbtn wshow">Schemat połączeń</button>
    <div class="grp"><label>Moduł PCA9685 (16 wyjść przez I2C)</label>
      <div class="pinrow"><span>SDA</span><select data-pin="sda"></select></div>
      <div class="pinrow"><span>SCL</span><select data-pin="scl"></select></div>
      <div class="pinrow"><span>Adres I2C</span><select data-hk="pa">
        <option value="64">0x40</option><option value="65">0x41</option><option value="66">0x42</option><option value="67">0x43</option>
        <option value="68">0x44</option><option value="69">0x45</option><option value="70">0x46</option><option value="71">0x47</option>
      </select></div>
      <div class="pinrow"><span>Częstotliwość PWM</span><select data-hk="pq">
        <option value="50">50 Hz (serwa)</option><option value="60">60 Hz</option><option value="100">100 Hz</option>
        <option value="200">200 Hz</option><option value="300">300 Hz</option><option value="500">500 Hz</option><option value="1000">1000 Hz</option>
      </select></div>
      <div class="msg" id="pcast"></div>
      <p class="note">Moduł zajmuje 2 piny, a daje 16 kanałów: na serwa, silniki, diodę i wyjścia przycisków. Częstotliwość jest wspólna dla wszystkich kanałów – zwykłe serwa potrzebują 50 Hz.</p>
    </div>
    <div class="grp" id="servos"></div>
    <div class="grp" id="iomsg"></div>
    <button class="primary hwreboot">Zapisz i uruchom ponownie</button>
    <p class="note">Wyjścia serw i moduł PCA zaczną działać po ponownym uruchomieniu. Zakres, środek, prędkość i sterowanie – od razu.</p>
    <button class="mreset" id="ioreset">Przywróć domyślne serwa i przyciski</button>
  </div>

  <div class="mbody" id="tab-przyciski" hidden>
    <button class="wbtn eopen">Edytuj układ ekranu</button>
    <div class="grp"><label>Pełny ekran (ten telefon)</label>
      <div class="row"><label for="fsauto">Włączaj po pierwszym dotknięciu</label><input type="checkbox" id="fsauto"></div>
      <div class="row"><label for="fsland">Obracaj ekran do poziomu</label><input type="checkbox" id="fsland"></div>
      <p class="note" id="fsnote">Przycisk ⛶ obok ⚙ chowa pasek adresu i pasek systemu. Działa w Chrome i Edge na Androidzie oraz na komputerze. Jeśli telefon nie pozwala zablokować obrotu, strona obraca sam obraz o 90°. Na iPhonie: Udostępnij → „Do ekranu początkowego” – strona otworzy się bez paska adresu.</p>
    </div>
    <p class="note">Tu ustawiasz, co robi każdy z 8 przycisków. Na ekran dodajesz je w edytorze układu, tam też zmienisz ich położenie i rozmiar. Każdy można też wyzwalać przełącznikiem aparatury.</p>
    <div class="grp" id="acts"></div>
    <div class="grp" id="actmsg"></div>
    <button class="primary hwreboot">Zapisz i uruchom ponownie</button>
    <p class="note">Zmiana wyjścia przycisku działa po ponownym uruchomieniu, reszta od razu.</p>
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
      <p class="note">Plik zawiera ustawienia obrazu, napędu, aparatury, serw, przycisków, układ ekranu i Wi-Fi, bez haseł.</p>
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

</div><!--app-->
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
      if (p.length >= 10 && +p[9] !== actMaskNow){ actMaskNow = +p[9]; markButtons(); }
      showRc(+p[1]);
      var l = p[2] === '1';
      if (l !== light){ light = l; showLight(); }
      if (p[3] !== undefined) stxt.textContent = 'połączono · ' + p[3] + ' fps' + (p[4] ? ' · ' + p[4] + ' KB' : '');
    };
    ws.onclose = function(){ showRc(0); dot.className = ''; stxt.textContent = 'rozłączono'; setTimeout(connect, 1000); };
    ws.onerror = function(){ try{ ws.close(); }catch(e){} };
  }
  var rcEl = document.getElementById('rc');
  function showRc(st){
    rcEl.className = 'hud' + (st === 2 ? ' armed' : st === 1 ? ' on' : '');
    rcEl.textContent = st === 2 ? 'Aparatura steruje' : st === 1 ? 'Aparatura połączona' : '';
    lay.classList.toggle('locked', st === 2);
  }
  function send(msg){ if (ws && ws.readyState === 1) ws.send(msg); }
  connect();

  // ---------- moc ----------
  var speed = 70;
  function setSpeed(v){
    speed = Math.max(20, Math.min(100, v));
    document.querySelectorAll('#lay .spdv').forEach(function(e){ e.textContent = speed + '%'; });
    markButtons();
  }

  // ---------- światło ----------
  var light = false;
  function sendLight(){ send('L ' + (light ? 1 : 0)); }
  function toggleLight(){ light = !light; showLight(); sendLight(); }
  function showLight(){
    document.querySelectorAll('#lay [data-light]').forEach(function(e){ e.classList.toggle('on', light); });
    markButtons();
  }

  // ---------- ustawienia obrazu ----------
  var menu = document.getElementById('menu');
  var camS = {};
  var PRESETS = [ { fs: 0, q: 14 }, { fs: 2, q: 12 }, { fs: 3, q: 10 } ];   // Płynnie / Balans / Jakość
  tap('gear', function(){ menu.hidden = !menu.hidden; });

  // ---------- współrzędne strony (przy zapasowym obrocie interfejsu o 90°) ----------
  var rotated = false;
  function P(e){ return rotated ? { x: e.clientY, y: window.innerWidth - e.clientX } : { x: e.clientX, y: e.clientY }; }
  function C(r){ return P({ clientX: r.left + r.width / 2, clientY: r.top + r.height / 2 }); }   // środek elementu
  function appW(){ return rotated ? window.innerHeight : window.innerWidth; }
  function appH(){ return rotated ? window.innerWidth : window.innerHeight; }

  // ---------- pełny ekran ----------
  // Przeglądarka pozwala na pełny ekran i blokadę obrotu tylko po dotknięciu – stąd przycisk ⛶
  // albo opcja „po pierwszym dotknięciu”. Ustawienia są osobne dla każdego telefonu.
  var FS_KEY = 'robotFullscreen';
  function fsPrefs(){
    try { var p = JSON.parse(localStorage.getItem(FS_KEY)); if (p) return p; } catch (e) {}
    return { auto: 0, land: 1 };
  }
  function fsSave(p){ try { localStorage.setItem(FS_KEY, JSON.stringify(p)); } catch (e) {} }
  function fsActive(){ return !!(document.fullscreenElement || document.webkitFullscreenElement); }
  function fsSupported(){ var d = document.documentElement; return !!(d.requestFullscreen || d.webkitRequestFullscreen); }
  var standalone = (window.matchMedia && (matchMedia('(display-mode: fullscreen)').matches || matchMedia('(display-mode: standalone)').matches)) || navigator.standalone;
  function enterFs(){
    var d = document.documentElement, p = fsPrefs(), r;
    try { r = d.requestFullscreen ? d.requestFullscreen({ navigationUI: 'hide' }) : d.webkitRequestFullscreen(); } catch (e) { r = Promise.reject(e); }
    Promise.resolve(r).then(function(){
      if (!p.land) return;
      if (!screen.orientation || !screen.orientation.lock) throw new Error('brak blokady obrotu');
      return screen.orientation.lock('landscape').then(function(){ rotFallback = false; updRot(); });
    }).catch(function(){ rotFallback = true; updRot(); });   // np. iPhone albo strona w ramce – obracamy sam interfejs
  }
  // Zapasowy obrót: cały interfejs obrócony o 90°, gdy telefon jest pionowo, a blokada orientacji nie zadziałała
  var rotFallback = false;
  function updRot(){
    var on = !!fsPrefs().land && window.innerHeight > window.innerWidth &&
             ((fsActive() && rotFallback) || standalone);
    var a = document.getElementById('app');
    a.style.width = on ? window.innerHeight + 'px' : '';
    a.style.height = on ? window.innerWidth + 'px' : '';
    a.style.left = on ? window.innerWidth + 'px' : '';
    if (on !== rotated){
      rotated = on;
      document.body.classList.toggle('rot', on);
      window.dispatchEvent(new Event('resize'));   // układ, kamera i gałki przeliczają się na nowe wymiary
    }
  }
  window.addEventListener('resize', updRot);
  function exitFs(){
    if (screen.orientation && screen.orientation.unlock) try { screen.orientation.unlock(); } catch (e) {}
    if (document.exitFullscreen) document.exitFullscreen().catch(function(){});
    else if (document.webkitExitFullscreen) document.webkitExitFullscreen();
  }
  function fsUpdate(){
    var b = document.getElementById('fsb');
    b.style.display = standalone || !fsSupported() ? 'none' : '';
    b.classList.toggle('on', fsActive());
    b.title = fsActive() ? 'Wyjdź z pełnego ekranu' : 'Pełny ekran';
    updRot();
  }
  tap('fsb', function(){ fsActive() ? exitFs() : enterFs(); });
  document.addEventListener('fullscreenchange', fsUpdate);
  document.addEventListener('webkitfullscreenchange', fsUpdate);
  // pierwsze dotknięcie gdziekolwiek (np. gałki) włącza pełny ekran, jeśli tak ustawiono
  document.addEventListener('pointerdown', function(){
    if (fsPrefs().auto && !fsActive() && !standalone && fsSupported() && !editing) enterFs();
  }, true);
  (function(){
    var p = fsPrefs(), a = document.getElementById('fsauto'), l = document.getElementById('fsland');
    a.checked = !!p.auto; l.checked = !!p.land;
    a.addEventListener('change', function(){ var q = fsPrefs(); q.auto = a.checked ? 1 : 0; fsSave(q); });
    l.addEventListener('change', function(){ var q = fsPrefs(); q.land = l.checked ? 1 : 0; fsSave(q); updRot(); });
    if (standalone) document.getElementById('fsnote').textContent = 'Strona działa jako aplikacja z ekranu początkowego – pasek adresu jest już schowany.';
    else if (!fsSupported()) document.getElementById('fsnote').textContent = 'Ta przeglądarka nie pozwala stronom na pełny ekran. Na iPhonie: Udostępnij → „Do ekranu początkowego” – strona otworzy się bez paska adresu.';
    fsUpdate();
  })();
  var tab = 'obraz';
  var tabBtns = menu.querySelectorAll('.tabs button');
  for (var ti = 0; ti < tabBtns.length; ti++) tabBtns[ti].addEventListener('click', function(){
    tab = this.dataset.tab;
    for (var j = 0; j < tabBtns.length; j++) tabBtns[j].classList.toggle('on', tabBtns[j] === this);
    ['obraz', 'naped', 'elrs', 'serwa', 'przyciski', 'wifi', 'sys'].forEach(function(t){ document.getElementById('tab-' + t).hidden = t !== tab; });
    if (tab === 'sys') loadSys();
    if (tab === 'naped' || tab === 'elrs' || tab === 'serwa' || tab === 'przyciski') loadHw();
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
      var w = appW(), h = appH();
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

  // ---------- układ ekranu: gałki, przyciski, pole kamery, światło, moc ----------
  // x / y – środek elementu w % szerokości / wysokości ekranu, s – rozmiar w % krótszego boku.
  // Osobne układy dla poziomego i pionowego trzymania telefonu (jak w emulatorach).
  var lay = document.getElementById('lay'), layout = null, layCustom = false, editing = false, selIdx = null;
  var FX = [['none', 'brak'], ['turn', 'skręt / obrót'], ['strafe', 'jazda bokiem (Mecanum)'], ['pan', 'kamera – pan']];
  var FY = [['none', 'brak'], ['gas', 'gaz (przód / tył)'], ['tilt', 'kamera – tilt']];
  function portrait(){ return appH() > appW(); }
  function items(){ return layout[portrait() ? 'port' : 'land']; }
  function unit(){ return Math.min(window.innerWidth, window.innerHeight) / 100; }

  function defaultLayout(){
    var mec = hwS && hwS.bootMode === 1;
    function base(port){
      var y = port ? 76 : 60, s = port ? 40 : 44, L = [
        { t: 'stick', x: port ? 25 : 16, y: y, s: s, fx: mec ? 'strafe' : 'none', fy: 'gas', fl: 1, sp: 1 },
        { t: 'stick', x: port ? 75 : 84, y: y, s: s, fx: 'turn', fy: 'none', fl: 1, sp: 1 },
        { t: 'light', x: port ? 22 : 37, y: port ? 8 : 9, s: 12 },
        { t: 'speed', x: port ? 76 : 64, y: port ? 8 : 9, s: 12 } ];
      if (ioS && ioS.s.some(function(sv){ return (sv.ct === 1 || sv.ct === 2) && sv.o >= 0; }))
        L.push({ t: 'pad', x: 50, y: port ? 40 : 55, s: port ? 40 : 38 });
      var n = 0;                       // przyciski oznaczone w starszej wersji jako „Pokaż na ekranie”
      if (ioS) ioS.a.forEach(function(a, i){
        if (a.t && a.sh){ L.push({ t: 'btn', a: i, x: (port ? 90 : 95) - (n % 4) * (port ? 14 : 7), y: (port ? 17 : 11) + Math.floor(n / 4) * (port ? 8 : 15), s: 12 }); n++; }
      });
      return L;
    }
    return { v: 1, op: 85, land: base(false), port: base(true) };
  }
  function ensureLayout(){
    if (!layCustom && !editing && hwS) layout = defaultLayout();   // domyślny nadąża za trybem napędu i serwami
    if (layout) renderLayout();
  }
  function loadLayout(){
    fetch('/layout', { cache: 'no-store' }).then(function(r){ return r.json(); }).then(function(j){
      if (j && j.land && j.port){ layout = j; layCustom = true; }
      ensureLayout();
    }).catch(function(){ ensureLayout(); });
  }
  function saveLayout(){
    layCustom = true;
    fetch('/layout', { method: 'POST', body: JSON.stringify(layout) }).catch(function(){});
  }

  function stickGeom(it){
    var D = it.s * unit(), round = it.fx !== 'none' && it.fy !== 'none';
    var T = round ? D : D * 0.42, K = round ? D * 0.4 : T * 0.82;
    return { D: D, T: T, K: K, R: Math.max(10, (D - K) / 2 - 3), round: round };
  }
  function iconFor(a){ return a.l || (a.t === 5 ? a.a + '%' : ACT_ICON[a.t] || '?'); }

  function renderLayout(){
    if (!layout) return;
    lay.innerHTML = '';
    lay.style.opacity = editing ? 1 : layout.op / 100;
    var u = unit();
    items().forEach(function(it, idx){
      var el = document.createElement('div'), D = it.s * u;
      el.className = 'li' + (editing && idx === selIdx ? ' sel' : '');
      el.style.left = it.x + '%';
      el.style.top = it.y + '%';
      el.dataset.idx = idx;
      it.vx = it.vx || 0; it.vy = it.vy || 0;
      if (it.t === 'stick'){
        var g = stickGeom(it), hit = D * 1.25;
        el.style.width = el.style.height = hit + 'px';
        var b = document.createElement('div');
        b.className = 'sbase' + (g.round ? '' : it.fx === 'none' ? ' v' : ' h');
        b.style.width = (g.round || it.fy === 'none' ? g.D : g.T) + 'px';
        b.style.height = (g.round || it.fx === 'none' ? g.D : g.T) + 'px';
        var k = document.createElement('div');
        k.className = 'sknob';
        k.style.width = k.style.height = g.K + 'px';
        k.style.margin = (-g.K / 2) + 'px 0 0 ' + (-g.K / 2) + 'px';
        b.appendChild(k);
        el.appendChild(b);
        if (!editing) bindStick(el, it, b, k, g);
      } else if (it.t === 'btn' || it.t === 'light'){
        var a = it.t === 'btn' && ioS ? ioS.a[it.a] : null;
        el.className += ' lb' + (it.t === 'light' ? ' lgt round' : '');
        el.style.width = el.style.height = D + 'px';
        el.style.fontSize = (D * 0.38) + 'px';
        el.textContent = it.t === 'light' ? '💡' : a ? iconFor(a) : '?';
        if (it.t === 'light'){ el.dataset.light = 1; if (hwS && !hwS.lightOk && !editing) el.style.display = 'none'; }
        else el.dataset.b = it.a;
        if (!editing) bindButton(el, it);
      } else if (it.t === 'speed'){
        el.className += ' lspd';
        el.style.width = (D * 3.2) + 'px';
        el.style.height = D + 'px';
        el.style.fontSize = (D * 0.34) + 'px';
        el.innerHTML = '<button data-d="-10">&minus;</button><span class="spdv">' + speed + '%</span><button data-d="10">+</button>';
        if (!editing) el.addEventListener('pointerdown', function(e){
          var d = e.target.dataset.d;
          e.preventDefault(); e.stopPropagation();
          if (d) setSpeed(speed + +d);
        });
      } else if (it.t === 'pad'){
        el.className += ' lpad';
        el.style.width = el.style.height = D + 'px';
        el.innerHTML = '<div class="padhud">przeciągnij – kamera · dwuklik – środek</div>';
        if (!editing) bindPad(el);
      }
      lay.appendChild(el);
    });
    if (editing) bindEditor();
    showLight();
  }
  window.addEventListener('resize', function(){ if (layout) renderLayout(); });

  // gałka: osie mogą sterować jazdą albo serwami; pływająca pojawia się pod palcem
  function bindStick(el, it, base, knob, g){
    var id = null, cx = 0, cy = 0, sent = 0;
    function move(e){
      var q = P(e), dx = it.fx === 'none' ? 0 : q.x - cx, dy = it.fy === 'none' ? 0 : q.y - cy;
      var d = Math.sqrt(dx * dx + dy * dy);
      if (d > g.R){ dx = dx / d * g.R; dy = dy / d * g.R; d = g.R; }
      knob.style.transform = 'translate(' + dx + 'px,' + dy + 'px)';
      var n = d / g.R, k = n < DEADZONE ? 0 : (n - DEADZONE) / (1 - DEADZONE) / n;
      it.vx = dx / g.R * k;
      it.vy = -dy / g.R * k;
      if (Date.now() - sent > 40){ stickServos(it); sent = Date.now(); }
    }
    el.addEventListener('pointerdown', function(e){
      if (id !== null) return;
      e.preventDefault();
      id = e.pointerId;
      try { el.setPointerCapture(id); } catch (err) {}
      var c = C(el.getBoundingClientRect()), q = P(e);
      if (it.fl){                      // pływająca: środek pod palcem, ale tor zostaje w polu dotyku
        var mx = el.offsetWidth / 2 - g.R - g.K / 2, my = el.offsetHeight / 2 - g.R - g.K / 2;
        var ox = Math.max(-mx, Math.min(mx, q.x - c.x)), oy = Math.max(-my, Math.min(my, q.y - c.y));
        base.style.left = 'calc(50% + ' + ox + 'px)';
        base.style.top = 'calc(50% + ' + oy + 'px)';
        cx = c.x + ox; cy = c.y + oy;
      } else { cx = c.x; cy = c.y; }
      move(e);
    });
    el.addEventListener('pointermove', function(e){ if (e.pointerId === id) move(e); });
    function end(e){
      if (e.pointerId !== id) return;
      id = null;
      if (it.sp){
        it.vx = 0; it.vy = 0;
        knob.style.transform = '';
        base.style.left = base.style.top = '';
        stickServos(it);
      } else stickServos(it);
      pushNow();
    }
    el.addEventListener('pointerup', end);
    el.addEventListener('pointercancel', end);
  }
  // osie gałki przypisane do serw: wychylenie = kąt (od min przez środek do max)
  function stickServos(it){
    if (!ioS) return;
    function apply(f, v){
      var list = f === 'pan' ? lookServos(1) : f === 'tilt' ? lookServos(2)
        : /^sv\d$/.test(f) ? [{ s: ioS.s[+f[2]], i: +f[2] }] : [];
      list.forEach(function(x){
        var sv = x.s, ang = v >= 0 ? sv.c + v * (sv.mx - sv.c) : sv.c + v * (sv.c - sv.mn);
        lookPos[x.i] = ang;
        send('S ' + x.i + ' p ' + Math.round(ang));
      });
    }
    apply(it.fx, it.vx || 0);
    apply(it.fy, it.vy || 0);
  }
  function bindButton(el, it){
    var held = null;
    el.addEventListener('pointerdown', function(e){
      e.preventDefault(); e.stopPropagation();
      if (it.t === 'light'){ toggleLight(); return; }
      if (!ioS) return;
      held = e.pointerId;
      try { el.setPointerCapture(held); } catch (err) {}
      pressAct(it.a, true);
    });
    function up(e){ if (held !== null && e.pointerId === held){ held = null; pressAct(it.a, false); } }
    el.addEventListener('pointerup', up);
    el.addEventListener('pointercancel', up);
  }

  // ---------- edytor układu ----------
  var etb = document.getElementById('etb'), epanel = document.getElementById('epanel');
  function openEditor(){
    if (!layout) return;
    menu.hidden = true;
    editing = true; selIdx = null;
    layCustom = true;                 // od teraz układ jest własny – nie zastępuje go domyślny
    document.body.classList.add('editing');
    etb.hidden = false;
    document.getElementById('eop').value = layout.op;
    renderLayout();
    renderPanel();
  }
  function closeEditor(){
    editing = false; selIdx = null;
    document.body.classList.remove('editing');
    etb.hidden = true; epanel.hidden = true;
    saveLayout();
    renderLayout();
  }
  document.querySelectorAll('.eopen').forEach(function(b){ b.addEventListener('click', openEditor); });
  document.getElementById('edone').addEventListener('click', closeEditor);
  document.getElementById('edef').addEventListener('click', function(){
    layout[portrait() ? 'port' : 'land'] = defaultLayout()[portrait() ? 'port' : 'land'];
    selIdx = null; renderLayout(); renderPanel();
  });
  document.getElementById('eop').addEventListener('input', function(){ layout.op = +this.value; });
  etb.addEventListener('click', function(e){
    var t = e.target.dataset.add;
    if (!t) return;
    var it = { t: t, x: 50, y: 50, s: t === 'stick' ? 40 : t === 'pad' ? 36 : 12 };
    if (t === 'stick'){ it.fx = 'turn'; it.fy = 'gas'; it.fl = 1; it.sp = 1; }
    if (t === 'btn'){
      var used = items().filter(function(x){ return x.t === 'btn'; }).map(function(x){ return x.a; });
      var free = [0, 1, 2, 3, 4, 5, 6, 7].filter(function(i){ return used.indexOf(i) < 0; });
      if (!free.length){ alertPanel('Na ekranie może być najwyżej 8 przycisków.'); return; }
      it.a = free[0];
    }
    items().push(it);
    selIdx = items().length - 1;
    renderLayout(); renderPanel();
  });
  function alertPanel(t){ epanel.hidden = false; epanel.innerHTML = '<div class="msg warn">' + t + '</div>'; }

  // przeciąganie elementów, a dwoma palcami – zmiana rozmiaru zaznaczonego
  var eptr = {}, edrag = null, epinch = null;
  function bindEditor(){
    lay.querySelectorAll('.li').forEach(function(el){
      el.addEventListener('pointerdown', function(e){
        e.preventDefault(); e.stopPropagation();
        var idx = +el.dataset.idx, it = items()[idx];
        eptr[e.pointerId] = P(e);
        try { el.setPointerCapture(e.pointerId); } catch (err) {}
        if (Object.keys(eptr).length === 2 && selIdx !== null){
          var p = Object.values(eptr);
          epinch = { d: Math.hypot(p[0].x - p[1].x, p[0].y - p[1].y), s: items()[selIdx].s };
          edrag = null;
          return;
        }
        var q0 = P(e);
        edrag = { id: e.pointerId, idx: idx, sx: q0.x, sy: q0.y, x: it.x, y: it.y, moved: false };
        if (selIdx !== idx){ selIdx = idx; lay.querySelectorAll('.li').forEach(function(x){ x.classList.toggle('sel', x === el); }); renderPanel(); }
      });
      el.addEventListener('pointermove', function(e){
        if (!eptr[e.pointerId]) return;
        eptr[e.pointerId] = P(e);
        if (epinch && Object.keys(eptr).length >= 2){
          var p = Object.values(eptr), d = Math.hypot(p[0].x - p[1].x, p[0].y - p[1].y);
          items()[selIdx].s = Math.round(Math.max(6, Math.min(80, epinch.s * d / epinch.d)));
          renderLayout(); syncSize();
          return;
        }
        if (!edrag || e.pointerId !== edrag.id) return;
        var q1 = P(e), dx = q1.x - edrag.sx, dy = q1.y - edrag.sy;
        if (Math.abs(dx) + Math.abs(dy) > 4) edrag.moved = true;
        var it = items()[edrag.idx];
        it.x = Math.round(Math.max(0, Math.min(100, edrag.x + dx / appW() * 100)) * 2) / 2;
        it.y = Math.round(Math.max(0, Math.min(100, edrag.y + dy / appH() * 100)) * 2) / 2;
        el.style.left = it.x + '%';
        el.style.top = it.y + '%';
      });
      function up(e){
        delete eptr[e.pointerId];
        if (Object.keys(eptr).length < 2) epinch = null;
        if (edrag && e.pointerId === edrag.id) edrag = null;
      }
      el.addEventListener('pointerup', up);
      el.addEventListener('pointercancel', up);
    });
  }
  // dotknięcie pustego miejsca odznacza element
  lay.addEventListener('pointerdown', function(e){ if (editing && e.target === lay){ selIdx = null; renderLayout(); renderPanel(); } });

  function svOpts(list, v){
    var o = list.slice();
    if (ioS) ioS.s.forEach(function(sv, i){ o.push(['sv' + i, 'serwo „' + sv.n + '”']); });
    return o.map(function(x){ return '<option value="' + x[0] + '"' + (x[0] === v ? ' selected' : '') + '>' + esc(x[1]) + '</option>'; }).join('');
  }
  function syncSize(){
    var r = epanel.querySelector('[data-lk="s"]');
    if (r && selIdx !== null){ r.value = items()[selIdx].s; r.parentNode.querySelector('[data-u]').textContent = items()[selIdx].s + '%'; }
  }
  var TYPE_NAMES = { stick: 'Gałka', btn: 'Przycisk', pad: 'Pole kamery', light: 'Światło', speed: 'Moc' };
  function renderPanel(){
    if (!editing){ epanel.hidden = true; return; }
    epanel.hidden = false;
    if (selIdx === null || !items()[selIdx]){
      epanel.innerHTML = '<p class="note">Przeciągnij element, żeby go przesunąć. Dotknij go, żeby zmienić rozmiar i funkcję – rozmiar zmienisz też dwoma palcami. ' +
        'Układ dla telefonu trzymanego ' + (portrait() ? 'pionowo' : 'poziomo') + ' – obróć telefon, żeby ustawić drugi.</p>';
      return;
    }
    var it = items()[selIdx], h = '<div class="pinrow"><b>' + TYPE_NAMES[it.t] + '</b><button class="small" id="edel">Usuń</button></div>';
    h += rng('Rozmiar', 'data-lk', 's', it.s, 6, 80, 1, '%');
    if (it.t === 'stick'){
      h += '<div class="pinrow"><span>Lewo / prawo</span><select data-lk="fx">' + svOpts(FX, it.fx) + '</select></div>';
      h += '<div class="pinrow"><span>Góra / dół</span><select data-lk="fy">' + svOpts(FY, it.fy) + '</select></div>';
      h += '<div class="row"><label>Pływająca – pojawia się pod palcem</label><input type="checkbox" data-lk="fl"' + (it.fl ? ' checked' : '') + '></div>';
      h += '<div class="row"><label>Wraca na środek po puszczeniu</label><input type="checkbox" data-lk="sp"' + (it.sp ? ' checked' : '') + '></div>';
      h += '<p class="note">Jedna oś = tor, dwie osie = okrągła gałka. Bez powrotu na środek gaz działa jak tempomat, a kamera zostaje tam, gdzie ją ustawisz.</p>';
    } else if (it.t === 'btn' && ioS){
      h += actCard(ioS.a[it.a], it.a);
      h += '<p class="note">To przycisk nr ' + (it.a + 1) + ' z zakładki Przyciski – tę samą funkcję można wyzwalać z aparatury.</p>';
    } else if (it.t === 'pad'){
      h += '<p class="note">Przeciąganie palcem obraca kamerę (serwa ustawione w zakładce Serwa jako pan i tilt), dwuklik ją centruje.</p>';
    }
    epanel.innerHTML = h;
    epanel.querySelectorAll('[data-ak="o"]').forEach(function(sel){ pinOptions(sel, ioS.a[it.a].o, true, true); });
  }
  epanel.addEventListener('input', function(e){
    var k = e.target.dataset.lk;
    if (k !== 's' || selIdx === null) return;
    items()[selIdx].s = +e.target.value;
    e.target.parentNode.querySelector('[data-u]').textContent = e.target.value + '%';
    renderLayout();
  });
  epanel.addEventListener('change', function(e){
    var t = e.target, k = t.dataset.lk;
    if (!k || selIdx === null) return;
    var it = items()[selIdx];
    it[k] = t.type === 'checkbox' ? (t.checked ? 1 : 0) : k === 's' ? +t.value : t.value;
    renderLayout(); renderPanel();
  });
  epanel.addEventListener('click', function(e){
    if (e.target.id !== 'edel' || selIdx === null) return;
    items().splice(selIdx, 1);
    selIdx = null;
    renderLayout(); renderPanel();
  });

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
    var x = 0, y = 0, sd = 0;
    if (layout) items().forEach(function(it){
      if (it.t !== 'stick') return;
      if (it.fx === 'turn') x += it.vx || 0; else if (it.fx === 'strafe') sd += it.vx || 0;
      if (it.fy === 'gas') y += it.vy || 0;
    });
    if (!x && !y && !sd){ var k = keyAxis(); x = k.x; y = k.y; sd = k.s; }
    x = Math.max(-1, Math.min(1, x)); y = Math.max(-1, Math.min(1, y)); sd = mecanum ? Math.max(-1, Math.min(1, sd)) : 0;
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

  var ioS = null;
  function loadHw(){
    Promise.all([fetch('/hw', { cache: 'no-store' }).then(function(r){ return r.json(); }),
                 fetch('/io', { cache: 'no-store' }).then(function(r){ return r.json(); }).catch(function(){ return null; })])
      .then(function(r){ hwS = r[0]; if (r[1]) ioS = r[1]; applyMode(); renderHw(); renderIo(); }).catch(function(){});
  }
  // tryb gałek i przycisk światła zależą od konfiguracji aktywnej od startu robota
  function applyMode(){
    mecanum = hwS.bootMode === 1;
    if (hwS.dz !== undefined) DEADZONE = hwS.dz / 100;
    ensureLayout();                   // układ domyślny zależy od trybu napędu i serw
  }
  var hwTimer = null;
  function setHw(k, v){
    send('H ' + k + ' ' + v);
    clearTimeout(hwTimer);
    hwTimer = setTimeout(loadHw, 400);   // odśwież stan (np. czy potrzebny restart)
  }
  function pcaCfg(){ return hwS && hwS.sda >= 0 && hwS.scl >= 0 && hwS.sda !== hwS.scl; }
  // pca: czy pokazać kanały PCA9685 (wyjścia PWM); bez PCA – tylko piny ESP32
  function pinOptions(sel, val, allowNone, pca){
    var h = allowNone ? '<option value="-1">brak</option>' : '';
    FREE_PINS.forEach(function(p){ h += '<option value="' + p + '">' + gp(p) + (p === 4 ? ' – dioda' : '') + '</option>'; });
    if (pca && (pcaCfg() || val >= 100)) for (var c = 0; c < 16; c++) h += '<option value="' + (100 + c) + '">PCA kanał ' + c + '</option>';
    sel.innerHTML = h;
    sel.value = String(val);
  }

  // Kto zajmuje który pin / kanał – w kolejności ważności, jak w firmware (pierwszy wygrywa)
  var SV_COL = '#b7e36b', OUT_COL = '#e3a36b', I2C_COL = '#4fd1c5';
  function usage(){
    var list = [], taken = {}, gpioServos = 0, mec = hwS.mode === 1, n = mec ? 8 : 4;
    var tb = hwS.drv === 1, IN = tb ? ['AIN1', 'AIN2', 'BIN1', 'BIN2'] : ['IN1', 'IN2', 'IN3', 'IN4'];
    var WH = mec ? ['LP', 'PP', 'LT', 'PT'] : ['lewa', 'prawa'];
    function add(p, name, short, kind, color, ix){
      var it = { p: p, name: name, short: short, kind: kind, c: color, ok: true, why: '', ix: ix };
      if (p < 0) return;
      if (p >= 100 && !pcaCfg()){ it.ok = false; it.why = 'nopca'; }
      else if (taken[p]){ it.ok = false; it.why = 'conflict'; it.owner = taken[p].name; }
      else if (kind === 'servo' && p < 100 && gpioServos >= 2){ it.ok = false; it.why = 'limit'; }
      else { taken[p] = it; if (kind === 'servo' && p < 100) gpioServos++; }
      list.push(it);
    }
    if (pcaCfg()){ add(hwS.sda, 'PCA9685 SDA', 'SDA (PCA9685)', 'i2c', I2C_COL); add(hwS.scl, 'PCA9685 SCL', 'SCL (PCA9685)', 'i2c', I2C_COL); }
    for (var i = 0; i < n; i++)
      add(hwS.p[i], 'silnik ' + WH[i >> 1] + ' (' + IN[i % 4] + ')', (mec ? (i < 4 ? '#1 ' : '#2 ') : '') + IN[i % 4] + ' · ' + WH[i >> 1], 'motor', WCOL[i >> 1]);
    add(hwS.lp, 'dioda', hwS.lp === 4 ? 'dioda (na płytce)' : 'dioda', 'light', C_LED);
    if (hwS.rp >= 100) hwS.rp = -1;
    add(hwS.rp, 'odbiornik ELRS', 'ELRS TX', 'rc', C_RC);
    if (ioS){
      ioS.s.forEach(function(sv, i){ add(sv.o, 'serwo „' + sv.n + '”', 'serwo ' + sv.n, 'servo', SV_COL, i); });
      ioS.a.forEach(function(a, i){ if (a.t === 2) add(a.o, 'wyjście przycisku ' + (a.l || i + 1), 'wyjście ' + (a.l || 'przycisk ' + (i + 1)), 'out', OUT_COL, i); });
    }
    return { list: list, taken: taken };
  }
  function hwMessages(){
    var err = [], warn = [], n = hwS.mode === 1 ? 8 : 4, mp = hwS.p.slice(0, n), u = usage();
    if (mp.indexOf(-1) >= 0) err.push('Każde wejście sterownika silników musi mieć wybrany pin.');
    if (hwS.sda >= 0 && hwS.sda === hwS.scl) err.push('SDA i SCL modułu PCA9685 muszą być na różnych pinach.');
    u.list.forEach(function(it){
      if (it.ok) return;
      var why = it.why === 'nopca' ? 'kanał PCA wymaga modułu PCA9685 – ustaw piny SDA i SCL w zakładce Serwa'
        : it.why === 'limit' ? 'na pinach ESP32 działają najwyżej 2 serwa – użyj PCA9685'
        : 'pin ' + gp(it.p) + ' zajmuje już ' + it.owner;
      if (it.kind === 'motor' || it.kind === 'i2c') err.push(it.name[0].toUpperCase() + it.name.slice(1) + ': ' + why + '. Robot uruchomi się z domyślnymi pinami.');
      else warn.push(it.name[0].toUpperCase() + it.name.slice(1) + ' będzie wyłączone: ' + why + '.');
    });
    var all = u.list.filter(function(it){ return it.ok; }).map(function(it){ return it.p; });
    if (all.indexOf(1) >= 0 || all.indexOf(3) >= 0)
      warn.push('U0T (IO1) i U0R (IO3) to piny programatora: Serial Monitor zostanie wyłączony, a przy wgrywaniu programu odłącz przewody z tych pinów.');
    if (mp.indexOf(1) >= 0) warn.push('U0T (IO1) przy starcie wysyła dane – podłączony silnik może na chwilę drgnąć.');
    if (mp.indexOf(4) >= 0) warn.push('IO4 steruje też diodą flash – będzie świecić podczas jazdy.');
    if (all.indexOf(2) >= 0) warn.push('IO2: przy wgrywaniu programu odłącz od niego przewód.');
    if (all.indexOf(12) >= 0) warn.push('IO12 musi mieć stan niski przy starcie – jeśli robot nie startuje, dodaj rezystor 10 kΩ do GND.');
    if (hwS.drv === 1 && hwS.md > 70) warn.push('TB6612 nie traci napięcia jak L298N – zmniejsz minimalną moc startową do ok. 40–60.');
    if (hwS.mode === 1 && mp.every(function(p){ return p < 100; }))
      warn.push('Mecanum na pinach ESP32 zajmuje wszystkie 8 wolnych pinów. Z modułem PCA9685 silniki przejdą na jego kanały i zwolnią piny dla ELRS, diody i serw.');
    if (ioS && ioS.s.some(function(sv){ return sv.o === 1; })) warn.push('Serwo na U0T (IO1) może drgnąć przy włączaniu robota.');
    if (pcaCfg() && !hwS.pcaOk && !hwS.pending) warn.push('Moduł PCA9685 nie odpowiada – sprawdź przewody SDA/SCL, zasilanie VCC i adres.');
    return { err: err, warn: warn };
  }
  function msgHtml(m){
    var h = '';
    m.err.forEach(function(t){ h += '<div class="msg err">' + t + '</div>'; });
    m.warn.forEach(function(t){ h += '<div class="msg warn">' + t + '</div>'; });
    if ((hwS.pending || (ioS && ioS.pending)) && !m.err.length) h += '<div class="msg ok">Zmiany zapisane – uruchom robota ponownie, żeby zaczęły działać.</div>';
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
    for (var k = 0; k < sels.length; k++) pinOptions(sels[k], hwS.p[k], false, true);
    pinOptions(document.getElementById('lp'), hwS.lp, true, true);
    pinOptions(document.getElementById('rp'), hwS.rp, true, false);
    pinOptions(menu.querySelector('[data-pin="sda"]'), hwS.sda, true, false);
    pinOptions(menu.querySelector('[data-pin="scl"]'), hwS.scl, true, false);
    var pst = document.getElementById('pcast');
    pst.className = 'msg ' + (!pcaCfg() ? 'warn' : hwS.pcaOk ? 'ok' : 'err');
    pst.textContent = !pcaCfg() ? 'Moduł wyłączony – wybierz piny SDA i SCL.'
      : hwS.pcaOk ? 'Moduł PCA9685 działa.' : hwS.pending ? 'Uruchom robota ponownie, żeby włączyć moduł.' : 'Moduł nie odpowiada.';
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
    ['hwmsg', 'rcmsg', 'iomsg', 'actmsg'].forEach(function(id){ document.getElementById(id).innerHTML = html; });
    var rb = menu.querySelectorAll('.hwreboot'), pend = hwS.pending || (ioS && ioS.pending);
    for (var b = 0; b < rb.length; b++) rb[b].disabled = !pend || m.err.length > 0;
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
      if (/^p\d$/.test(t.dataset.pin)) hwS.p[+t.dataset.pin.slice(1)] = v; else hwS[t.dataset.pin] = v;
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
    function tagCh(c, t){ if (c) used[c] = (used[c] ? used[c] + ', ' : '') + t; }
    if (hwS) hwS.c.forEach(function(c, i){ if (i !== 2 || hwS.mode === 1) tagCh(c, CH_FUNCS[i]); });
    if (ioS){
      ioS.s.forEach(function(sv){ if (sv.ct === 3) tagCh(sv.ch, 'serwo ' + sv.n); });
      ioS.a.forEach(function(a, i){ if (a.t) tagCh(a.ch, 'przycisk ' + (a.l || i + 1)); });
    }
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
  loadLayout();

  // ---------- serwa i przyciski (menu) ----------
  var SV_CTRL = ['tylko przyciski', 'przeciąganie obrazu w poziomie (pan)', 'przeciąganie obrazu w pionie (tilt)', 'kanał aparatury'];
  var ACT_TYPES = ['— nieużywany —', 'Światło – włącz/wyłącz', 'Wyjście – włącz/wyłącz', 'Serwo – pozycja', 'Kamera – na środek',
                   'Moc – ustaw', 'Turbo – 100% przytrzymane'];
  var ACT_ICON = ['', '💡', '⏻', '↔', '◎', '%', '🚀'];
  function opt(list, v){ return list.map(function(t, i){ return '<option value="' + i + '"' + (i === v ? ' selected' : '') + '>' + esc(t) + '</option>'; }).join(''); }
  function chOpts(v){
    var h = '<option value="0">brak</option>';
    for (var c = 1; c <= 16; c++) h += '<option value="' + c + '"' + (c === v ? ' selected' : '') + '>CH' + c + (c >= 5 && c <= 12 ? ' (AUX' + (c - 4) + ')' : '') + '</option>';
    return h;
  }
  function rngFmt(u, v){ return u === 'sp' ? (v ? v * 10 + '°/s' : 'natychmiast') : v + u; }
  function rng(label, attr, key, v, min, max, step, u){
    return '<div class="grp"><label>' + label + ' <span class="val" data-u="' + u + '">' + rngFmt(u, v) + '</span></label>' +
      '<input type="range" ' + attr + '="' + key + '" min="' + min + '" max="' + max + '" step="' + step + '" value="' + v + '"></div>';
  }
  var ioTimer = null;
  function ioReload(){ clearTimeout(ioTimer); ioTimer = setTimeout(loadHw, 350); }

  function renderIo(){
    if (!ioS || !hwS) return;
    var u = usage(), h = '';
    ioS.s.forEach(function(sv, i){
      var st = u.list.filter(function(it){ return it.kind === 'servo' && it.ix === i; })[0];
      h += '<div class="wcard" data-s="' + i + '"><div class="inrow"><input type="text" data-sk="n" maxlength="11" value="' + esc(sv.n) + '"></div>' +
        '<div class="pinrow"><span>Wyjście (sygnał)</span><select data-sk="o"></select></div>';
      if (sv.o >= 0){
        h += '<div class="pinrow"><span>Sterowanie</span><select data-sk="ct">' + opt(SV_CTRL, sv.ct) + '</select></div>';
        if (sv.ct === 3) h += '<div class="pinrow"><span>Kanał aparatury</span><select data-sk="ch">' + chOpts(sv.ch) + '</select></div>';
        h += rng('Najmniej', 'data-sk', 'mn', sv.mn, 0, 180, 1, '°') + rng('Najwięcej', 'data-sk', 'mx', sv.mx, 0, 180, 1, '°') +
             rng('Środek', 'data-sk', 'c', sv.c, 0, 180, 1, '°') + rng('Prędkość', 'data-sk', 'sp', sv.sp, 0, 30, 1, 'sp');
        h += '<div class="row"><label>Odwróć kierunek</label><input type="checkbox" data-sk="inv"' + (sv.inv ? ' checked' : '') + '></div>';
        h += '<div class="inrow"><button class="small" data-test="mn">◀ min</button><button class="small" data-test="c">środek</button><button class="small" data-test="mx">max ▶</button></div>';
        if (st && !st.ok) h += '<p class="wnote">Nieaktywne – szczegóły niżej.</p>';
        else if (!sv.ok) h += '<p class="wnote">Zacznie działać po ponownym uruchomieniu robota.</p>';
      }
      h += '</div>';
    });
    $('servos').innerHTML = h;
    $('servos').querySelectorAll('[data-sk="o"]').forEach(function(sel){ pinOptions(sel, ioS.s[+sel.closest('[data-s]').dataset.s].o, true, true); });

    $('acts').innerHTML = ioS.a.map(actCard).join('');
    $('acts').querySelectorAll('[data-ak="o"]').forEach(function(sel){ pinOptions(sel, ioS.a[+sel.closest('[data-a]').dataset.a].o, true, true); });
    ioS.s.forEach(function(sv, i){ if (lookPos[i] === undefined) lookPos[i] = sv.pos; });
    ensureLayout();
    if (editing) renderPanel();
  }
  function actCard(a, i){
    var h = '<div class="wcard" data-a="' + i + '"><div class="pinrow"><b>Przycisk ' + (i + 1) + '</b><select data-ak="t">' + opt(ACT_TYPES, a.t) + '</select></div>';
    if (a.t){
      h += '<div class="pinrow"><span>Napis (np. emoji)</span><input type="text" data-ak="l" maxlength="7" value="' + esc(a.l) + '" placeholder="' + ACT_ICON[a.t] + '"></div>';
      if (a.t === 2 || a.t === 3) h += '<div class="pinrow"><span>Działanie</span><select data-ak="m">' + opt(['przełącznik (klik włącza i wyłącza)', 'tylko przytrzymany'], a.m) + '</select></div>';
      if (a.t === 2) h += '<div class="pinrow"><span>Wyjście</span><select data-ak="o"></select></div>';
      if (a.t === 3) h += '<div class="pinrow"><span>Serwo</span><select data-ak="sv">' + opt(ioS.s.map(function(x){ return x.n; }), a.sv) + '</select></div>' +
        rng('Pozycja po włączeniu', 'data-ak', 'a', a.a, 0, 180, 1, '°') + rng('Pozycja po wyłączeniu', 'data-ak', 'b', a.b, 0, 180, 1, '°');
      if (a.t === 5) h += rng('Moc', 'data-ak', 'a', Math.max(20, Math.min(100, a.a)), 20, 100, 10, '%');
      if (a.t >= 1 && a.t <= 4) h += '<div class="pinrow"><span>Też z kanału aparatury</span><select data-ak="ch">' + chOpts(a.ch) + '</select></div>';
    }
    return h + '</div>';
  }

  // zmiany w kartach serw i przycisków
  function utf8Cut(t, n){ while (new TextEncoder().encode(t).length > n) t = t.slice(0, -1); return t; }
  function ioChange(e){
    var t = e.target, sk = t.dataset.sk, ak = t.dataset.ak;
    if (!sk && !ak) return;
    var card = t.closest(sk ? '[data-s]' : '[data-a]'), i = +(sk ? card.dataset.s : card.dataset.a), key = sk || ak, v, sendV;
    if (t.type === 'checkbox'){ v = t.checked ? 1 : 0; sendV = v; }
    else if (t.type === 'text'){ v = utf8Cut(t.value.trim(), key === 'n' ? 11 : 7); t.value = v; sendV = encodeURIComponent(v); }
    else { v = +t.value; sendV = v; }
    (sk ? ioS.s : ioS.a)[i][key] = v;
    send((sk ? 'S ' : 'A ') + i + ' ' + key + ' ' + sendV);
    if (sk && key === 'c'){ lookPos[i] = v; }
    if (ak) renderLayout();           // nowy napis / funkcja przycisku od razu na ekranie
    ioReload();
  }
  menu.addEventListener('change', ioChange);
  epanel.addEventListener('change', ioChange);
  function ioInput(e){
    var t = e.target;
    if (t.type !== 'range' || !(t.dataset.sk || t.dataset.ak)) return;
    var sh = t.parentNode.querySelector('[data-u]');
    if (sh) sh.textContent = rngFmt(sh.dataset.u, +t.value);
    // podgląd na żywo: serwo jedzie za suwakiem zakresu / środka
    if (t.dataset.sk && /^(mn|mx|c)$/.test(t.dataset.sk)) send('S ' + t.closest('[data-s]').dataset.s + ' p ' + t.value);
  }
  menu.addEventListener('input', ioInput);
  epanel.addEventListener('input', ioInput);
  menu.addEventListener('click', function(e){
    var k = e.target.dataset.test;
    if (!k) return;
    var i = +e.target.closest('[data-s]').dataset.s;
    send('S ' + i + ' p ' + ioS.s[i][k]);
    lookPos[i] = ioS.s[i][k];
  });
  document.getElementById('ioreset').addEventListener('click', function(){ send('S 0 reset 1'); ioReload(); });

  // ---------- przyciski na ekranie ----------
  var actMaskNow = 0, turboPrev = null, turboIdx = -1;
  function markButtons(){
    if (!ioS) return;
    var bs = document.querySelectorAll('#lay [data-b]');
    for (var k = 0; k < bs.length; k++){
      var i = +bs[k].dataset.b, a = ioS.a[i];
      var on = a.t === 1 ? light : a.t === 6 ? turboIdx === i : a.t === 5 ? speed === a.a : !!((actMaskNow >> i) & 1);
      bs[k].classList.toggle('on', on);
    }
  }
  function pressAct(i, down){
    var a = ioS.a[i];
    if (a.t === 5){ if (down) setSpeed(a.a); }
    else if (a.t === 6){
      if (down && turboIdx < 0){ turboPrev = speed; turboIdx = i; setSpeed(100); }
      else if (!down && turboIdx === i){ turboIdx = -1; setSpeed(turboPrev); }
    }
    else send('B ' + i + ' ' + (down ? 1 : 0));
    markButtons();
  }
  // ---------- pole kamery: przeciąganie steruje serwami pan / tilt ----------
  var lookPos = {}, LOOK_K = 0.3;     // stopnie na piksel
  function lookServos(role){
    return ioS ? ioS.s.map(function(sv, i){ return { s: sv, i: i }; }).filter(function(x){ return x.s.ct === role && x.s.ok; }) : [];
  }
  function lookSend(hud){
    lookServos(1).concat(lookServos(2)).forEach(function(x){ send('S ' + x.i + ' p ' + Math.round(lookPos[x.i])); });
    var pan = lookServos(1)[0], tilt = lookServos(2)[0];
    if (hud) hud.textContent = (pan ? 'pan ' + Math.round(lookPos[pan.i]) + '°' : '') + (pan && tilt ? ' · ' : '') +
      (tilt ? 'tilt ' + Math.round(lookPos[tilt.i]) + '°' : '') || 'brak serw pan/tilt';
  }
  function bindPad(el){
    var hud = el.querySelector('.padhud'), drag = null, lastTap = 0, sent = 0;
    el.addEventListener('pointerdown', function(e){
      e.preventDefault();
      var now = Date.now();
      if (now - lastTap < 300){        // dwuklik – kamera na środek
        lookServos(1).concat(lookServos(2)).forEach(function(x){ lookPos[x.i] = x.s.c; });
        lookSend(hud);
        lastTap = 0;
        return;
      }
      lastTap = now;
      try { el.setPointerCapture(e.pointerId); } catch (err) {}
      var q = P(e);
      drag = { id: e.pointerId, x: q.x, y: q.y, start: Object.assign({}, lookPos) };
    });
    el.addEventListener('pointermove', function(e){
      if (!drag || e.pointerId !== drag.id) return;
      var q = P(e), dx = (q.x - drag.x) * LOOK_K, dy = (q.y - drag.y) * LOOK_K;
      lookServos(1).forEach(function(x){ lookPos[x.i] = Math.max(x.s.mn, Math.min(x.s.mx, drag.start[x.i] + dx)); });
      lookServos(2).forEach(function(x){ lookPos[x.i] = Math.max(x.s.mn, Math.min(x.s.mx, drag.start[x.i] - dy)); });
      if (Date.now() - sent > 40){ lookSend(hud); sent = Date.now(); }
    });
    function end(e){ if (drag && e.pointerId === drag.id){ lookSend(hud); drag = null; } }
    el.addEventListener('pointerup', end);
    el.addEventListener('pointercancel', end);
  }

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
    // piny ESP32: to, co działa; konflikt silników / I2C na czerwono, wyłączone dodatki pomijamy (opis w kartach)
    usage().list.forEach(function(it){
      if (it.p >= 100) return;
      if (it.ok) m[it.p] = { t: it.short, c: it.c };
      else if (it.why === 'conflict' && (it.kind === 'motor' || it.kind === 'i2c'))
        m[it.p] = { t: '⚠ ' + (m[it.p] ? m[it.p].t : '') + ' + ' + it.short, c: '#ff4d4d', bad: true };
    });
    var p5 = tb || pcaCfg() || (ioS && ioS.s.some(function(sv){ return sv.o >= 0; }));
    m.p5v = { t: p5 ? '← przetwornica 5 V' : '← +5V z L298N', c: C_PWR };
    m.gnd = { t: 'masa wspólna', c: C_GND };
    var to33 = [];
    if (tb) to33.push('TB6612');
    if (pcaCfg()) to33.push('PCA VCC');
    m.p33 = to33.length ? { t: '→ ' + to33.join(', '), c: C_PWR } : { t: 'nie podłączaj', c: C_OFF };
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
  function gp(p){ return p >= 100 ? 'PCA ' + (p - 100) : p >= 0 ? (p === 1 ? 'U0T (IO1)' : p === 3 ? 'U0R (IO3)' : 'IO' + p) : '–'; }

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

  // PCA9685 (moduł jak Adafruit): listwa po lewej, kanały 0–15 na dole
  function pcaCard(u){
    if (!pcaCfg()) return '';
    var body = '<rect x="66" y="10" width="240" height="150" rx="6" fill="#1d2a3a" stroke="#4f7fb0"/>' +
      '<text x="186" y="70" fill="#9fc2e6" font-size="12" font-weight="600" text-anchor="middle">PCA9685</text>' +
      '<text x="186" y="86" fill="#6f8fb0" font-size="9" text-anchor="middle">adres 0x' + hwS.pa.toString(16).toUpperCase() + ' · ' + hwS.pq + ' Hz</text>';
    var L = [['GND', 'masa', C_GND], ['OE', 'wolny', C_OFF], ['SCL', gp(hwS.scl), I2C_COL], ['SDA', gp(hwS.sda), I2C_COL],
             ['VCC', '3V3', C_PWR], ['V+', '5–6 V serwa', C_PWR]];
    var pins = L.map(function(p, i){ return [70, 26 + i * 18, 'l', p[0], p[1], p[2]]; });
    for (var c = 0; c < 16; c++){
      var it = u.list.filter(function(x){ return x.p === 100 + c && x.ok; })[0];
      pins.push([92 + c * 13, 150, 'b', String(c), it ? it.short.slice(0, 12) : '', it ? it.c : C_OFF]);
    }
    var h = '<div class="wcard"><h4>PCA9685 – 16 wyjść</h4>' + moduleSvg(320, 250, body, pins);
    h += wrow(C_PWR, 'VCC ←', '3V3 z ESP32-CAM (logika)') + wrow(I2C_COL, 'SDA ←', gp(hwS.sda)) + wrow(I2C_COL, 'SCL ←', gp(hwS.scl));
    h += wrow(C_PWR, 'V+ ←', '5–6 V z przetwornicy (zasilanie serw, zacisk śrubowy)') + wrow(C_GND, 'GND', 'masa wspólna');
    h += '<p class="wnote">Każdy kanał ma 3 piny: sygnał (PWM), V+ i GND – wtyczka serwa pasuje wprost (żółty/pomarańczowy przewód do PWM).</p>';
    if (!hwS.pcaOk && !hwS.pending) h += '<p class="wnote">Moduł nie odpowiada – sprawdź SDA/SCL, VCC i adres (zworki A0–A5).</p>';
    return h + '</div>';
  }
  function servoCard(u){
    var items = u.list.filter(function(x){ return x.kind === 'servo'; });
    if (!items.length) return '';
    var h = '<div class="wcard"><h4>Serwa</h4>';
    items.forEach(function(it){
      h += wrow(it.ok ? SV_COL : '#ff4d4d', ioS.s[it.ix].n, 'sygnał ← ' + gp(it.p) + (it.ok ? '' : ' – nieaktywne (' + (it.why === 'limit' ? 'za dużo serw na pinach ESP32' : it.why === 'nopca' ? 'brak PCA9685' : 'pin zajęty') + ')'));
    });
    h += wrow(C_PWR, '+ (czerwony)', items.some(function(x){ return x.p >= 100; }) ? 'V+ na PCA9685 albo 5 V z przetwornicy' : '5 V z przetwornicy');
    h += wrow(C_GND, '− (brązowy)', 'masa wspólna');
    h += '<p class="wnote">Serwo pobiera chwilowo do ok. 0,7 A. Zasilaj serwa z przetwornicy 5 V (min. 2 A), nie ze stabilizatora L298N ani z pinu 3V3 – inaczej ESP32 będzie się restartował.</p>';
    return h + '</div>';
  }
  function outCard(u){
    var items = u.list.filter(function(x){ return x.kind === 'out'; });
    if (!items.length) return '';
    var h = '<div class="wcard"><h4>Wyjścia przycisków</h4>';
    items.forEach(function(it){ h += wrow(it.ok ? OUT_COL : '#ff4d4d', ioS.a[it.ix].l || 'przycisk ' + (it.ix + 1), gp(it.p) + (it.ok ? '' : ' – nieaktywne')); });
    h += '<p class="wnote">Wyjście daje 3,3 V (pin ESP32 ok. 12 mA, kanał PCA ok. 25 mA). Diodę podłącz przez rezystor 220 Ω, a buzzer, silniczek czy przekaźnik – przez moduł z tranzystorem.</p>';
    return h + '</div>';
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
    var mp = hwS.p.slice(0, mec ? 8 : 4), u = usage();
    h += pcaCard(u) + servoCard(u) + outCard(u);
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
  var HW_KEYS = ['mode', 'mot', 'md', 'lp', 'rp', 'pf', 'fo', 'rt', 'rv', 'tg', 'mp', 'ac', 'lt', 'dz', 'rd', 'rm', 'drv', 'sda', 'scl', 'pa', 'pq'];
  var SV_KEYS = ['n', 'o', 'mn', 'mx', 'c', 'inv', 'sp', 'ct', 'ch'], ACT_KEYS = ['t', 'm', 'o', 'sv', 'a', 'b', 'ch', 'sh', 'l'];
  function getJson(u){ return fetch(u, { cache: 'no-store' }).then(function(r){ return r.json(); }); }
  $('bkdown').addEventListener('click', function(){
    Promise.all([getJson('/settings'), getJson('/hw'), getJson('/wifi'), getJson('/sys'), getJson('/io'), getJson('/layout')]).then(function(r){
      var out = { app: 'esp32-cam-robot', ver: r[3].ver, cam: {}, hw: { p: r[1].p, inv: r[1].inv, c: r[1].c }, wifi: {} };
      CAM_KEYS.forEach(function(k){ out.cam[k] = r[0][k]; });
      HW_KEYS.forEach(function(k){ out.hw[k] = r[1][k]; });
      ['mode', 'keep', 'ch', 'as', 'ss', 'hn'].forEach(function(k){ out.wifi[k] = r[2][k]; });
      if (r[5] && r[5].land) out.layout = r[5];
      out.io = {
        s: r[4].s.map(function(x){ var o = {}; SV_KEYS.forEach(function(k){ o[k] = x[k]; }); return o; }),
        a: r[4].a.map(function(x){ var o = {}; ACT_KEYS.forEach(function(k){ o[k] = x[k]; }); return o; })
      };
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
      // układ ekranu najpierw (osobne żądanie), potem ustawienia i restart
      return fetch('/layout', { method: 'POST', body: JSON.stringify(d.layout && d.layout.land ? d.layout : {}) }).then(function(){ return d; });
    }).then(function(d){
      CAM_KEYS.forEach(function(k){ if (d.cam[k] !== undefined) send('C ' + k + ' ' + d.cam[k]); });
      HW_KEYS.forEach(function(k){ if (d.hw[k] !== undefined) send('H ' + k + ' ' + d.hw[k]); });
      (d.hw.p || []).forEach(function(v, i){ send('H p' + i + ' ' + v); });
      (d.hw.inv || []).forEach(function(v, i){ send('H inv' + i + ' ' + v); });
      (d.hw.c || []).forEach(function(v, i){ send('H c' + i + ' ' + v); });
      if (d.io){
        (d.io.s || []).forEach(function(x, i){ SV_KEYS.forEach(function(k){ if (x[k] !== undefined) send('S ' + i + ' ' + k + ' ' + (k === 'n' ? encodeURIComponent(x[k]) : x[k])); }); });
        (d.io.a || []).forEach(function(x, i){ ACT_KEYS.forEach(function(k){ if (x[k] !== undefined) send('A ' + i + ' ' + k + ' ' + (k === 'l' ? encodeURIComponent(x[k]) : x[k])); }); });
      }
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
