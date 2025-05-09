#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Encoder.h> // Include the Encoder library for rotary encoder support

const char* ssid = "Your_SSID";
const char* password = "Your_Password";

int ENA_pin = 13;
int IN1 = 5;
int IN2 = 4;
volatile int slider_value = 0; // Use `volatile` for variables modified in interrupts

const int frequency = 500;
const int pwm_channel = 0;
const int resolution = 8;

const char* input_parameter = "value";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); // WebSocket endpoint

// Define pins for the rotary encoder
#define ENCODER_PIN_A 12
#define ENCODER_PIN_B 14

// Create an Encoder object
Encoder myEnc(ENCODER_PIN_A, ENCODER_PIN_B);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>DC Motor Speed Control Web Server</title>
  <style>
    html {font-family: Times New Roman; display: inline-block; text-align: center;}
    h2 {font-size: 2.3rem;}
    p {font-size: 2.0rem;}
    body {max-width: 400px; margin:0px auto; padding-bottom: 25px;}
    .slider { -webkit-appearance: none; margin: 14px; width: 360px; height: 25px; background: #4000ff;
      outline: none; -webkit-transition: .2s; transition: opacity .2s;}
    .slider::-webkit-slider-thumb {-webkit-appearance: none; appearance: none; width: 35px; height: 35px; background:#01070a; cursor: pointer;}
    .slider::-moz-range-thumb { width: 35px; height: 35px; background: #01070a; cursor: pointer; } 
  </style>
</head>
<body>
  <h2>DC Motor Speed Control Web Server</h2>
  <p><span id="textslider_value">%SLIDERVALUE%</span></p>
  <p><input type="range" oninput="updateSliderPWM(this)" id="pwmSlider" min="0" max="255" value="%SLIDERVALUE%" step="1" class="slider"></p>
<script>
function updateSliderPWM(element) {
  var slider_value = document.getElementById("pwmSlider").value;
  document.getElementById("textslider_value").innerHTML = slider_value;
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/slider?value="+slider_value, true);
  xhr.send();
}

// WebSocket connection
var websocket = new WebSocket("ws://" + window.location.hostname + "/ws");
websocket.onmessage = function(event) {
  var slider_value = event.data;
  document.getElementById("textslider_value").innerHTML = slider_value;
  document.getElementById("pwmSlider").value = slider_value;
};
</script>
</body>
</html>
)rawliteral";

String processor(const String& var) {
  if (var == "SLIDERVALUE") {
    return String(slider_value);
  }
  return String();
}

void notifyClients() {
  ws.textAll(String(slider_value));
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    slider_value = String((char*)data).toInt();
    ledcWrite(pwm_channel, slider_value);
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    notifyClients();
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    handleWebSocketMessage(arg, data, len);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(ENA_pin, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  ledcSetup(pwm_channel, frequency, resolution);
  ledcAttachPin(ENA_pin, pwm_channel);
  ledcWrite(pwm_channel, slider_value);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting...");
  }

  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/slider", HTTP_GET, [](AsyncWebServerRequest *request) {
    String message;
    if (request->hasParam(input_parameter)) {
      message = request->getParam(input_parameter)->value();
      slider_value = message.toInt();
      ledcWrite(pwm_channel, slider_value);
      digitalWrite(IN1, HIGH);
      digitalWrite(IN2, LOW);
      notifyClients();
    } else {
      message = "No message sent";
    }
    Serial.println(message);
    request->send(200, "text/plain", "OK");
  });

  ws.onEvent(onEvent);
  server.addHandler(&ws);

  server.begin();
}

void loop() {
  static int lastValue = 0;
  int newValue = myEnc.read() / 4; // Adjust sensitivity by dividing by 4
  if (newValue != lastValue) {
    lastValue = newValue;
    slider_value = constrain(newValue, 0, 255); // Constrain slider value between 0 and 255
    ledcWrite(pwm_channel, slider_value);
    notifyClients();
    Serial.println(slider_value);
  }
}
