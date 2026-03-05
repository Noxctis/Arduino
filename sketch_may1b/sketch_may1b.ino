#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>

// Wi-Fi AP credentials
const char* ssid     = "QTR_Live";
const char* password = "12345678";

ESP8266WebServer server(80);
WebSocketsServer  webSocket(81);

const int NUM_SENSORS = 15;

// Full HTML + JS with extended PID guidance
const char htmlPage[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head><meta name="viewport" content="width=device-width,initial-scale=1"/>
  <title>QTR + PID Tuning</title>
  <style>
    .sensor-box { width:30px; height:100px; margin:2px; display:inline-block;
      border:1px solid #333; text-align:center; line-height:100px;
      font-size:12px; background:#fff; color:#000; }
    .slider-group { margin:8px 0; }
    label { display:block; font-size:14px; }
    pre { background:#eee; padding:8px; white-space:pre-wrap; }
  </style>
</head>
<body>
  <h2>QTR Sensor Live Data</h2>
  <div id="bars"></div>

  <h2>PID Tuning</h2>

  <p><strong>1. Tuning Process</strong><br>
  A common approach is to:<br>
  • Set <code>Ki</code> to 0 and tune the proportional and derivative gains (<code>Kp</code> and <code>Kd</code>) first.<br>
  • Once the robot performs well with just P and D, start increasing <code>Ki</code> to see if it improves performance.<br>
  • Adjust <code>Ki</code> until you find the value that provides the desired level of accuracy and stability.</p>

  <p><strong>2. Tuning Order</strong><br>
  Start with only <code>Kp</code> nonzero (set <code>Ki=0</code> and <code>Kd=0</code>), then:<br>
  • Adjust <code>Kp</code> first until you get firm line tracking without sustained oscillation.<br>
  • Next, introduce <code>Ki</code> to eliminate any slow drift away from the line center.<br>
  • Finally, add <code>Kd</code> to smooth out your corrections through turns.</p>

  <p><strong>3. K<sub>p</sub> (Proportional)</strong><br>
  • Controls reaction to the current error (distance from line center).<br>
  • <em>If Kp too low</em>: robot corrects too gently, may cut corners or drift wide on turns.<br>
  • <em>If Kp too high</em>: robot “hugs” the line but oscillates rapidly left/right.<br>
  🛠 <em>Tip:</em> Increase Kp until you see tight cornering in turns without bouncing. If you overshoot and oscillate, dial Kp down slightly.</p>

  <p><strong>4. K<sub>i</sub> (Integral)</strong><br>
  • Corrects for accumulated error (persistent biases).<br>
  • <em>If Ki too low</em>: small constant drift—robot regularly finishes turns slightly off track.<br>
  • <em>If Ki too high</em>: slow “hunting” oscillations around the center.<br>
  🛠 <em>Tip:</em> With Kp working well, ramp Ki up until drift disappears. If you see slow waves, reduce Ki a bit.</p>

  <p><strong>5. K<sub>d</sub> (Derivative)</strong><br>
  • Dampens response based on the error’s rate of change.<br>
  • <em>If Kd too low</em>: corners still overshoot, robot can feel “sloppy.”<br>
  • <em>If Kd too high</em>: sensor noise causes jittery, choppy corrections.<br>
  🛠 <em>Tip:</em> Slowly add Kd to reduce overshoot in sharp turns. Stop increasing once you see the robot “twitch” on straight lines.</p>

  <p><strong>6. Handling Turns & Broken Lines</strong><br>
  • **Sharp turns:** Increase Kp until the robot corners tightly, then add Kd to prevent overshoot.<br>
  • **Wide curves:** Too much Kp can make the robot twitch; balance with a bit more Kd.<br>
  • **Broken/dashed lines:** When the line disappears, your code should “coast” forward using the last error and encoder count—if you drift too far (e.g. > 40 mm), pause and search.<br>
  🛠 <em>Tip:</em> If you see the robot veer off during a gap, lower Ki (less slow drift) and increase Kd (more predictive hold).</p>

  <p><strong>7. Interplay Between Gains</strong><br>
  • Raising Kp may require a small increase in Kd to tame resulting oscillations.<br>
  • If Kd is high and you add Ki, you may need to lower Kd slightly to avoid choppy noise amplification.<br>
  • Always change one parameter at a time, observe behavior over several seconds, then tweak the next.</p>

  <div id="sliders"></div>

  <h2>PID Parameters</h2>
  <pre id="pids">
Kp: not set
Ki: not set
Kd: not set
  </pre>

  <h2>Motor Telemetry</h2>
  <pre id="telemetry">Waiting for data…</pre>

<script>
  const N = 15;
  let ws, lastKp="Kp: not set", lastKi="Ki: not set", lastKd="Kd: not set";

  function makeSlider(id, min, max, step, init, name) {
    let grp = document.createElement('div'); grp.className='slider-group';
    let lbl = document.createElement('label'); lbl.htmlFor=id; lbl.innerText=`${name}: ${init}`;
    let sld = document.createElement('input');
    sld.type='range'; sld.id=id; sld.min=min; sld.max=max; sld.step=step; sld.value=init;
    sld.oninput = () => { lbl.innerText=`${name}: ${sld.value}`; sendPID(); };
    grp.appendChild(lbl); grp.appendChild(sld);
    return grp;
  }

  function sendPID(){
    let kp = `${document.getElementById('kp_c').value},${document.getElementById('kp_f').value}`;
    let ki = `${document.getElementById('ki_c').value},${document.getElementById('ki_f').value}`;
    let kd = `${document.getElementById('kd_c').value},${document.getElementById('kd_f').value}`;
    ws.send(`kp:${kp}`); ws.send(`ki:${ki}`); ws.send(`kd:${kd}`);
  }

  window.onload = () => {
    // Sensor bars
    for(let i=0;i<N;i++){
      let d = document.createElement('div');
      d.id='box'+i; d.className='sensor-box'; d.textContent='0';
      document.getElementById('bars').appendChild(d);
    }
    // Sliders
    let sv = document.getElementById('sliders');
    sv.appendChild(makeSlider('kp_c','0','100','1','10','Kp coarse'));
    sv.appendChild(makeSlider('kp_f','0','0.99','0.01','0.1','Kp fine'));
    sv.appendChild(makeSlider('ki_c','0','100','1','0','Ki coarse'));
    sv.appendChild(makeSlider('ki_f','0','0.99','0.01','0','Ki fine'));
    sv.appendChild(makeSlider('kd_c','0','100','1','0','Kd coarse'));
    sv.appendChild(makeSlider('kd_f','0','0.99','0.01','0','Kd fine'));

    // WebSocket setup
    ws = new WebSocket(`ws://${location.hostname}:81/`);
    ws.onopen = () => console.log('WS connected');
    ws.onmessage = e => {
      let msg = e.data.trim(), parts = msg.split('\t');
      if(parts.length===N){
        parts.forEach((v,i)=>{
          let val = parseInt(v)||0;
          let gray = 255-Math.min(val,1000)/1000*255;
          let b = document.getElementById('box'+i);
          b.style.background = `rgb(${gray},${gray},${gray})`;
          b.textContent = val;
        });
      }
      else if(msg.startsWith("Kp updated")) lastKp = msg;
      else if(msg.startsWith("Ki updated")) lastKi = msg;
      else if(msg.startsWith("Kd updated")) lastKd = msg;
      else if(msg.startsWith("M1:")||msg.startsWith("M2:")){
        let t = document.getElementById('telemetry');
        let lines = t.textContent.split('\n');
        if(msg.startsWith('M1:')) lines[0]=msg;
        else lines[1]=msg;
        t.textContent = lines.join('\n');
      }
      document.getElementById('pids').textContent = `${lastKp}\n${lastKi}\n${lastKd}`;
    };
    ws.onerror = e => console.error('WS error',e);
    ws.onclose = () => console.log('WS closed');
  };
</script>
</body>
</html>
)rawliteral";

void handleRoot(){
  server.send(200, "text/html", htmlPage);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length){
  if(type == WStype_TEXT){
    size_t len = length < 64 ? length : 63;
    char buf[64];
    memcpy(buf, payload, len);
    buf[len] = '\0';
    Serial.println(buf);
  }
}

void setup(){
  Serial.begin(115200);
  WiFi.softAP(ssid, password);
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, handleRoot);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop(){
  server.handleClient();
  webSocket.loop();

  if(Serial.available()){
    String line = Serial.readStringUntil('\n');
    webSocket.broadcastTXT(line);
  }
}
