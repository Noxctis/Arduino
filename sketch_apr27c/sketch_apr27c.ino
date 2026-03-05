#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>

// your AP credentials
const char* ssid     = "QTR_Live";
const char* password = "12345678";

ESP8266WebServer server(80);
WebSocketsServer  webSocket(81);

// This is your HTML + JS, served at “/”
const char htmlPage[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <title>QTR Live WebSocket</title>
  <style>
    .sensor-box {
      width: 30px; height: 100px;
      margin: 2px; display: inline-block;
      border: 1px solid #333;
      text-align: center; font-size:12px;
      line-height:100px; color:#000;
      background: #fff;
    }
  </style>
</head>
<body>
  <h2>QTR Sensor Live Data</h2>
  <div id="bars"></div>
  <script>
    const NUM_SENSORS = 15;
    const container = document.getElementById('bars');
    // Create 15 empty boxes
    for (let i = 0; i < NUM_SENSORS; i++) {
      const d = document.createElement('div');
      d.id = 'box' + i;
      d.className = 'sensor-box';
      d.textContent = '0';
      container.appendChild(d);
    }

    // Open WebSocket on port 81
    const ws = new WebSocket('ws://' + location.hostname + ':81/');
    ws.onopen    = () => console.log('WebSocket open');
    ws.onmessage = e => {
      const vals = e.data.trim().split('\t');
      for (let i = 0; i < vals.length && i < NUM_SENSORS; i++) {
        const v = parseInt(vals[i]) || 0;
        const gray = 255 - Math.min(v,1000)/1000*255;
        const box  = document.getElementById('box' + i);
        box.style.background = `rgb(${gray},${gray},${gray})`;
        box.textContent = v;
      }
    };
    ws.onerror = e => console.log('WebSocket error', e);
    ws.onclose = () => console.log('WebSocket closed');
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_CONNECTED) {
    Serial.printf("WS client #%u connected\n", num);
  }
}

void setup() {
  Serial.begin(115200);

  // Start Access Point
  WiFi.softAP(ssid, password);
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());

  // Web server for index page
  server.on("/", HTTP_GET, handleRoot);
  server.begin();

  // WebSocket server on port 81
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  server.handleClient();
  webSocket.loop();

  // Read one line from Teensy → broadcast to all WS clients
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    webSocket.broadcastTXT(line);
  }
}
