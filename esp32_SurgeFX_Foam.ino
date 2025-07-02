#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Encoder.h>

// WiFi credentials
const char* ssid = "FoamControl";
const char* password = "password123";

// Motor pins
#define MOTOR_PIN1 26
#define MOTOR_PIN2 27
#define MOTOR_EN   14

// Encoder pins
#define ENCODER_A 32
#define ENCODER_B 33
#define ENCODER_BTN 25

// OLED config
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

// Global objects
AsyncWebServer server(80);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
ESP32Encoder encoder;

// State variables
volatile bool motorRunning = false;
volatile bool toggleRequested = false;
int motorSpeed = 0; // 0-255

// Function declarations
void updateMotor();
void updateDisplay();
void IRAM_ATTR handleEncoderBtn();

// Web page (with Start/Stop buttons)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta charset="UTF-8">
  <title>Foam Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; background: #222; color:#eee; margin:0; padding:0;}
    .container { max-width: 400px; margin: 40px auto; background: #333; border-radius: 8px; box-shadow: 0 2px 8px #111; padding: 24px;}
    h1 { text-align: center; color: #ffa500;}
    .reading { font-size: 1.3em; margin: 1em 0; text-align: center;}
    .form-group { margin: 20px 0; text-align: center;}
    button { padding: 12px 36px; margin: 7px; border: none; border-radius:6px; font-size:1em; cursor:pointer;}
    #startBtn { background: #059669; color:#fff;}
    #stopBtn { background: #e11d48; color:#fff;}
    #startBtn:active { background: #047857;}
    #stopBtn:active { background: #be123c;}
    .status { text-align:center; margin-top:18px; font-size:1.2em;}
    .slider { width: 100%; margin: 20px 0;}
  </style>
</head>
<body>
  <div class="container">
    <h1>Foam Machine Control</h1>
    <div class="form-group">
      <label for="speedSlider">Speed: <span id="speedValue">0</span></label>
      <input type="range" min="0" max="255" value="0" class="slider" id="speedSlider">
    </div>
    <div class="form-group">
      <button id="startBtn" onclick="startMotor()">Start</button>
      <button id="stopBtn" onclick="stopMotor()">Stop</button>
    </div>
    <div class="status">
      Status: <span id="motorStatus">Stopped</span>
    </div>
  </div>
<script>
function startMotor() {
  fetch('/start', {method:'POST'}).then(updateStatus);
}
function stopMotor() {
  fetch('/stop', {method:'POST'}).then(updateStatus);
}
document.getElementById('speedSlider').oninput = function() {
  document.getElementById('speedValue').textContent = this.value;
  fetch('/speed?value=' + this.value, {method:'POST'});
};
function updateStatus() {
  fetch('/status').then(resp => resp.text()).then(txt => {
    document.getElementById('motorStatus').textContent = txt;
  });
}
setInterval(updateStatus, 1000);
window.onload = updateStatus;
</script>
</body>
</html>
)rawliteral";

// Motor control
void updateMotor() {
  if (motorRunning && motorSpeed > 0) {
    digitalWrite(MOTOR_PIN1, LOW);
    digitalWrite(MOTOR_PIN2, HIGH);
    ledcWrite(0, motorSpeed); // PWM
  } else {
    digitalWrite(MOTOR_PIN1, LOW);
    digitalWrite(MOTOR_PIN2, LOW);
    ledcWrite(0, 0);
  }
}

// OLED display: show state
void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(motorRunning ? "Running" : "Stopped");
  display.setTextSize(1);
  display.setCursor(0, 32);
  display.print("Speed: ");
  display.print(motorSpeed);
  display.display();
}

// Rotary encoder push button: toggle state
void IRAM_ATTR handleEncoderBtn() {
  static uint32_t last_interrupt = 0;
  uint32_t now = millis();
  if (now - last_interrupt > 250) { // debounce
    toggleRequested = true;
    last_interrupt = now;
  }
}

void setup() {
  Serial.begin(115200);

  // Motor pins
  pinMode(MOTOR_PIN1, OUTPUT);
  pinMode(MOTOR_PIN2, OUTPUT);
  ledcAttachPin(MOTOR_EN, 0);
  ledcSetup(0, 5000, 8); // channel 0, 5kHz, 8-bit

  // Encoder
  ESP32Encoder::useInternalWeakPullResistors=UP;
  encoder.attachHalfQuad(ENCODER_A, ENCODER_B);
  encoder.setCount(0);

  // Encoder button
  pinMode(ENCODER_BTN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_BTN), handleEncoderBtn, FALLING);

  // OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    while(true);
  }
  display.clearDisplay();
  display.display();

  // WiFi
  WiFi.softAP(ssid, password);
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Web server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
  server.on("/start", HTTP_POST, [](AsyncWebServerRequest *request){
    motorRunning = true;
    updateMotor();
    updateDisplay();
    request->send(200, "text/plain", "Running");
  });
  server.on("/stop", HTTP_POST, [](AsyncWebServerRequest *request){
    motorRunning = false;
    updateMotor();
    updateDisplay();
    request->send(200, "text/plain", "Stopped");
  });
  server.on("/speed", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("value", true)) {
      motorSpeed = constrain(request->getParam("value", true)->value().toInt(), 0, 255);
      encoder.setCount(motorSpeed);
      updateMotor();
      updateDisplay();
    }
    request->send(200, "text/plain", String(motorSpeed));
  });
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", motorRunning ? "Running" : "Stopped");
  });
  server.begin();

  updateMotor();
  updateDisplay();
}

void loop() {
  // Encoder for speed
  static int32_t lastCount = 0;
  int32_t count = encoder.getCount();
  if (count != lastCount) {
    motorSpeed = constrain(count, 0, 255);
    encoder.setCount(motorSpeed); // Prevent rollover
    lastCount = motorSpeed;
    updateMotor();
    updateDisplay();
  }

  // Encoder push button toggles start/stop
  if (toggleRequested) {
    motorRunning = !motorRunning;
    updateMotor();
    updateDisplay();
    toggleRequested = false;
  }

  delay(10);
}
