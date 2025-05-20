#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Encoder.h>
#include "esp32-hal-ledc.h" // Add this line for PWM functions

// WiFi credentials
const char* ssid = "SurgeFX";
const char* password = "password";

// Pin definitions
#define MOTOR_PIN1 26  // H-Bridge input 1
#define MOTOR_PIN2 27  // H-Bridge input 2
#define MOTOR_EN 14    // H-Bridge enable pin
#define ENCODER_A 32   // Rotary encoder pin A
#define ENCODER_B 33   // Rotary encoder pin B

// OLED display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1  // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C

// PWM configuration
#define PWM_CHANNEL 0
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8

// Initialize objects
WebServer server(80);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
ESP32Encoder encoder;

// Global variables
int speedValue = 0;
bool updateDisplay = true;

// HTML webpage
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta charset="UTF-8">
  <title>SurgeFX Foam Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  
   <style>
        body { font-family: Arial, sans-serif; background:rgb(46, 46, 46); color: #222; margin: 0; padding: 0;}
        .container { max-width: 400px; margin: 40px auto; background:rgb(46, 46, 46); border-radius: 8px; box-shadow: 0 2px 8px #ccc; padding: 24px;}
        h1 { text-align: center; margin-bottom: 0.5em; color:rgb(235, 133, 37);}
        .reading { font-size: 1.4em; margin: 1em 0; color:rgb(230, 230, 230); text-align: center;}
        .label { font-weight: bold; margin-bottom: 8px; color:rgb(230, 230, 230); display: block;}
        .form-group { margin: 16px 0; text-align: center;}
        input[type="number"] { width: 80px; padding: 0.5em; margin-right: 8px;}

	.slidecontainer {
	width: 100%; /* Width of the outside container */
	}
	
	/* The slider itself */
	.slider {
	-webkit-appearance: none;  /* Override default CSS styles */
	appearance: none;
	width: 100%; /* Full-width */
	height: 50px; /* Specified height */
	background: #d3d3d3; /* Grey background */
	outline: none; /* Remove outline */
	opacity: 0.7; /* Set transparency (for mouse-over effects on hover) */
	-webkit-transition: .2s; /* 0.2 seconds transition on hover */
	transition: opacity .2s;
	}

	/* Mouse-over effects */
	.slider:hover {
	opacity: 1; /* Fully shown on mouse-over */
	}

	/* The slider handle (use -webkit- (Chrome, Opera, Safari, Edge) and -moz- (Firefox) to override default look) */
	.slider::-webkit-slider-thumb {
	-webkit-appearance: none; /* Override default look */
	appearance: none;
	width: 35px; /* Set a specific slider handle width */
	height: 50px; /* Slider handle height */
	background: #04AA6D; /* Green background */
	cursor: pointer; /* Cursor on hover */
	}

	.slider::-moz-range-thumb {
	width: 35px; /* Set a specific slider handle width */
	height: 50px; /* Slider handle height */
	background: #04AA6D; /* Green background */
	cursor: pointer; /* Cursor on hover */
	}
        .duration-btns button {
          background: #2563eb;
          color: #fff;
          border: none;
          border-radius: 4px;
          padding: 10px 22px;
          margin: 0 5px;
          font-size: 1em;
          cursor: pointer;
          transition: background 0.2s;
        }
        .duration-btns button.selected, .duration-btns button:hover {
          background: rgb(11, 38, 114);
		  
        }
        .submit-btn {
          margin-top: 18px;
          background: #059669;
          color: #fff;
          border: none;
          border-radius: 4px;
          padding: 10px 28px;
          font-size: 1em;
          cursor: pointer;
          transition: background 0.2s;
        }
		a:link, a:visited {
			
			color: white;
			
			text-align: center;
			text-decoration: none;
			}

			a:hover, a:active {
			background-color: red;
			}
        .submit-btn:hover { background: #047857;}
        .status { margin-top: 18px; text-align: center;}
        .footer { margin-top: 32px; text-align: center; font-size: 0.95em; color: #888;}
      </style>
  
</head>
<body>
  <div class="container">
  <h1>SurgeFX Foam Control</h1>
  <div class="reading">
  <div class="slidecontainer">
  <input type="range" min="0" max="250" value="0" class="slider" step="10" id="speedSlider" list="ticks">
  <datalist id="ticks">
    <option value="50"></option>
    <option value="100"></option>
    <option value="150"></option>
    <option value="250"></option>
  </datalist>
  </div>
    <p>Speed: <span id="speedValue">0</span></p>
  </div>
  <script>
    var slider = document.getElementById("speedSlider");
    var output = document.getElementById("speedValue");
    slider.oninput = function() {
      output.innerHTML = this.value;
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/speed?value=" + this.value, true);
      xhr.send();
    }
    setInterval(function() {
      var xhr = new XMLHttpRequest();
      xhr.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          slider.value = this.responseText;
          output.innerHTML = this.responseText;
        }
      };
      xhr.open("GET", "/getSpeed", true);
      xhr.send();
    }, 1000);
  </script>
   <div class="footer">SurgeFX &copy; 2025</div>
  </div>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  
  // Initialize OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();
  
  // Initialize encoder
  encoder.attachHalfQuad(ENCODER_A, ENCODER_B);
  encoder.setCount(0);
  
  // Initialize motor pins
  pinMode(MOTOR_PIN1, OUTPUT);
  pinMode(MOTOR_PIN2, OUTPUT);

//  ledc Functions are depreciated

//  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
//  ledcAttachPin(MOTOR_EN, PWM_CHANNEL);

// -----------------------------------------------
    // Connect to an existing WiFi 

//  WiFi.begin(ssid, password);
//  while (WiFi.status() != WL_CONNECTED) {
//   delay(500);
//   Serial.print(".");
//  }
// -----------------------------------------------  


    // Start the Wifi Access Point
    // Remove if using an existing WiFi
  WiFi.softAP(ssid, password);

  // Display IP on OLED
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("IP:");
  display.println(WiFi.softAPIP());
  display.display();
  
  // Setup web server routes
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", index_html);
  });
  
  server.on("/speed", HTTP_GET, []() {
    if (server.hasArg("value")) {
      speedValue = server.arg("value").toInt();
      updateMotor();
      updateDisplay = true;
    }
    server.send(200, "text/plain", "OK");
  });
  
  server.on("/getSpeed", HTTP_GET, []() {
    server.send(200, "text/plain", String(speedValue));
  });
  
  server.begin();
}

// ledc functions are depreciated

//void updateMotor() {
//  if (speedValue > 0) {
//    digitalWrite(MOTOR_PIN1, HIGH);
//    digitalWrite(MOTOR_PIN2, LOW);
//    ledcWrite(PWM_CHANNEL, speedValue);
//  } else {
//    digitalWrite(MOTOR_PIN1, LOW);
//    digitalWrite(MOTOR_PIN2, LOW);
//    ledcWrite(PWM_CHANNEL, 0);
//  }
//}

// Alternative PWM method
void updateMotor() {
  if (speedValue > 0) {
    digitalWrite(MOTOR_PIN1, HIGH);
    digitalWrite(MOTOR_PIN2, LOW);
    analogWrite(MOTOR_EN, speedValue);  // Using analogWrite instead of ledcWrite
  } else {
    digitalWrite(MOTOR_PIN1, LOW);
    digitalWrite(MOTOR_PIN2, LOW);
    analogWrite(MOTOR_EN, 0);
  }
}

void updateOLED() {
  display.clearDisplay();
  
  // Display IP address
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("IP:");
  display.println(WiFi.softAPIP());
  
  // Display speed value
  display.setCursor(0, 32);
  display.print("Speed: ");
  display.println(speedValue);
  
  // Draw a progress bar
  int barWidth = map(speedValue, 0, 255, 0, SCREEN_WIDTH - 4);
  display.drawRect(0, 50, SCREEN_WIDTH - 2, 10, SSD1306_WHITE);
  display.fillRect(2, 52, barWidth, 6, SSD1306_WHITE);
  
  display.display();
}

void loop() {
  server.handleClient();
  
  // Handle encoder
  static int lastCount = 0;
  int count = encoder.getCount();
  
  if (count != lastCount) {
    speedValue = constrain(count, 0, 255);
    encoder.setCount(speedValue);
    lastCount = speedValue;
    updateMotor();
    updateDisplay = true;
  }
  
  // Update OLED display
  if (updateDisplay) {
    updateOLED();
    updateDisplay = false;
  }
  
  delay(10);
}
