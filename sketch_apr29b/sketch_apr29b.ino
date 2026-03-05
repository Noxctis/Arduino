#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>

const char* ssid     = "QTR_Live";
const char* password = "12345678";

ESP8266WebServer server(80);
WebSocketsServer  webSocket(81);

const int NUM_SENSORS = 15;

const char htmlPage[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head><meta name="viewport" content="width=device-width,initial-scale=1"/>
  <title>QTR + Dual Motor Telemetry</title>
  <style>
    .sensor-box {
      width:30px; height:100px;
      margin:2px; display:inline-block;
      border:1px solid #333;
      text-align:center; line-height:100px; font-size:12px;
      background:#fff; color:#000;
    }
    pre { background:#eee; padding:8px; white-space:pre-wrap; }
  </style>
</head>
<body>
  <h2>QTR Sensor Live Data</h2>
  <div id="bars"></div>
  <h2>Motor Telemetry</h2>
  <pre id="telemetry">Waiting for data…</pre>
<script>
  const N = 15;
  const bars = document.getElementById('bars');
  for (let i = 0; i < N; i++) {
    const d = document.createElement('div');
    d.id = 'box' + i;
    d.className = 'sensor-box';
    d.textContent = '0';
    bars.appendChild(d);
  }
  const tel = document.getElementById('telemetry');
  let lastM1 = 'M1: no data';
  let lastM2 = 'M2: no data';
  const ws = new WebSocket('ws://' + location.hostname + ':81/');
  ws.onopen = () => console.log('WebSocket connected');
  ws.onmessage = e => {
    const msg = e.data.trim();
    const parts = msg.split('\t');
    if (parts.length === N) {
      // sensor update
      parts.forEach((v,i) => {
        const val = parseInt(v) || 0;
        const gray = 255 - Math.min(val,1000)/1000*255;
        const box = document.getElementById('box' + i);
        box.style.background = `rgb(${gray},${gray},${gray})`;
        box.textContent = val;
      });
    } else if (msg.startsWith('M1:')) {
      lastM1 = msg;
    } else if (msg.startsWith('M2:')) {
      lastM2 = msg;
    }
    // always display both
    tel.textContent = lastM1 + '\n' + lastM2;
  };
  ws.onerror = e => console.error('WebSocket error', e);
  ws.onclose = () => console.log('WebSocket closed');
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t*, size_t) {
  if (type == WStype_CONNECTED) {
    Serial.printf("WS client #%u connected\n", num);
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.softAP(ssid, password);
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, handleRoot);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  server.handleClient();
  webSocket.loop();

  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    webSocket.broadcastTXT(line);
  }
}
