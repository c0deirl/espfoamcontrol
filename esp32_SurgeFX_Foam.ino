#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>       // <-- Added for Websockets
#include <TFT_eSPI.h>             // ILI9341 display driver
#include <XPT2046_Touchscreen.h>  // Touch controller
#include "Free_Fonts.h" // Include the header file attached to this sketch
#include "GlitchGoblin_2O87v20pt7b.h"

// Pin definitions — adjust as necessary for your wiring
#define MOTOR_PWM_PIN 35
#define MOTOR_IN1_PIN 16
#define MOTOR_IN2_PIN 17
//#define BLOWER_PIN    17

#define MYFONT32 &GlitchGoblin_2O87v20pt7b

// Touchscreen pins
#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS

#define TOUCH_CS     33
#define TOUCH_IRQ    36

const char* ssid = "ESP32_MotorControl";
const char* password = "12345678";

SPIClass touchscreenSPI = SPIClass(VSPI);
TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen touchscreen(TOUCH_CS, TOUCH_IRQ);
int x, y, z;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); // <-- Websocket endpoint

// Colors (565 format)
#define COLOR_BG          0x0000  // Dark gray blue (#313d54 approx)
#define COLOR_ACCENT      0x4EDB     // Cyan  (#00ffff)
#define COLOR_BTN_OFF     0x10a2     // Gray (#7f8c8d)
#define COLOR_TEXT        0xFFFF     // White
#define COLOR_TEXT_ACC    COLOR_ACCENT
#define COLOR_BLACK       0x0000
#define COLOR_RED         0x8000
#define COLOR_GREEN       0x03e0

// Layout constants matching your image
#define BTN_RADIUS         30

#define BTN_POWER_X        40
#define BTN_POWER_Y        95

#define TIME_X             160
#define TIME_Y             60

#define TITLE_X            165
#define TITLE_Y            25

#define SPEED_BOX_X        100
#define SPEED_BOX_Y        80
#define SPEED_BOX_W        120
#define SPEED_BOX_H        60

#define SLIDER_X           40
#define SLIDER_Y           150
#define SLIDER_W           250
#define SLIDER_H           30

#define BTN_BLWR_X         40
#define BTN_FOAM_X         40
#define BTN_BLKL_X         220
#define BTN_BTNS_Y         200
#define BTN_BTN_W          70
#define BTN_BTN_H          50
#define BTN_BTN_RADIUS     10

// Device states
bool motorOn = false;
uint8_t motorSpeed = 0;  // 0-100%
//bool blowerOn = true;
bool foamMachineOn = false;
bool blackLightsOn = false;

IPAddress AP_IP;

// --- WebSocket event handler ---
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WebSocket client #%u connected\n", client->id());
    // Optionally send initial state
    String json = "{";
    json += "\"motorOn\":" + String(motorOn ? "true" : "false") + ",";
    json += "\"motorSpeed\":" + String(motorSpeed) + ",";
    json += "\"foamMachineOn\":" + String(foamMachineOn ? "true" : "false") + ",";
    json += "\"blackLightsOn\":" + String(blackLightsOn ? "true" : "false");
    json += "}";
    client->text(json);
  }
  // You can handle received data here if you want two-way sync from web
  // (e.g., if user changes from web, but you already have HTTP handlers for that)
}

// --- Notify all websocket clients of current state ---
void notifyClients() {
  String json = "{";
  json += "\"motorOn\":" + String(motorOn ? "true" : "false") + ",";
  json += "\"motorSpeed\":" + String(motorSpeed) + ",";
  json += "\"foamMachineOn\":" + String(foamMachineOn ? "true" : "false") + ",";
  json += "\"blackLightsOn\":" + String(blackLightsOn ? "true" : "false");
  json += "}";
  ws.textAll(json);
}

void setup() {
  Serial.begin(115200);

  // Setup motor control pins and PWM channel 0
  pinMode(MOTOR_IN1_PIN, OUTPUT);
  pinMode(MOTOR_IN2_PIN, OUTPUT);
  ledcSetup(0, 20000, 8);  // 20 kHz, 8-bit
  ledcAttachPin(MOTOR_PWM_PIN, 0);
  pinMode(TOUCH_IRQ, INPUT_PULLUP);

  // Blower pin setup
  //pinMode(BLOWER_PIN, OUTPUT);
 // digitalWrite(BLOWER_PIN, blowerOn ? HIGH : LOW);

  // Initialize display and touchscreen
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(COLOR_BG);

 // touch.begin();
 touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
 touchscreen.begin(touchscreenSPI);
 // touch.setRotation(1);
 // Set the Touchscreen rotation in landscape mode
  // Note: in some displays, the touchscreen might be upside down, so you might need to set the rotation to 3: touchscreen.setRotation(3);
  touchscreen.setRotation(1);

  // Start WiFi access point
  WiFi.softAP(ssid, password);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  IPAddress IP = WiFi.softAPIP(); 
  drawGUI();

  // Define web server routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    // Simple placeholder web GUI; please expand as needed
    request->send(200, "text/html", R"rawliteral(
      <!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>ESP32 Motor Control</title>
  <style>
    body {
  margin: 0; padding: 20px;
  background: #2f3b4d;
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Oxygen,
               Ubuntu, Cantarell, "Open Sans", "Helvetica Neue", sans-serif;
  color: #ccd6f6;
  display: flex;
  justify-content: center;
  align-items: center;
  height: 100vh;
}
    .card {
      background: #38435b;
      border-radius: 16px;
      box-shadow: 0 8px 16px rgba(0,0,0,0.5);
      padding: 24px;
      width: 360px;
      box-sizing: border-box;
      display: flex;
      flex-direction: column;
      user-select: none;
    }
    .top-bar {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 12px;
    }
    .time {
      font-weight: 500;
      font-size: 14px;
      color: #8a94a6;
    }
    .settings-icon {
      width: 24px; height: 24px;
      fill: #8a94a6;
      cursor: pointer;
    }
    h1 {
      font-size: 22px;
      margin: 8px 0 16px 0;
      text-align: center;
      font-weight: 700;
      color: #e6f0ff;
    }
    .speed-display {
      background: #000;
      border-radius: 8px;
      padding: 16px 0;
      width: 120px;
      margin: 0 auto 24px auto;
      box-shadow: inset 0 -4px 6px rgba(0,0,0,0.7);
      font-size: 48px;
      font-weight: 700;
      color: #00ffff;
      text-align: center;
      font-variant-numeric: tabular-nums;
    }
    input[type=range] {
      -webkit-appearance: none;
      width: 100%;
      height: 12px;
      background: #19222e;
      border-radius: 6px;
      outline: none;
      margin-bottom: 24px;
      cursor: pointer;
    }
    input[type=range]::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 28px;
      height: 28px;
      background: #00ffff;
      border-radius: 50%;
      cursor: pointer;
      box-shadow: 0 0 8px #00ffffcc;
      transition: background 0.3s ease;
      margin-top: -8px; /* center thumb vertically */
      border: none;
    }
    input[type=range]:active::-webkit-slider-thumb {
      background: #00cccc;
      box-shadow: 0 0 12px #00cccccc;
    }
    input[type=range]::-moz-range-thumb {
      width: 28px;
      height: 28px;
      background: #00ffff;
      border-radius: 50%;
      cursor: pointer;
      border: none;
      box-shadow: 0 0 8px #00ffffcc;
      transition: background 0.3s ease;
    }
    input[type=range]:active::-moz-range-thumb {
      background: #00cccc;
      box-shadow: 0 0 12px #00cccccc;
    }
    .buttons-row {
      display: flex;
      justify-content: space-between;
    }
    .btn {
      flex-grow: 1;
      margin: 0 6px;
      padding: 12px 0;
      border-radius: 12px;
      font-weight: 600;
      font-size: 16px;
      cursor: pointer;
      color: #000;
      display: flex;
      flex-direction: column;
      align-items: center;
      user-select: none;
      box-shadow: 0 4px 8px rgba(0,0,0,0.3);
      transition: background-color 0.3s ease;
      border: none;
      outline: none;
    }
    .btn svg {
      width: 20px;
      height: 20px;
      margin-bottom: 6px;
      stroke-width: 2;
      stroke: currentColor;
      fill: none;
    }
    .btn.on {
      background-color: #00ffff;
      color: #000;
    }
    .btn.off {
      background-color: #7f8c8d;
      color: #ddd;
    }
    .btn:first-child {
      margin-left: 0;
    }
    .btn:last-child {
      margin-right: 0;
    }
  </style>
</head>
<body>
  <div class="card" role="main" aria-label="Pump control panel">
    <div class="top-bar">
      <button id="powerBtn" class="btn on" aria-pressed="true" aria-label="Motor power toggle" title="Power">
        <svg viewBox="0 0 24 24" aria-hidden="true"><circle cx="12" cy="12" r="9"/><line x1="12" y1="5" x2="12" y2="12"/></svg>
      </button>
    </div>
    <h1>SurgeFX Pump Speed</h1>
    <div class="speed-display" id="speedDisplay">0%</div>
    <input type="range" id="speedSlider" min="0" max="100" value="0" step="1" aria-valuemin="0" aria-valuemax="100" aria-valuenow="0" aria-label="Pump speed control" />
    <div class="buttons-row" role="group" aria-label="Control buttons">
           
      <button id="foamBtn" class="btn on" aria-pressed="true" aria-label="Foam toggle">
        <svg viewBox="0 0 24 24" aria-hidden="true"><circle cx="12" cy="12" r="9"/><line x1="12" y1="5" x2="12" y2="12"/></svg>
        Foam
      </button>
      
      <button id="blackBtn" class="btn on" aria-pressed="true" aria-label="Black lights toggle">
        <svg viewBox="0 0 24 24" aria-hidden="true"><circle cx="12" cy="12" r="9"/><line x1="12" y1="5" x2="12" y2="12"/></svg>
        Lights
      </button>
    </div>
  </div>

  <script>
    const speedDisplay = document.getElementById('speedDisplay');
    const speedSlider = document.getElementById('speedSlider');
    const powerBtn = document.getElementById('powerBtn');
    const blowerBtn = document.getElementById('blowerBtn');
    const foamBtn = document.getElementById('foamBtn');
    const blackBtn = document.getElementById('blackBtn');

    // Initial states
    let motorOn = false;
    let blowerOn = false;
    let foamOn = false;
    let blackOn = false;

    // --- WebSocket live sync ---
    const ws = new WebSocket(`ws://${window.location.hostname}/ws`);
    ws.onmessage = function(event) {
      const data = JSON.parse(event.data);
      speedDisplay.textContent = `${data.motorSpeed}%`;
      speedSlider.value = data.motorSpeed;
      toggleButton(powerBtn, data.motorOn);
      // toggleButton(blowerBtn, data.blowerOn); // Only if you support blower sync
      toggleButton(foamBtn, data.foamMachineOn);
      toggleButton(blackBtn, data.blackLightsOn);
    };

    function updateSpeed(val) {
      speedDisplay.textContent = `${val}%`;
      speedSlider.setAttribute('aria-valuenow', val);
      fetch(`/setSpeed?val=${val}`).catch(() => {});
    }

    function toggleButton(button, flag) {
      if (flag) {
        button.classList.add('on');
        button.classList.remove('off');
        button.setAttribute('aria-pressed', 'true');
      } else {
        button.classList.add('off');
        button.classList.remove('on');
        button.setAttribute('aria-pressed', 'false');
      }
    }

    speedSlider.addEventListener('input', (e) => {
      updateSpeed(e.target.value);
    });

    powerBtn.addEventListener('click', () => {
      motorOn = !motorOn;
      toggleButton(powerBtn, motorOn);
      fetch(`/setMotor?state=${motorOn ? 'on' : 'off'}`).catch(() => {});
    });

    blowerBtn.addEventListener('click', () => {
      blowerOn = !blowerOn;
      toggleButton(blowerBtn, blowerOn);
      fetch(`/setBlower?state=${blowerOn ? 'on' : 'off'}`).catch(() => {});
    });

    foamBtn.addEventListener('click', () => {
      foamOn = !foamOn;
      toggleButton(foamBtn, foamOn);
      fetch(`/setFoam?state=${foamOn ? 'on' : 'off'}`).catch(() => {});
    });

    blackBtn.addEventListener('click', () => {
      blackOn = !blackOn;
      toggleButton(blackBtn, blackOn);
      fetch(`/setBlackLights?state=${blackOn ? 'on' : 'off'}`).catch(() => {});
    });
  </script>
</body>
</html>
      
      )rawliteral");
  });

  // API routes
  server.on("/setSpeed", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("val")) {
      motorSpeed = constrain(request->getParam("val")->value().toInt(), 0, 100);
      updateMotor();
      drawSpeedValue(motorSpeed);
      drawSlider(motorSpeed);
      notifyClients(); // <-- Websocket broadcast
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/setMotor", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("state")) {
      motorOn = (request->getParam("state")->value() == "on");
      updateMotor();
      drawPowerButton(motorOn);
      notifyClients(); // <-- Websocket broadcast
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/setFoam", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("state")) {
      foamMachineOn = (request->getParam("state")->value() == "on");
      drawBottomButton(BTN_FOAM_X, foamMachineOn, "");
      notifyClients(); // <-- Websocket broadcast
    }
    request->send(200, "text/plain", "OK");
  });
  server.on("/setBlackLights", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("state")) {
      blackLightsOn = (request->getParam("state")->value() == "on");
      drawBottomButton(BTN_BLKL_X, blackLightsOn, "");
      notifyClients(); // <-- Websocket broadcast
    }
    request->send(200, "text/plain", "OK");
  });

  // WebSocket endpoint
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.begin();
  Serial.println("HTTP server ready");
}


unsigned long lastTouchTime = 0;

void loop() {

  if (touchscreen.tirqTouched() && touchscreen.touched()) {

    if (touchscreen.touched()) {
      if (millis() - lastTouchTime < 200) return; // 200ms debounce
      lastTouchTime = millis();

      TS_Point p = touchscreen.getPoint();

      int x = map(p.x, 200, 3700, 1, tft.width() - 1);
      int y = map(p.y, 240, 3800, 1, tft.height() - 1);

      Serial.printf("Touch %d,%d\n", x, y);

      handleTouch(x, y);
      delay(200);
    }
  }
  ws.cleanupClients(); // <-- Clean up disconnected clients
}

unsigned long lastUpdate = 0;
const unsigned long updateInterval = 100; // ms

void handleTouch(int x, int y) {

  if (millis() - lastUpdate < updateInterval) return;
  lastUpdate = millis();

  // Power button circle
  if (pointInCircle(x, y, BTN_POWER_X, BTN_POWER_Y, BTN_RADIUS)) {
    motorOn = !motorOn;
    updateMotor();
    drawPowerButton(motorOn);
    notifyClients(); // <-- Websocket broadcast
    return;
  }

  // Slider control (horizontal bar)
  if ((y >= SLIDER_Y) && (y <= SLIDER_Y + SLIDER_H) && (x >= SLIDER_X) && (x <= SLIDER_X + SLIDER_W)) {
    motorSpeed = map(x, SLIDER_X, SLIDER_X + SLIDER_W, 0, 100);
    motorSpeed = constrain(motorSpeed, 0, 100);
    updateMotor();
    drawSpeedValue(motorSpeed);
    drawSlider(motorSpeed);
    notifyClients(); // <-- Websocket broadcast
    return;
  }

  // Bottom buttons rects

  if (pointInRect(x, y, BTN_FOAM_X, BTN_BTNS_Y, BTN_BTN_W, BTN_BTN_H)) {
    foamMachineOn = !foamMachineOn;
    drawBottomButton(BTN_FOAM_X, foamMachineOn, "");
    notifyClients(); // <-- Websocket broadcast
    return;
  }
  if (pointInRect(x, y, BTN_BLKL_X, BTN_BTNS_Y, BTN_BTN_W, BTN_BTN_H)) {
    blackLightsOn = !blackLightsOn;
    drawBottomButton(BTN_BLKL_X, blackLightsOn, "");
    notifyClients(); // <-- Websocket broadcast
    return;
  }
}

void drawGUI() {
  tft.fillScreen(COLOR_BG);
  drawPowerButton(motorOn);
  drawTime();
  drawTitle();
  drawSpeedValue(motorSpeed);
  drawSlider(motorSpeed);
  tft.setTextSize(1);
  drawBottomButton(BTN_FOAM_X, foamMachineOn, "");
  drawBottomButton(BTN_BLKL_X, blackLightsOn, "");
}

void drawPowerButton(bool on) {
  uint16_t bg = on ? COLOR_GREEN : COLOR_RED;
  tft.fillCircle(BTN_POWER_X, BTN_POWER_Y, BTN_RADIUS, bg);
  tft.drawCircle(BTN_POWER_X, BTN_POWER_Y, BTN_RADIUS - 3, COLOR_TEXT_ACC);
  tft.drawLine(BTN_POWER_X, BTN_POWER_Y - 15, BTN_POWER_X, BTN_POWER_Y + 5, COLOR_TEXT_ACC);
  tft.drawCircle(BTN_POWER_X, BTN_POWER_Y + 5, 8, COLOR_TEXT_ACC);
}

void drawTime() {
  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(FF17);
  tft.setTextColor(COLOR_BTN_OFF);
  tft.setTextSize(1);
  IPAddress IP = WiFi.softAPIP(); 
  String ipString = IP.toString();
  tft.drawString(ipString, TIME_X, TIME_Y);
}

void drawTitle() {
  tft.setFreeFont(MYFONT32);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  tft.drawString("Pump Speed", TITLE_X, TITLE_Y);
}

void drawSpeedValue(uint8_t speed) {
  // Black rectangle
  tft.fillRoundRect(SPEED_BOX_X, SPEED_BOX_Y, SPEED_BOX_W, SPEED_BOX_H, 10, COLOR_BLACK);
  // Cyan speed text
  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(FF31);
  tft.setTextColor(COLOR_ACCENT);
  tft.setTextSize(1);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", speed);
  tft.drawString(buf, SPEED_BOX_X + SPEED_BOX_W / 2, SPEED_BOX_Y + SPEED_BOX_H / 2);
}

void drawSlider(uint8_t speed) {
  // Slider background track (dark gray)
  tft.fillRoundRect(SLIDER_X, SLIDER_Y, SLIDER_W, SLIDER_H, SLIDER_H / 2, COLOR_BTN_OFF);
  // Filled portion track (cyan)
  int fillWidth = (SLIDER_W * speed) / 100;
  tft.fillRoundRect(SLIDER_X, SLIDER_Y, fillWidth, SLIDER_H, SLIDER_H / 2, COLOR_ACCENT);
  // Circular knob
  int knobX = SLIDER_X + fillWidth;
  knobX = constrain(knobX, SLIDER_X + SLIDER_H / 2, SLIDER_X + SLIDER_W - SLIDER_H / 2);
  tft.fillCircle(knobX, SLIDER_Y + SLIDER_H / 2, SLIDER_H / 2, COLOR_ACCENT);
  tft.fillCircle(knobX, SLIDER_Y + SLIDER_H / 2, SLIDER_H / 2 - 3, COLOR_BG);
}

void drawBottomButton(int x, bool on, const char* label) {
  uint16_t bg = on ? COLOR_ACCENT : COLOR_BTN_OFF;
  uint16_t fg = on ? COLOR_BG : COLOR_TEXT;
  tft.fillRoundRect(x, BTN_BTNS_Y, BTN_BTN_W, BTN_BTN_H, BTN_BTN_RADIUS, bg);

  // Power icon
  int cx = x + BTN_BTN_W / 2;
  int cy = BTN_BTNS_Y + 15;
  tft.drawCircle(cx, cy, 10, fg);
  tft.drawLine(cx, cy - 10, cx, cy + 5, fg);

  // Label
  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(FF31);
  tft.setTextColor(fg);
  tft.setTextSize(1);
  tft.drawString(label, cx, BTN_BTNS_Y + 10);
}

bool pointInCircle(int px, int py, int cx, int cy, int r) {
  int dx = px - cx;
  int dy = py - cy;
  return (dx * dx + dy * dy) <= (r * r);
}

bool pointInRect(int px, int py, int rx, int ry, int w, int h) {
  return (px >= rx && px <= rx + w && py >= ry && py <= ry + h);
}

void updateMotor() {
  Serial.printf("Updating motor: motorOn=%d, speed=%d\n", motorOn, motorSpeed);
  if (motorOn) {
    digitalWrite(MOTOR_IN1_PIN, HIGH);
    digitalWrite(MOTOR_IN2_PIN, LOW);
    ledcWrite(0, map(motorSpeed, 0, 100, 0, 255));
  } else {
    digitalWrite(MOTOR_IN1_PIN, LOW);
    digitalWrite(MOTOR_IN2_PIN, LOW);
    ledcWrite(0, 0);
  }
}
