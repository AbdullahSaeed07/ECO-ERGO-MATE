
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <NewPing.h>
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h> 

// Pins
#define PIN_MOISTURE   2   
#define PIN_DHT        4   
#define PIN_TRIG       5   
#define PIN_ECHO       6   
#define PIN_PIR        7   
#define PIN_SDA        8   
#define PIN_SCL        9   

// Credentials
const char* ssid = "";              
const char* password = "";     
const char* mqtt_server = "broker.hivemq.com"; 

// Components
Adafruit_SSD1306 display(128, 64, &Wire, -1);
DHT dht(PIN_DHT, DHT22);
NewPing sonar(PIN_TRIG, PIN_ECHO, 200);
WebServer server(80); 
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// variables
float currentTemp = 0.0, currentHum = 0.0, currentDew = 0.0;
int currentDist = 0, currentMoisture = 0;

// Ergonomic and alert Limits
int slouchLimit = 35; 
int tempHighLimit = 28; 
int tempLowLimit = 18;   
int standLimitMin = 45;  

// Power Settings
bool enableSleep = true;
int sleepTrigger = 1;
int sleepDistLimit = 100; 
int sleepTimeoutSec = 15; 
bool isSleeping = false;

// Display Toggles
bool showTemp = true, showHum = true, showDew = false, showPlant = true;

// Timers
unsigned long lastSensorRead = 0, lastMotionTime = 0;
unsigned long presenceStartTime = 0;
bool isPresent = false;
int presenceMinutes = 0;

int zX = 10, zY = 10, zDirX = 1, zDirY = 1;


// dashboard
const char webpage_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Eco-Ergo-Mate</title>
  <style>
    :root {
      --bg: #f6f6f4; --surface: #ffffff; --border: #e3e3df; --text: #1d1d1b;
      --muted: #74746f; --track: #d6d6d1; --accent: #1d1d1b;
      --warn: #a15c07; --alert: #b42318;
    }
    @media (prefers-color-scheme: dark) {
      :root {
        --bg: #131313; --surface: #1b1b1b; --border: #2b2b2b; --text: #e9e9e7;
        --muted: #8d8d88; --track: #3a3a3a; --accent: #e9e9e7;
        --warn: #e0a043; --alert: #f07167;
      }
    }
    * { box-sizing: border-box; }
    body {
      margin: 0; padding: 32px 16px 48px; background: var(--bg); color: var(--text);
      font: 15px/1.45 -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      -webkit-font-smoothing: antialiased;
    }
    .wrap { max-width: 640px; margin: 0 auto; }

    header { display: flex; justify-content: space-between; align-items: baseline; margin-bottom: 24px; }
    h1 { margin: 0; font-size: 20px; font-weight: 600; letter-spacing: -0.01em; }
    .sub { margin: 2px 0 0; color: var(--muted); font-size: 13px; }
    .conn { font-size: 12px; color: var(--muted); display: flex; align-items: center; gap: 6px; }
    .dot { width: 7px; height: 7px; border-radius: 50%; background: var(--track); }
    .conn.on .dot { background: var(--text); }

    .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 1px; background: var(--border);
            border: 1px solid var(--border); border-radius: 10px; overflow: hidden; }
    @media (max-width: 420px) { .grid { grid-template-columns: 1fr; } }
    .card { background: var(--surface); padding: 18px 20px; }
    .label { font-size: 12px; color: var(--muted); text-transform: uppercase; letter-spacing: 0.06em; }
    .val { font-size: 30px; font-weight: 500; margin: 6px 0 4px; font-variant-numeric: tabular-nums; letter-spacing: -0.02em; }
    .val small { font-size: 15px; color: var(--muted); font-weight: 400; margin-left: 2px; }
    .stat { font-size: 13px; color: var(--muted); }
    .card.warn .stat { color: var(--warn); }
    .card.alert .stat { color: var(--alert); }
    .card.warn .val, .card.alert .val { color: inherit; }
    .card.alert .val { color: var(--alert); }

    h2 { font-size: 12px; font-weight: 500; color: var(--muted); text-transform: uppercase;
         letter-spacing: 0.06em; margin: 32px 0 8px; }
    .panel { background: var(--surface); border: 1px solid var(--border); border-radius: 10px; }
    details + details { border-top: 1px solid var(--border); }
    summary { padding: 14px 20px; cursor: pointer; list-style: none; display: flex;
              justify-content: space-between; align-items: center; font-weight: 500; }
    summary::-webkit-details-marker { display: none; }
    summary::after { content: ""; width: 7px; height: 7px; border-right: 1.5px solid var(--muted);
                     border-bottom: 1.5px solid var(--muted); transform: rotate(45deg); margin-top: -4px; transition: transform .15s; }
    details[open] summary::after { transform: rotate(-135deg); margin-top: 4px; }
    .body { padding: 4px 20px 18px; }

    .row { display: flex; justify-content: space-between; align-items: center; padding: 10px 0; gap: 12px; }
    .row + .row, .field + .field { border-top: 1px solid var(--border); }
    .field { padding: 10px 0 6px; }
    .field .head { display: flex; justify-content: space-between; font-size: 14px; }
    .field .head b { font-weight: 500; font-variant-numeric: tabular-nums; }
    input[type=range] { width: 100%; margin: 10px 0 4px; accent-color: var(--accent); }

    select, input[type=text] {
      font: inherit; font-size: 14px; color: var(--text); background: var(--bg);
      border: 1px solid var(--border); border-radius: 6px; padding: 7px 10px; outline: none;
    }
    select:focus, input[type=text]:focus { border-color: var(--muted); }
    input.invalid { border-color: var(--alert); }

    .switch { position: relative; width: 34px; height: 20px; flex: none; }
    .switch input { opacity: 0; width: 0; height: 0; position: absolute; }
    .switch span { position: absolute; inset: 0; background: var(--track); border-radius: 20px; cursor: pointer; transition: background .15s; }
    .switch span::before { content: ""; position: absolute; width: 16px; height: 16px; left: 2px; top: 2px;
                           background: var(--surface); border-radius: 50%; transition: transform .15s; }
    .switch input:checked + span { background: var(--accent); }
    .switch input:checked + span::before { transform: translateX(14px); }

    .profiles { display: flex; flex-wrap: wrap; gap: 8px; padding: 8px 0 14px; }
    .profiles:empty::before { content: "No saved profiles yet."; color: var(--muted); font-size: 14px; }
    .chip { display: flex; align-items: center; border: 1px solid var(--border); border-radius: 6px; overflow: hidden; }
    .chip button { font: inherit; font-size: 13px; background: none; border: none; color: var(--text); padding: 6px 10px; cursor: pointer; }
    .chip button:hover { background: var(--bg); }
    .chip .act { color: var(--muted); border-left: 1px solid var(--border); padding: 6px 8px; }
    .add { display: flex; gap: 8px; border-top: 1px solid var(--border); padding-top: 14px; }
    .add input { flex: 1; min-width: 0; }
    .btn { font: inherit; font-size: 14px; font-weight: 500; background: var(--accent); color: var(--surface);
           border: none; border-radius: 6px; padding: 7px 14px; cursor: pointer; }
    .hint { font-size: 13px; color: var(--muted); margin: 0 0 8px; }

    #sleepOverlay { display: none; position: fixed; inset: 0; background: var(--bg); z-index: 10;
                    flex-direction: column; align-items: center; justify-content: center; gap: 4px; }
    .sleeping #sleepOverlay { display: flex; }
    #sleepOverlay p { margin: 0; }
    #sleepOverlay .t { font-size: 18px; font-weight: 500; }
    #sleepOverlay .s { font-size: 13px; color: var(--muted); }
  </style>
</head>
<body>
  <div id="sleepOverlay"><p class="t">Sleep mode</p><p class="s">Waiting for activity</p></div>

  <div class="wrap">
    <header>
      <div>
        <h1>Eco-Ergo-Mate</h1>
        <p class="sub">Workspace monitor</p>
      </div>
      <div class="conn" id="conn"><span class="dot"></span><span id="connText">Connecting</span></div>
    </header>

    <div class="grid">
      <div class="card" id="cTemp">
        <div class="label">Temperature</div>
        <div class="val"><span id="temp">--</span><small>&deg;C</small></div>
        <div class="stat" id="sTemp">--</div>
      </div>
      <div class="card" id="cHum">
        <div class="label">Humidity</div>
        <div class="val"><span id="hum">--</span><small>%</small></div>
        <div class="stat" id="sHum">--</div>
      </div>
      <div class="card" id="cDew">
        <div class="label">Dew point</div>
        <div class="val"><span id="dew">--</span><small>&deg;C</small></div>
        <div class="stat" id="sSoil">Soil moisture --%</div>
      </div>
      <div class="card" id="cPost">
        <div class="label">Posture distance</div>
        <div class="val"><span id="dist">--</span><small>cm</small></div>
        <div class="stat" id="sPost">--</div>
      </div>
    </div>

    <h2>Profiles</h2>
    <div class="panel"><div class="body" style="padding-top:8px">
      <div id="profileContainer" class="profiles"></div>
      <div class="add">
        <input type="text" id="newName" placeholder="Profile name" maxlength="24">
        <button class="btn" id="saveBtn" onclick="saveProfile()">Save</button>
      </div>
      <p class="hint" style="margin:8px 0 0">Saves the current settings below.</p>
    </div></div>

    <h2>Settings</h2>
    <div class="panel">
      <details>
        <summary>Alerts</summary>
        <div class="body">
          <div class="field">
            <div class="head"><span>Posture limit</span><span><b id="lVal">35</b> cm</span></div>
            <input type="range" id="limitSlider" min="10" max="80" value="35" oninput="updateLabels()" onchange="pushSettings()">
          </div>
          <div class="field">
            <div class="head"><span>Stand-up reminder</span><span><b id="stVal">45</b> min</span></div>
            <input type="range" id="standSlider" min="10" max="120" step="5" value="45" oninput="updateLabels()" onchange="pushSettings()">
          </div>
          <div class="field">
            <div class="head"><span>High temperature</span><span><b id="thVal">28</b> &deg;C</span></div>
            <input type="range" id="thSlider" min="20" max="40" value="28" oninput="updateLabels()" onchange="pushSettings()">
          </div>
          <div class="field">
            <div class="head"><span>Low temperature</span><span><b id="tlVal">18</b> &deg;C</span></div>
            <input type="range" id="tlSlider" min="10" max="25" value="18" oninput="updateLabels()" onchange="pushSettings()">
          </div>
        </div>
      </details>

      <details>
        <summary>Sleep and presence</summary>
        <div class="body">
          <div class="row">
            <span>Auto sleep</span>
            <label class="switch"><input type="checkbox" id="cEnSlp" checked onchange="pushSettings()"><span></span></label>
          </div>
          <div class="row">
            <span>Wake trigger</span>
            <select id="selTrg" onchange="pushSettings()">
              <option value="0">Motion sensor</option>
              <option value="1" selected>Distance</option>
            </select>
          </div>
          <div class="field">
            <div class="head"><span>Presence distance</span><span><b id="sdVal">100</b> cm</span></div>
            <input type="range" id="sDistSlider" min="30" max="200" value="100" oninput="updateLabels()" onchange="pushSettings()">
          </div>
          <div class="field">
            <div class="head"><span>Sleep after</span><span><b id="tVal">15</b> s</span></div>
            <input type="range" id="timeSlider" min="5" max="120" value="15" oninput="updateLabels()" onchange="pushSettings()">
          </div>
        </div>
      </details>

      <details>
        <summary>OLED display</summary>
        <div class="body">
          <div class="row"><span>Temperature</span><label class="switch"><input type="checkbox" id="cT" checked onchange="pushSettings()"><span></span></label></div>
          <div class="row"><span>Humidity</span><label class="switch"><input type="checkbox" id="cH" checked onchange="pushSettings()"><span></span></label></div>
          <div class="row"><span>Dew point</span><label class="switch"><input type="checkbox" id="cD" onchange="pushSettings()"><span></span></label></div>
          <div class="row"><span>Soil moisture</span><label class="switch"><input type="checkbox" id="cP" checked onchange="pushSettings()"><span></span></label></div>
        </div>
      </details>
    </div>
  </div>

  <script>
    const $ = id => document.getElementById(id);
    const sliders = [['limitSlider','lVal'],['standSlider','stVal'],['thSlider','thVal'],
                     ['tlSlider','tlVal'],['sDistSlider','sdVal'],['timeSlider','tVal']];
    let profiles = [];
    try { profiles = JSON.parse(localStorage.getItem('profiles')) || []; } catch (e) {}
    let editingId = null, synced = false;

    const esc = s => String(s).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));

    function updateLabels() { sliders.forEach(([s, l]) => $(l).textContent = $(s).value); }

    function getSettings() {
      return {
        limit: $('limitSlider').value, stand: $('standSlider').value,
        tempHigh: $('thSlider').value, tempLow: $('tlSlider').value,
        autoSleep: $('cEnSlp').checked, trigger: $('selTrg').value,
        presenceDist: $('sDistSlider').value, sleepTime: $('timeSlider').value,
        oledTemp: $('cT').checked, oledHum: $('cH').checked,
        oledDew: $('cD').checked, oledSoil: $('cP').checked
      };
    }

    function applySettings(s) {
      if (!s) return;
      $('limitSlider').value = s.limit; $('standSlider').value = s.stand;
      $('thSlider').value = s.tempHigh; $('tlSlider').value = s.tempLow;
      $('cEnSlp').checked = s.autoSleep; $('selTrg').value = s.trigger;
      $('sDistSlider').value = s.presenceDist; $('timeSlider').value = s.sleepTime;
      $('cT').checked = s.oledTemp; $('cH').checked = s.oledHum;
      $('cD').checked = s.oledDew; $('cP').checked = s.oledSoil;
      updateLabels();
      pushSettings();
    }

    // Send settings to the ESP32
    function pushSettings() {
      updateLabels();
      const s = getSettings(), b = v => v ? 1 : 0;
      const q = new URLSearchParams({
        lim: s.limit, stL: s.stand, th: s.tempHigh, tl: s.tempLow,
        to: s.sleepTime, sEn: b(s.autoSleep), sTrg: s.trigger, sDist: s.presenceDist,
        t: b(s.oledTemp), h: b(s.oledHum), d: b(s.oledDew), p: b(s.oledSoil)
      });
      fetch('/update?' + q).catch(() => {});
    }

    // Profiles
    function saveProfile() {
      const input = $('newName'), name = input.value.trim();
      if (!name) { input.classList.add('invalid'); input.focus(); return; }
      input.classList.remove('invalid');
      const settings = getSettings();
      if (editingId !== null) {
        const p = profiles.find(p => p.id === editingId);
        if (p) { p.name = name; p.settings = settings; }
        editingId = null;
        $('saveBtn').textContent = 'Save';
      } else {
        profiles.push({ id: Date.now(), name, settings });
      }
      store();
      input.value = '';
    }
    function deleteProfile(id) {
      if (!confirm('Delete this profile?')) return;
      profiles = profiles.filter(p => p.id !== id);
      store();
    }
    function editProfile(id) {
      const p = profiles.find(p => p.id === id);
      if (!p) return;
      applySettings(p.settings);
      $('newName').value = p.name;
      editingId = id;
      $('saveBtn').textContent = 'Update';
    }
    function loadProfile(id) {
      const p = profiles.find(p => p.id === id);
      if (p) applySettings(p.settings);
    }
    function store() {
      try { localStorage.setItem('profiles', JSON.stringify(profiles)); } catch (e) {}
      renderProfiles();
    }
    function renderProfiles() {
      $('profileContainer').innerHTML = profiles.map(p =>
        '<div class="chip">' +
          '<button onclick="loadProfile(' + p.id + ')">' + esc(p.name) + '</button>' +
          '<button class="act" onclick="editProfile(' + p.id + ')">Edit</button>' +
          '<button class="act" onclick="deleteProfile(' + p.id + ')">Delete</button>' +
        '</div>').join('');
    }

    // Live data
    function setCard(card, stat, text, level) {
      $(card).className = 'card' + (level ? ' ' + level : '');
      $(stat).textContent = text;
    }

    function render(d) {
      if (!synced) {
        $('limitSlider').value = d.lim; $('standSlider').value = d.stL;
        $('thSlider').value = d.th; $('tlSlider').value = d.tl;
        $('timeSlider').value = d.to; $('cEnSlp').checked = d.sEn == 1;
        $('selTrg').value = d.sTrg; $('sDistSlider').value = d.sDist;
        $('cT').checked = d.sT == 1; $('cH').checked = d.sH == 1;
        $('cD').checked = d.sD == 1; $('cP').checked = d.sP == 1;
        updateLabels();
        synced = true;
      }

      $('temp').textContent = d.temp.toFixed(1);
      $('hum').textContent = d.hum.toFixed(0);
      $('dew').textContent = d.dew.toFixed(1);
      $('dist').textContent = d.dist > 0 ? d.dist : '--';
      $('sSoil').textContent = 'Soil moisture ' + d.soil + '%';

      if (d.temp > d.th) setCard('cTemp', 'sTemp', 'Above ' + d.th + ' °C', 'alert');
      else if (d.temp < d.tl) setCard('cTemp', 'sTemp', 'Below ' + d.tl + ' °C', 'alert');
      else setCard('cTemp', 'sTemp', 'Normal', '');

      if (d.hum < 30) setCard('cHum', 'sHum', 'Dry', 'warn');
      else if (d.hum > 60) setCard('cHum', 'sHum', 'Humid', 'warn');
      else setCard('cHum', 'sHum', 'Normal', '');

      const seated = d.pMin > 0 ? ' · seated ' + d.pMin + ' min' : '';
      if (d.dist <= 0) setCard('cPost', 'sPost', 'No reading', '');
      else if (d.dist < d.lim) setCard('cPost', 'sPost', 'Too close, sit back' + seated, 'alert');
      else if (d.pMin >= d.stL) setCard('cPost', 'sPost', 'Time to stand up' + seated, 'warn');
      else setCard('cPost', 'sPost', 'Good' + seated, '');

      document.body.classList.toggle('sleeping', !!d.sleep);
    }

    function setConn(on) {
      $('conn').classList.toggle('on', on);
      $('connText').textContent = on ? 'Live' : 'Offline';
    }

    async function poll() {
      try {
        const r = await fetch('/data', { cache: 'no-store' });
        render(await r.json());
        setConn(true);
      } catch (e) { setConn(false); }
    }

    renderProfiles();
    updateLabels();
    poll();
    setInterval(poll, 2000);
  </script>
</body>
</html>
)rawliteral";

void setup() {
  delay(3000);
  Serial.begin(115200);
  pinMode(PIN_PIR, INPUT_PULLDOWN);
  WiFi.setTxPower(WIFI_POWER_11dBm); 
  
  Wire.begin(PIN_SDA, PIN_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  dht.begin();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println(WiFi.localIP());

  server.on("/", []() { server.send(200, "text/html", webpage_html); });
  
  server.on("/update", []() {
    if (server.hasArg("lim")) slouchLimit = server.arg("lim").toInt();
    if (server.hasArg("stL")) standLimitMin = server.arg("stL").toInt();
    if (server.hasArg("th")) tempHighLimit = server.arg("th").toInt();
    if (server.hasArg("tl")) tempLowLimit = server.arg("tl").toInt();
    if (server.hasArg("to")) sleepTimeoutSec = server.arg("to").toInt();
    if (server.hasArg("sEn")) enableSleep = (server.arg("sEn") == "1");
    if (server.hasArg("sTrg")) sleepTrigger = server.arg("sTrg").toInt();
    if (server.hasArg("sDist")) sleepDistLimit = server.arg("sDist").toInt();
    if (server.hasArg("t")) showTemp = (server.arg("t") == "1");
    if (server.hasArg("h")) showHum = (server.arg("h") == "1");
    if (server.hasArg("d")) showDew = (server.arg("d") == "1");
    if (server.hasArg("p")) showPlant = (server.arg("p") == "1");
    server.send(200, "text/plain", "OK");
  });
  
  server.on("/data", []() {
    String j = "{\"temp\":" + String(currentTemp) + ",\"hum\":" + String(currentHum) + 
               ",\"dew\":" + String(currentDew) + ",\"dist\":" + String(currentDist) + ",\"soil\":" + String(currentMoisture) +
               ",\"lim\":" + String(slouchLimit) + ",\"stL\":" + String(standLimitMin) + 
               ",\"th\":" + String(tempHighLimit) + ",\"tl\":" + String(tempLowLimit) +
               ",\"to\":" + String(sleepTimeoutSec) + ",\"sEn\":" + String(enableSleep?1:0) + 
               ",\"sTrg\":" + String(sleepTrigger) + ",\"sDist\":" + String(sleepDistLimit) + 
               ",\"pMin\":" + String(presenceMinutes) + ",\"sleep\":" + (isSleeping ? "true" : "false") +
               ",\"sT\":" + String(showTemp?1:0) + ",\"sH\":" + String(showHum?1:0) + 
               ",\"sD\":" + String(showDew?1:0) + ",\"sP\":" + String(showPlant?1:0) + "}";
    server.send(200, "application/json", j);
  });
  
  server.begin();
  mqttClient.setServer(mqtt_server, 1883);
}

void loop() {
  server.handleClient();
  
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) mqttClient.connect("EcoErgo_Ayan_Roj");
    mqttClient.loop();
  }

  // sensor
  if (millis() - lastSensorRead > 2000) { 
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) currentTemp = t;
    if (!isnan(h)) currentHum = h;
    currentDew = currentTemp - ((100.0 - currentHum) / 5.0);
    
    currentDist = sonar.ping_cm();
    currentMoisture = map(analogRead(PIN_MOISTURE), 4095, 0, 0, 100);
    lastSensorRead = millis();
    
    // data
    if (!isSleeping && mqttClient.connected()) {
      mqttClient.publish("ecoergo/posture", String(currentDist).c_str());
      mqttClient.publish("ecoergo/temperature", String(currentTemp, 1).c_str());
      mqttClient.publish("ecoergo/humidity", String(currentHum, 1).c_str());
      mqttClient.publish("ecoergo/dewpoint", String(currentDew, 1).c_str());
    }
  }

  // Presence tracker
  bool currentlyPresent = (currentDist > 0 && currentDist <= sleepDistLimit);
  
  if (currentlyPresent) {
    if (!isPresent) {
      isPresent = true;
      presenceStartTime = millis(); 
    }
    presenceMinutes = (millis() - presenceStartTime) / 60000;
  } else {
    isPresent = false;
    presenceMinutes = 0; 
  }

  // Sleep mode
  if (enableSleep) {
    bool isActive = (sleepTrigger == 0) ? (digitalRead(PIN_PIR) == HIGH) : currentlyPresent;

    if (isActive) {
      lastMotionTime = millis();
      if (isSleeping) { isSleeping = false; display.dim(false); }
    }
    if (!isSleeping && (millis() - lastMotionTime > (sleepTimeoutSec * 1000UL))) {
      isSleeping = true; display.dim(true); 
    }
  } else {
    if (isSleeping) { isSleeping = false; display.dim(false); }
    lastMotionTime = millis(); 
  }

  // ZZZ thingg for sleep mode
  display.clearDisplay();
  
  if (isSleeping) {
    display.setTextSize(2); display.setTextColor(WHITE);
    display.setCursor(zX, zY); display.print("Zzz");
    zX += zDirX; zY += zDirY;
    if (zX > 90 || zX < 0) zDirX = -zDirX;
    if (zY > 45 || zY < 0) zDirY = -zDirY;
  } 
  else {
    display.setTextSize(1); display.setTextColor(WHITE);
    int curY = 0;
    
    if(showTemp)  { display.setCursor(0, curY); display.print("Temp: "); display.print(currentTemp, 1); display.print(" C"); curY += 10; }
    if(showHum)   { display.setCursor(0, curY); display.print("Hum:  "); display.print(currentHum, 1); display.print(" %"); curY += 10; }
    if(showDew)   { display.setCursor(0, curY); display.print("Dew:  "); display.print(currentDew, 1); display.print(" C"); curY += 10; }
    if(showPlant) { display.setCursor(0, curY); display.print("Soil: "); display.print(currentMoisture); display.print(" %"); curY += 10; }

    // Physical OLED alerts
    if (currentDist > 0 && currentDist < slouchLimit) {
      display.fillRoundRect(0, 40, 128, 24, 4, WHITE);
      display.setTextColor(BLACK); display.setTextSize(2); display.setCursor(20, 44); display.println("SIT UP!");
    } else if (presenceMinutes >= standLimitMin && currentDist > 0) {
      display.fillRoundRect(0, 40, 128, 24, 4, WHITE);
      display.setTextColor(BLACK); display.setTextSize(2); display.setCursor(15, 44); display.println("STAND!");
    } else {
      display.setCursor(0, 45); display.print("Posture: "); display.print(currentDist); display.print("cm");
    }
  }
  display.display();
}