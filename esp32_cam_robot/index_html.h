#pragma once

// Strona sterowania: podgląd z kamery na całym ekranie + wirtualny analog.
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
  #readout{bottom:10px;left:50%;transform:translateX(-50%);font-variant-numeric:tabular-nums}
  #side{position:fixed;right:16px;top:50%;transform:translateY(-50%);display:flex;
    flex-direction:column;gap:14px;align-items:center}
  .btn{width:64px;height:64px;border-radius:50%;border:2px solid rgba(255,255,255,.5);
    background:rgba(0,0,0,.4);color:#fff;font-size:26px;display:flex;align-items:center;
    justify-content:center;cursor:pointer}
  .btn.on{background:rgba(255,220,80,.85);color:#000;border-color:#fd5}
  .speed{display:flex;flex-direction:column;align-items:center;gap:4px;font-size:12px;
    background:rgba(0,0,0,.4);padding:8px 6px;border-radius:12px}
  .speed button{width:44px;height:36px;border-radius:8px;border:1px solid rgba(255,255,255,.4);
    background:rgba(255,255,255,.1);color:#fff;font-size:20px}
  #spd{font-size:16px;font-weight:600;font-variant-numeric:tabular-nums}
  .quality{position:fixed;top:10px;right:10px;display:flex;gap:4px;background:rgba(0,0,0,.45);
    padding:4px;border-radius:10px}
  .quality button{height:30px;padding:0 10px;border-radius:8px;border:1px solid rgba(255,255,255,.4);
    background:rgba(255,255,255,.1);color:#fff;font-size:12px}
  .quality button.on{background:rgba(80,170,255,.85);border-color:#6bf}
  #zone{position:fixed;left:0;bottom:0;width:55%;height:100%}
  #base{position:absolute;width:170px;height:170px;border-radius:50%;
    border:3px solid rgba(255,255,255,.55);background:radial-gradient(circle,rgba(255,255,255,.12),rgba(255,255,255,.04));
    transform:translate(-50%,-50%);pointer-events:none;transition:opacity .2s}
  #knob{position:absolute;left:50%;top:50%;width:76px;height:76px;margin:-38px 0 0 -38px;
    border-radius:50%;background:radial-gradient(circle at 35% 35%,#fff,#9ad 70%);
    box-shadow:0 2px 10px rgba(0,0,0,.6);opacity:.9}
  #hint{position:fixed;bottom:10px;right:10px;font-size:11px;color:#bbb}
  @media (pointer:coarse){#hint{display:none}}
  @media (orientation:portrait){
    #zone{width:100%;height:55%}
    #side{top:auto;bottom:auto;top:60px;transform:none}
  }
</style>
</head>
<body>
<div id="nocam">brak obrazu z kamery</div>
<img id="cam" alt="">

<div id="status" class="hud"><span id="dot"></span><span id="stxt">łączenie…</span></div>
<div id="readout" class="hud">L 0 · P 0</div>

<div id="zone"><div id="base"><div id="knob"></div></div></div>

<div id="side">
  <div class="btn" id="light" title="Światło (F)">&#128161;</div>
  <div class="speed">
    <button id="up">+</button>
    <span id="spd">70%</span>
    <span>moc</span>
    <button id="down">&minus;</button>
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
  var RADIUS = 85;          // promień ruchu gałki (px)
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
    ws.onopen = function(){ dot.className = 'ok'; stxt.textContent = 'połączono'; sendLight(); sendQuality(); };
    ws.onclose = function(){ dot.className = ''; stxt.textContent = 'rozłączono'; setTimeout(connect, 1000); };
    ws.onerror = function(){ try{ ws.close(); }catch(e){} };
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

  // ---------- analog ----------
  var zone = document.getElementById('zone'), base = document.getElementById('base'), knob = document.getElementById('knob');
  var stick = { id: null, cx: 0, cy: 0, x: 0, y: 0 };

  function homeBase(){
    var r = zone.getBoundingClientRect();
    base.style.left = (r.width / 2) + 'px';
    base.style.top = (r.height - Math.max(110, r.height * 0.3)) + 'px';
    base.style.opacity = '.6';
  }
  homeBase();
  window.addEventListener('resize', function(){ if (stick.id === null) homeBase(); });

  zone.addEventListener('pointerdown', function(e){
    if (stick.id !== null) return;
    e.preventDefault();
    stick.id = e.pointerId;
    zone.setPointerCapture(e.pointerId);
    var r = zone.getBoundingClientRect();
    stick.cx = e.clientX; stick.cy = e.clientY;   // analog pojawia się pod palcem
    base.style.left = (e.clientX - r.left) + 'px';
    base.style.top = (e.clientY - r.top) + 'px';
    base.style.opacity = '1';
    moveStick(e);
  });
  zone.addEventListener('pointermove', function(e){ if (e.pointerId === stick.id) moveStick(e); });
  function endStick(e){
    if (e.pointerId !== stick.id) return;
    stick.id = null; stick.x = 0; stick.y = 0;
    knob.style.transform = '';
    homeBase();
    pushNow();
  }
  zone.addEventListener('pointerup', endStick);
  zone.addEventListener('pointercancel', endStick);

  function moveStick(e){
    var dx = e.clientX - stick.cx, dy = e.clientY - stick.cy;
    var d = Math.sqrt(dx * dx + dy * dy);
    if (d > RADIUS){ dx = dx / d * RADIUS; dy = dy / d * RADIUS; d = RADIUS; }
    knob.style.transform = 'translate(' + dx + 'px,' + dy + 'px)';
    var n = d / RADIUS;
    if (n < DEADZONE){ stick.x = 0; stick.y = 0; }
    else {
      var k = (n - DEADZONE) / (1 - DEADZONE) / n;   // przeskalowanie poza martwą strefą
      stick.x = dx / RADIUS * k;
      stick.y = -dy / RADIUS * k;
    }
    if (navigator.vibrate && n >= 1 && !stick.edge){ navigator.vibrate(8); }
    stick.edge = n >= 1;
  }

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
    var x = stick.x, y = stick.y;
    if (stick.id === null){ var k = keyAxis(); x = k.x; y = k.y; }
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
