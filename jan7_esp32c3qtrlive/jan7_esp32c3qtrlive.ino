/*******************************************************************
 * ESP32-C3 Super Mini: "Team Sync" PID Tuner
 * UPDATED: Fixed "Current Settings" Text Display (Multi-line)
 *******************************************************************/

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

// --- CONFIGURATION ---
const char* ssid     = "QTR_Live_C3";
const char* password = "12345678";
const int LED_PIN    = 8;        // Built-in LED on Super Mini
bool ledActiveLow    = true;     // Set true if LOW turns LED ON

// Port 80 for Web Page, Port 81 for WebSocket data
WebServer server(80);
WebSocketsServer webSocket(81);

const int NUM_SENSORS = 15;

// LED Blink Variables
unsigned long previousMillis = 0;
const long interval = 500;       
int ledState = LOW;

const char htmlPage[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no"/>
  <title>QTR + PID Sync</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; 
           background: #f0f2f5; color: #333; margin: 0; padding: 10px; }
    h2 { margin-top: 20px; font-size: 1.2rem; color: #444; border-bottom: 2px solid #ddd; padding-bottom: 5px; }
    #bars { display: flex; justify-content: center; gap: 2px; margin-bottom: 20px; overflow-x: auto; }
    .sensor-box { min-width: 20px; height: 80px; flex-grow: 1; border: 1px solid #999; 
                  text-align: center; line-height: 80px; font-size: 10px; background: #fff; color: #000; }
    .slider-card { background: white; padding: 15px; margin-bottom: 10px; border-radius: 8px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }
    .slider-header { font-weight: bold; margin-bottom: 10px; font-size: 1rem; display: flex; justify-content: space-between; }
    .val-display { color: #007bff; font-family: monospace; font-size: 1.1rem; }
    .slider-row { display: flex; align-items: center; gap: 10px; }
    .btn-adj { width: 44px; height: 44px; font-size: 24px; border: none; border-radius: 8px; 
               background: #e4e6eb; color: #000; cursor: pointer; display: flex; align-items: center; justify-content: center; }
    .btn-adj:active { background: #d8dadf; transform: scale(0.95); }
    input[type=range] { -webkit-appearance: none; width: 100%; background: transparent; flex-grow: 1; height: 30px; }
    input[type=range]::-webkit-slider-thumb { -webkit-appearance: none; height: 28px; width: 28px; border-radius: 50%; 
                                              background: #007bff; margin-top: -12px; box-shadow: 0 1px 3px rgba(0,0,0,0.3); }
    input[type=range]::-webkit-slider-runnable-track { width: 100%; height: 4px; background: #ddd; border-radius: 2px; }
    
    /* Fixed Pre Style for Multi-line text */
    pre { background: #333; color: #0f0; padding: 15px; border-radius: 8px; overflow-x: auto; font-size: 14px; line-height: 1.5; }
    #pids { background: #fff; color: #333; border: 1px solid #ccc; font-weight: bold; }
  </style>
</head>
<body>

  <h2>QTR Sensor Live</h2>
  <div id="bars"></div>

  <h2>PID Controls</h2>
  <div id="sliders"></div>

  <h2>Current Settings</h2>
  <pre id="pids">Kp: 10.10
Ki: 0.00
Kd: 0.00</pre>

  <h2>Motor Telemetry</h2>
  <pre id="telemetry">M1: ...
M2: ...</pre>

<script>
  const N = 15;
  let ws;
  let ignoreUpdate = false; 
  
  // --- VARIABLES FOR THROTTLING ---
  let lastSendTime = 0; // When did we last speak to the robot?
  let sendTimer = null; // To track pending updates

  function updatePidBox() {
    let kpc = parseFloat(document.getElementById('kp_c').value);
    let kpf = parseFloat(document.getElementById('kp_f').value);
    let kic = parseFloat(document.getElementById('ki_c').value);
    let kif = parseFloat(document.getElementById('ki_f').value);
    let kdc = parseFloat(document.getElementById('kd_c').value);
    let kdf = parseFloat(document.getElementById('kd_f').value);

    let s = `Kp: ${(kpc + kpf).toFixed(2)}\n`;
    s += `Ki: ${(kic + kif).toFixed(2)}\n`;
    s += `Kd: ${(kdc + kdf).toFixed(2)}`;
    document.getElementById('pids').innerText = s;
  }

  function makeSlider(id, min, max, step, init, name) {
    let card = document.createElement('div'); card.className = 'slider-card';
    let header = document.createElement('div'); header.className = 'slider-header';
    let label = document.createElement('span'); label.innerText = name;
    let valDisplay = document.createElement('span'); valDisplay.className = 'val-display'; valDisplay.innerText = init;
    header.appendChild(label); header.appendChild(valDisplay);

    let row = document.createElement('div'); row.className = 'slider-row';
    let sld = document.createElement('input');
    sld.type='range'; sld.id=id; sld.min=min; sld.max=max; sld.step=step; sld.value=init;
    
    sld.updateLabel = function() {
        let decimals = (step.includes('.')) ? step.split('.')[1].length : 0;
        valDisplay.innerText = parseFloat(sld.value).toFixed(decimals);
    };

    let btnMinus = document.createElement('button'); btnMinus.className = 'btn-adj'; btnMinus.innerText = '-';
    btnMinus.onclick = () => { sld.stepDown(); sld.updateLabel(); sendPID(); updatePidBox(); };

    let btnPlus = document.createElement('button'); btnPlus.className = 'btn-adj'; btnPlus.innerText = '+';
    btnPlus.onclick = () => { sld.stepUp(); sld.updateLabel(); sendPID(); updatePidBox(); };

    sld.oninput = () => { sld.updateLabel(); sendPID(); updatePidBox(); };

    row.appendChild(btnMinus); row.appendChild(sld); row.appendChild(btnPlus);
    card.appendChild(header); card.appendChild(row);
    return card;
  }

  // --- NEW: SMART THROTTLED SENDING ---
  function sendPID() {
    if (ignoreUpdate) return;

    const now = Date.now();
    // If it has been more than 50ms since last send, SEND NOW.
    if (now - lastSendTime > 50) {
      performSend();
      lastSendTime = now;
    } else {
      // If we are sending too fast, wait!
      // But ensure we send the FINAL value once the user stops sliding.
      clearTimeout(sendTimer);
      sendTimer = setTimeout(() => {
        performSend();
        lastSendTime = Date.now();
      }, 50);
    }
  }

  // The actual sending logic (separated so we can delay it)
  function performSend() {
    let kp = `${document.getElementById('kp_c').value},${document.getElementById('kp_f').value}`;
    let ki = `${document.getElementById('ki_c').value},${document.getElementById('ki_f').value}`;
    let kd = `${document.getElementById('kd_c').value},${document.getElementById('kd_f').value}`;
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(`kp:${kp}`); ws.send(`ki:${ki}`); ws.send(`kd:${kd}`);
    }
  }

  function syncSlider(id, value) {
    let sld = document.getElementById(id);
    if(sld) {
        sld.value = value;
        if(sld.updateLabel) sld.updateLabel(); 
    }
  }

  window.onload = () => {
    let barContainer = document.getElementById('bars');
    for(let i=0; i<N; i++){
      let d=document.createElement('div'); d.id='box'+i; d.className='sensor-box'; d.textContent='0';
      barContainer.appendChild(d);
    }

    let sv=document.getElementById('sliders');
    sv.appendChild(makeSlider('kp_c','0','100','1','10','Kp (Coarse)'));
    sv.appendChild(makeSlider('kp_f','0','0.99','0.01','0.1','Kp (Fine)'));
    sv.appendChild(makeSlider('ki_c','0','100','1','0','Ki (Coarse)'));
    sv.appendChild(makeSlider('ki_f','0','0.99','0.01','0','Ki (Fine)'));
    sv.appendChild(makeSlider('kd_c','0','100','1','0','Kd (Coarse)'));
    sv.appendChild(makeSlider('kd_f','0','0.99','0.01','0','Kd (Fine)'));
    
    updatePidBox();

    ws = new WebSocket(`ws://${location.hostname}:81/`);
    ws.onopen = () => console.log('WS Connected');
    
    ws.onmessage = e => {
      let msg = e.data.trim();

      if(msg.startsWith("kp:") || msg.startsWith("ki:") || msg.startsWith("kd:")) {
          ignoreUpdate = true; 
          let type = msg.split(':')[0]; 
          let vals = msg.split(':')[1].split(','); 
          syncSlider(type + '_c', vals[0]);
          syncSlider(type + '_f', vals[1]);
          updatePidBox();
          ignoreUpdate = false;
      }
      else if(msg.indexOf('\t') !== -1) {
          let parts = msg.split('\t');
          if(parts.length >= N){
            parts.forEach((v,i)=>{
              if(i < N) {
                  let val = parseInt(v)||0;
                  let colorVal = 255 - Math.min(Math.max(val, 0), 1000) * 0.255; 
                  let b = document.getElementById('box'+i);
                  b.style.backgroundColor = `rgb(${colorVal},${colorVal},${colorVal})`;
                  b.style.color = (colorVal < 128) ? 'white' : 'black';
                  b.textContent = val;
              }
            });
          }
      }
      else if(msg.startsWith("M1:") || msg.startsWith("M2:")){
        let t = document.getElementById('telemetry');
        let lines = t.innerText.split('\n');
        if(msg.startsWith('M1:')) lines[0] = msg; 
        else if(msg.startsWith('M2:')) lines[1] = msg;
        t.innerText = lines.join('\n');
      }
    };
  };
</script>
</body>
</html>
)rawliteral";

void handleRoot(){ server.send(200, "text/html", htmlPage); }

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length){
  if(type == WStype_TEXT){
    size_t len = length < 64 ? length : 63;
    char buf[64];
    memcpy(buf, payload, len);
    buf[len] = '\0';
    Serial.println(buf);
    webSocket.broadcastTXT(payload); // Echo sync to other devices
  }
}

void setup(){
  Serial.begin(460800); 
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, ledActiveLow ? HIGH : LOW);

  WiFi.softAP(ssid, password);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.println("\n\n--- ESP32-C3 SYNC READY ---");
  Serial.print("   SSID: "); Serial.println(ssid);
  Serial.print("   URL:  http://"); Serial.println(IP);
  Serial.println("---------------------------\n");

  server.on("/", handleRoot);
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop(){
  server.handleClient();
  webSocket.loop();

  int stations = WiFi.softAPgetStationNum();
  if (stations > 0) {
    digitalWrite(LED_PIN, ledActiveLow ? LOW : HIGH); 
  } else {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      ledState = (ledState == LOW) ? HIGH : LOW;
      digitalWrite(LED_PIN, ledState);
    }
  }

  while(Serial.available()){
    String line = Serial.readStringUntil('\n');
    if(line.length() > 0) webSocket.broadcastTXT(line);
  }
}