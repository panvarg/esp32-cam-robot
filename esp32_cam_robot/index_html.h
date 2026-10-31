#pragma once

// Strona sterowania: podgląd z kamery na całym ekranie + dwie wirtualne gałki
// (lewa = gaz przód/tył, prawa = skręt lewo/prawo), jak w grach wyścigowych.
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
  *{box-sizing:border-box;-webkit-tap-highlight-color:transparent;user-select:none;-webkit-user-select:none}
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
  .quality{position:fixed;top:10px;right:10px;z-index:2;display:flex;gap:4px;background:rgba(0,0,0,.45);
    padding:4px;border-radius:10px}
  .quality button{height:30px;padding:0 10px;border-radius:8px;border:1px solid rgba(255,255,255,.4);
    background:rgba(255,255,255,.1);color:#fff;font-size:12px}
  .quality button.on{background:rgba(80,170,255,.85);border-color:#6bf}
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
  <div class="speed">
    <button id="down">&minus;</button>
    <span id="spd">70%</span>
    <button id="up">+</button>
  </div>
</div>
<div class="quality">
  <button data-q="0">Płynnie</button>
  <button data-q="1">Balans</button>
  <button data-q="2">Jakość</button>
</div>
<div id="hint" class="hud">WASD / strzałki · F światło</div>

<script>
(function(){
  var host = location.hostname || '192.168.4.1';
  var RADIUS = 85;          // zakres ruchu gałki (px)
  var DEADZONE = 0.08;      // martwa strefa analogu
  var TURN = 0.75;          // czułość skrętu

  // ---------- kamera ----------
  var cam = document.getElementById('cam');
  function startCam(){ cam.src = 'http://' + host + ':81/stream?t=' + Date.now(); }
  cam.onload = function(){ document.getElementById('nocam').style.display = 'none'; };
  cam.onerror = function(){ document.getElementById('nocam').style.display = 'flex'; setTimeout(startCam, 2000); };
  startCam();

  // ---------- WebSocket ----------
  var ws = null, dot = document.getElementById('dot'), stxt = document.getElementById('stxt');
  function connect(){
    ws = new WebSocket('ws://' + host + '/ws');
    ws.onopen = function(){ dot.className = 'ok'; stxt.textContent = 'połączono'; sendQuality(); };
    ws.onmessage = function(e){
      var p = String(e.data).split(' ');
      if (p[0] !== 'R') return;
      showRc(+p[1]);
      var l = p[2] === '1';
      if (l !== light){ light = l; lightEl.classList.toggle('on', light); }
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

  // ---------- jakość wideo ----------
  var quality = 1;
  try { var sq = parseInt(localStorage.getItem('q'), 10); if (sq >= 0 && sq <= 2) quality = sq; } catch(e){}
  var qBtns = document.querySelectorAll('.quality button');
  function showQuality(){ for (var i = 0; i < qBtns.length; i++) qBtns[i].classList.toggle('on', +qBtns[i].dataset.q === quality); }
  function sendQuality(){ send('Q ' + quality); }
  for (var i = 0; i < qBtns.length; i++) qBtns[i].addEventListener('pointerdown', function(e){
    e.preventDefault(); e.stopPropagation();
    quality = +this.dataset.q; showQuality(); sendQuality();
    try { localStorage.setItem('q', quality); } catch(e2){}
  });
  showQuality();

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

  function makeStick(zone, axis){
    var base = zone.querySelector('.base'), knob = zone.querySelector('.knob');
    var st = { id: null, c: 0, v: 0, edge: false };
    function home(){
      var r = zone.getBoundingClientRect();
      base.style.left = (r.width / 2) + 'px';
      base.style.top = (r.height / 2) + 'px';
      base.style.opacity = '.55';
    }
    function move(e){
      var d = (axis === 'y' ? e.clientY : e.clientX) - st.c;
      d = Math.max(-RADIUS, Math.min(RADIUS, d));
      knob.style.transform = axis === 'y' ? 'translateY(' + d + 'px)' : 'translateX(' + d + 'px)';
      var n = Math.abs(d) / RADIUS;
      var v = n < DEADZONE ? 0 : (n - DEADZONE) / (1 - DEADZONE);   // przeskalowanie poza martwą strefą
      st.v = (axis === 'y' ? -1 : 1) * (d < 0 ? -v : v);          // gaz: w górę = do przodu
      if (navigator.vibrate && n >= 1 && !st.edge) navigator.vibrate(8);
      st.edge = n >= 1;
    }
    function end(e){
      if (e.pointerId !== st.id) return;
      st.id = null; st.v = 0; st.edge = false;
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
      if (axis === 'y') cy = Math.max(r.top + m, Math.min(window.innerHeight - m, cy));
      else cx = Math.max(r.left + m, Math.min(r.right - m, cx));
      st.c = axis === 'y' ? cy : cx;                 // gałka pojawia się pod palcem
      base.style.left = (cx - r.left) + 'px';
      base.style.top = (cy - r.top) + 'px';
      base.style.opacity = '1';
      move(e);
    });
    zone.addEventListener('pointermove', function(e){ if (e.pointerId === st.id) move(e); });
    zone.addEventListener('pointerup', end);
    zone.addEventListener('pointercancel', end);
    window.addEventListener('resize', function(){ if (st.id === null) home(); });
    home();
    return st;
  }
  var zl = document.getElementById('zl'), zr = document.getElementById('zr');
  var gas = makeStick(zl, 'y'), steer = makeStick(zr, 'x');

  // ---------- klawiatura ----------
  var keys = {};
  window.addEventListener('keydown', function(e){
    var k = e.key.toLowerCase();
    if (k === 'f' && !e.repeat) toggleLight();
    keys[k] = true;
  });
  window.addEventListener('keyup', function(e){ keys[e.key.toLowerCase()] = false; });
  function keyAxis(){
    var x = 0, y = 0;
    if (keys['w'] || keys['arrowup']) y += 1;
    if (keys['s'] || keys['arrowdown']) y -= 1;
    if (keys['d'] || keys['arrowright']) x += 1;
    if (keys['a'] || keys['arrowleft']) x -= 1;
    return { x: x, y: y };
  }

  // ---------- wysyłanie ----------
  var last = '', lastSent = 0, readout = document.getElementById('readout');
  function command(){
    var k = keyAxis();
    var x = steer.id !== null ? steer.v : k.x;
    var y = gas.id !== null ? gas.v : k.y;
    var s = speed / 100;
    var X = Math.round(x * TURN * s * 100), Y = Math.round(y * s * 100);
    // podgląd mocy kół (to samo miksowanie co w ESP32)
    var l = Y + X, r = Y - X, m = Math.max(Math.abs(l), Math.abs(r));
    if (m > 100){ l = Math.round(l * 100 / m); r = Math.round(r * 100 / m); }
    readout.textContent = 'L ' + l + ' · P ' + r;
    return 'M ' + X + ' ' + Y;
  }
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
