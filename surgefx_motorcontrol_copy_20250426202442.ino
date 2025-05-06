#include <WiFi.h>
#include <WebServer.h>
//#include <Encoder.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "AiEsp32RotaryEncoder.h"
#include "Arduino.h"

//Hardware Connections
//OLED Screen:

//Connect SDA to GPIO 21 and SCL to GPIO 22 on the ESP32.
//Connect the VCC and GND pins of the OLED to 3.3V and GND on the ESP32.

// Replace with your network credentials
const char* ssid = "SurgeFX_Foam";
const char* password = "password";

// Motor control pins
const int motorPin1 = 5; // IN1
const int motorPin2 = 4; // IN2
const int enablePin = 16; // PWM

// Rotary encoder pins
const int encoderPinA = 34;
const int encoderPinB = 35;

// OLED display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Initialize rotary encoder
//Encoder myEnc(encoderPinA, encoderPinB);
#define ROTARY_ENCODER_A_PIN 32
#define ROTARY_ENCODER_B_PIN 33
#define ROTARY_ENCODER_BUTTON_PIN 21
#define ROTARY_ENCODER_VCC_PIN -1
#define ROTARY_ENCODER_STEPS 4
AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);

// Web server instance
WebServer server(80);

// Motor speed (0-255)
volatile int motorSpeed = 0;

// HTML GUI page
String htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>SurgeFX Foam Control</title>
  <style>
h2 {text-align: center;
font-size: 42px;}
p {text-align: center;
font-size: 42px;}
div {text-align: center;}
label {text-align: center;}
</style>
</head>
<body>
  <h2 style="font-size:8vw;">SurgeFX Foam Control</h2>
  <p>
  <label style="font-size:5vw;" for="speed">Amount <br><br></label>
  <input type="range" style="width: 600px;height:200px;" id="speed" min="0" max="255" value="0" oninput="updateSpeed(this.value)">
  </p>
  <p>Current Amount: <span id="currentSpeed">0</span></p>
  <script>
    function updateSpeed(value) {
      fetch(`/setspeed?value=${value}`);
      document.getElementById('currentSpeed').textContent = value;
    }
  </script>
</body>
</html>

)rawliteral";

void rotary_loop()
{
    //dont print anything unless value changed
    if (rotaryEncoder.encoderChanged())
    {
    display.clearDisplay();
    Serial.print("Value: ");
    Serial.println(rotaryEncoder.readEncoder());
    display.setCursor(0, 20); // Line 2
    display.print("Speed: ");
    display.println(motorSpeed);
    display.println(rotaryEncoder.readEncoder());
    display.clearDisplay();
  }
    }
    //if (rotaryEncoder.isEncoderButtonClicked())
  //  {
 //           rotary_onButtonClick();
 //   }
//}

void IRAM_ATTR readEncoderISR()
{
    rotaryEncoder.readEncoder_ISR();
}
void setup() {
  Serial.begin(115200);
      //we must initialize rotary encoder
    rotaryEncoder.begin();
    rotaryEncoder.setup(readEncoderISR);
    //set boundaries and if values should cycle or not
    //in this example we will set possible values between 0 and 1000;
    bool circleValues = false;
    rotaryEncoder.setBoundaries(0, 255, circleValues); //minValue, maxValue, circleValues true|false (when max go to min and vice versa)

    /*Rotary acceleration introduced 25.2.2021.
   * in case range to select is huge, for example - select a value between 0 and 1000 and we want 785
   * without accelerateion you need long time to get to that number
   * Using acceleration, faster you turn, faster will the value raise.
   * For fine tuning slow down.
   */
    //rotaryEncoder.disableAcceleration(); //acceleration is now enabled by default - disable if you dont need it
    rotaryEncoder.setAcceleration(250); //or set the value - larger number = more accelearation; 0 or 1 means disabled acceleration

  //Start the Wifi Access Point
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  server.begin();



  // Motor pins setup
  pinMode(motorPin1, OUTPUT);
  pinMode(motorPin2, OUTPUT);
  pinMode(enablePin, OUTPUT);

  // Initialize OLED display
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  // Text Size and reset here------------------
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  // Connect to WiFi
 // WiFi.begin(ssid, password);
 // while (WiFi.status() != WL_CONNECTED) {
 //   delay(1000);
 //   Serial.println("Connecting to WiFi...");
 // }
 // Serial.println("Connected to WiFi!");
 // Serial.println(WiFi.localIP());

  // Display IP address on OLED
  display.clearDisplay();
  display.setCursor(0, 5);
  display.print("");
 // display.println(WiFi.localIP());
  display.println(IP);
  display.setCursor(0, 10);
 // display.print("Speed: ");
 // display.println(motorSpeed);
  display.display();

  // Start the server
  server.on("/", []() {
    server.send(200, "text/html", htmlPage);
  });

  server.on("/setspeed", []() {
    if (server.hasArg("value")) {
      motorSpeed = server.arg("value").toInt();
      analogWrite(enablePin, motorSpeed);
      // Forward direction
      digitalWrite(motorPin1, HIGH);
      digitalWrite(motorPin2, LOW);

      // Update OLED display
      long motorSpeed = rotaryEncoder.readEncoder();
      IPAddress IP = WiFi.softAPIP();
      display.setCursor(0, 5);
     // display.print("");
      // display.println(WiFi.localIP());
      display.println(IP);
    //  display.setCursor(0, 50); // Line 2
    //  display.print("Speed: ");
    //  display.println(motorSpeed);
      display.display();
     rotary_loop();
    }
    server.send(200, "text/plain", "Speed updated");
  });

  server.begin();
}

void rotary_onButtonClick()
{
    static unsigned long lastTimePressed = 0; // Soft debouncing
    if (millis() - lastTimePressed < 500)
    {
            return;
    }
    lastTimePressed = millis();
    Serial.print("button pressed ");
    Serial.print(millis());
    Serial.println(" milliseconds after restart");
}


void loop() {
  server.handleClient();
 IPAddress IP = WiFi.softAPIP();

  // Read rotary encoder value
  
  int newSpeed = rotaryEncoder.readEncoder();
  if (newSpeed != motorSpeed) {
    motorSpeed = newSpeed;
    analogWrite(enablePin, motorSpeed);

    // Forward direction
    digitalWrite(motorPin1, HIGH);
    digitalWrite(motorPin2, LOW);

    // Update OLED display dynamically
  //  display.setCursor(0, 10); // Line 2
   // display.print("Speed: ");
   // display.println(motorSpeed);
   // display.println(newSpeed);
   // display.clearDisplay();
   rotary_loop();
  }

  delay(10);
}