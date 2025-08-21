

/*
  ESP32-2432S028R Motor Control with L298N + LVGL 9.3.0 GUI + Touch + WiFi AP + Web Interface
  - Uses TFT_eSPI as LVGL display driver & touch input
  - Web interface matches local GUI dark theme
  - All in one .ino file

  Prerequisites:
  - Configure TFT_eSPI User_Setup.h for ESP32-2432S028R + XPT2046 touch (see instructions below)
  - Install lvgl 9.3.0, TFT_eSPI, AsyncTCP, ESPAsyncWebServer libraries

  Motor Pins (example):
    IN1 -> GPIO26
    IN2 -> GPIO27
    ENA -> GPIO14 (PWM channel 0)
    IN3 -> GPIO25
    IN4 -> GPIO33
    ENB -> GPIO12 (PWM channel 1)

  Touch pins configured in TFT_eSPI User_Setup.h (usually TOUCH_CS=21, TOUCH_IRQ=39)
*/

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>


#include <TFT_eSPI.h>
#include <lvgl.h>
TFT_eSPI tft = TFT_eSPI();

static lv_draw_buf_t draw_buf;
static lv_color_t buf[LV_HOR_RES_MAX * 40];

// Motor Pins
const int IN1 = 26;
const int IN2 = 27;
const int ENA = 14;

const int IN3 = 25;
const int IN4 = 33;
const int ENB = 12;

// PWM config
const int pwmFreq = 20000;
const int pwmChannelA = 0;
const int pwmChannelB = 1;
const int pwmResolution = 8;

// WiFi AP credentials
const char* ssid = "ESP32-Motor-Control";
const char* password = "12345678";

// Async Web Server on port 80
AsyncWebServer server(80);

// Motor control state
volatile int motorSpeed = 0; // 0-255
volatile bool motorDirectionForward = true;

// LVGL Widgets
lv_obj_t* speed_label;
lv_obj_t* speed_slider;
lv_obj_t* dir_forward_btn;
lv_obj_t* dir_backward_btn;

// Forward declarations
void setupMotor();
void updateMotor();
void handleMotorControl(int speed, bool forward);
void wifiInit();
void webServerInit();
void drawLVGLGUI();
void slider_event_cb(lv_event_t * e);
void dir_btn_event_cb(lv_event_t * e);
bool touchpad_read(lv_indev_drv_t * indev_driver, lv_indev_data_t * data);
void tft_flush_lvgl(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);

hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  lv_tick_inc(5);
  portEXIT_CRITICAL_ISR(&timerMux);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  setupMotor();

  // Setup PWM channels
  ledcSetup(pwmChannelA, pwmFreq, pwmResolution);
  ledcAttachPin(ENA, pwmChannelA);

  ledcSetup(pwmChannelB, pwmFreq, pwmResolution);
  ledcAttachPin(ENB, pwmChannelB);

  tft.init();
  tft.setRotation(1);

  lv_init();

  lv_draw_buf_init(&draw_buf, buf, NULL, LV_HOR_RES_MAX * 40);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.flush_cb = tft_flush_lvgl;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.hor_res = 240;
  disp_drv.ver_res = 320;
  lv_disp_drv_register(&disp_drv);

  // Register touch input device for LVGL
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touchpad_read;
  lv_indev_drv_register(&indev_drv);

  drawLVGLGUI();

  wifiInit();
  webServerInit();

  updateMotor();

  // Setup LVGL tick timer (5ms)
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer);
  timerAlarmWrite(timer, 5000, true);
  timerAlarmEnable(timer);
}

void loop() {
  lv_task_handler();
  delay(5);
}

// Motor setup
void setupMotor(){
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);

  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENB, OUTPUT);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  ledcWrite(pwmChannelA, 0);
  ledcWrite(pwmChannelB, 0);
}

// Update motor output pins and PWM
void updateMotor(){
  if(motorDirectionForward){
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
  } else {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
  }
  ledcWrite(pwmChannelA, motorSpeed);
  ledcWrite(pwmChannelB, motorSpeed);
}

// Handle motor control from web or local GUI
void handleMotorControl(int speed, bool forward){
  if(speed < 0) speed = 0;
  if(speed > 255) speed = 255;

  motorSpeed = speed;
  motorDirectionForward = forward;
  updateMotor();

  // Update LVGL widgets to reflect new state
  lv_slider_set_value(speed_slider, motorSpeed, LV_ANIM_ON);
  lv_label_set_text_fmt(speed_label, "Speed: %d", motorSpeed);

  if(motorDirectionForward){
    lv_obj_add_state(dir_forward_btn, LV_STATE_CHECKED);
    lv_obj_clear_state(dir_backward_btn, LV_STATE_CHECKED);
  } else {
    lv_obj_add_state(dir_backward_btn, LV_STATE_CHECKED);
    lv_obj_clear_state(dir_forward_btn, LV_STATE_CHECKED);
  }
}

void wifiInit(){
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("WiFi AP IP address: ");
  Serial.println(IP);
}

void webServerInit(){
  // Serve main page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  // API to control motor: /control?speed=0-255&dir=forward|backward
  server.on("/control", HTTP_GET, [](AsyncWebServerRequest *request){
    String speedStr = "0";
    String dirStr = "forward";

    if(request->hasParam("speed")) speedStr = request->getParam("speed")->value();
    if(request->hasParam("dir")) dirStr = request->getParam("dir")->value();

    int speed = speedStr.toInt();
    bool forward = dirStr.equalsIgnoreCase("forward");

    handleMotorControl(speed, forward);

    request->send(200, "application/json", "{\"status\":\"ok\"}");
  });

  server.begin();
}

void drawLVGLGUI(){
  lv_obj_t * scr = lv_scr_act();

  // Background color black
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);

  // Title label
  lv_obj_t* title = lv_label_create(scr);
  lv_label_set_text(title, "ESP32 Motor Control");
  lv_obj_set_style_text_color(title, lv_color_hex(0x00BFA5), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

  // Speed label
  speed_label = lv_label_create(scr);
  lv_label_set_text_fmt(speed_label, "Speed: %d", motorSpeed);
  lv_obj_set_style_text_color(speed_label, lv_color_hex(0xC8C8C8), 0);
  lv_obj_set_style_text_font(speed_label, &lv_font_montserrat_16, 0);
  lv_obj_align(speed_label, LV_ALIGN_TOP_LEFT, 10, 50);

  // Speed slider
  speed_slider = lv_slider_create(scr);
  lv_slider_set_range(speed_slider, 0, 255);
  lv_slider_set_value(speed_slider, motorSpeed, LV_ANIM_OFF);
  lv_obj_set_width(speed_slider, 220);
  lv_obj_align(speed_slider, LV_ALIGN_TOP_LEFT, 10, 75);
  lv_obj_add_event_cb(speed_slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

  // Direction label
  lv_obj_t* dir_label = lv_label_create(scr);
  lv_label_set_text(dir_label, "Direction:");
  lv_obj_set_style_text_color(dir_label, lv_color_hex(0xC8C8C8), 0);
  lv_obj_set_style_text_font(dir_label, &lv_font_montserrat_16, 0);
  lv_obj_align(dir_label, LV_ALIGN_TOP_LEFT, 10, 120);

  // Direction buttons container
  lv_obj_t* btn_container = lv_obj_create(scr);
  lv_obj_set_size(btn_container, 240, 50);
  lv_obj_align(btn_container, LV_ALIGN_TOP_LEFT, 10, 145);
  lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_color(btn_container, lv_color_black(), 0);
  lv_obj_set_style_border_width(btn_container, 0, 0);

  // Forward button
  dir_forward_btn = lv_btn_create(btn_container);
  lv_obj_set_size(dir_forward_btn, 100, 40);
  lv_obj_add_state(dir_forward_btn, LV_STATE_CHECKED); // default forward
  lv_obj_set_style_bg_color(dir_forward_btn, lv_color_hex(0x282828), 0);
  lv_obj_set_style_bg_color(dir_forward_btn, lv_color_hex(0x00BFA5), LV_STATE_CHECKED);
  lv_obj_set_style_border_color(dir_forward_btn, lv_color_hex(0x00BFA5), 0);
  lv_obj_set_style_border_color(dir_forward_btn, lv_color_hex(0x00BFA5), LV_STATE_CHECKED);
  lv_obj_set_style_border_width(dir_forward_btn, 2, 0);
  lv_obj_set_style_border_width(dir_forward_btn, 2, LV_STATE_CHECKED);
  lv_obj_add_event_cb(dir_forward_btn, dir_btn_event_cb, LV_EVENT_CLICKED, (void*)true);

  lv_obj_t* f_label = lv_label_create(dir_forward_btn);
  lv_label_set_text(f_label, "Forward");
  lv_obj_center(f_label);

  // Backward button
  dir_backward_btn = lv_btn_create(btn_container);
  lv_obj_set_size(dir_backward_btn, 100, 40);
  lv_obj_set_style_bg_color(dir_backward_btn, lv_color_hex(0x282828), 0);
  lv_obj_set_style_bg_color(dir_backward_btn, lv_color_hex(0x00BFA5), LV_STATE_CHECKED);
  lv_obj_set_style_border_color(dir_backward_btn, lv_color_hex(0x00BFA5), 0);
  lv_obj_set_style_border_color(dir_backward_btn, lv_color_hex(0x00BFA5), LV_STATE_CHECKED);
  lv_obj_set_style_border_width(dir_backward_btn, 2, 0);
  lv_obj_set_style_border_width(dir_backward_btn, 2, LV_STATE_CHECKED);
  lv_obj_add_event_cb(dir_backward_btn, dir_btn_event_cb, LV_EVENT_CLICKED, (void*)false);

  lv_obj_t* b_label = lv_label_create(dir_backward_btn);
  lv_label_set_text(b_label, "Backward");
  lv_obj_center(b_label);
}

// LVGL slider event callback
void slider_event_cb(lv_event_t * e) {
  lv_obj_t * slider = (lv_obj_t *)lv_event_get_target(e);
  int val = lv_slider_get_value(slider);

  motorSpeed = val;
  lv_label_set_text_fmt(speed_label, "Speed: %d", motorSpeed);
  updateMotor();
}

// LVGL direction button event callback
void dir_btn_event_cb(lv_event_t * e) {
  lv_obj_t * btn = (lv_obj_t *)lv_event_get_target(e);
  bool forward = (bool)lv_event_get_user_data(e);

  motorDirectionForward = forward;
  updateMotor();

  // Update buttons states
  if(forward){
    lv_obj_add_state(dir_forward_btn, LV_STATE_CHECKED);
    lv_obj_clear_state(dir_backward_btn, LV_STATE_CHECKED);
  } else {
    lv_obj_add_state(dir_backward_btn, LV_STATE_CHECKED);
    lv_obj_clear_state(dir_forward_btn, LV_STATE_CHECKED);
  }
}

// TFT_eSPI flush callback for LVGL
void tft_flush_lvgl(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors(&color_p->full, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp);
}

// Touchpad read function for LVGL
bool touchpad_read(lv_indev_drv_t * indev_driver, lv_indev_data_t * data) {
  uint16_t touchX, touchY;

  if (tft.getTouch(&touchX, &touchY)) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = touchX;
    data->point.y = touchY;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
  return false;
}

// HTML webpage embedded as PROGMEM string - dark foamdisplay style
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8" />
<meta name="viewport" content="width=device-width, initial-scale=1" />
<title>ESP32 Motor Control</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;700&display=swap');
  body {
    margin: 0; padding: 0;
    background-color: #000000;
    color: #c8c8c8;
    font-family: 'Inter', sans-serif;
    display: flex;
    flex-direction: column;
    align-items: center;
    height: 100vh;
    justify-content: center;
  }
  h1 {
    font-weight: 700;
    margin-bottom: 1rem;
    color: #00bfa5;
  }
  .slider-container {
    width: 90vw;
    max-width: 350px;
    margin-bottom: 2rem;
  }
  .slider-label {
    font-weight: 700;
    margin-bottom: 0.5rem;
  }
  input[type=range] {
    -webkit-appearance: none;
    width: 100%;
    height: 12px;
    border-radius: 6px;
    background: #282828;
    outline: none;
  }
  input[type=range]::-webkit-slider-thumb {
    -webkit-appearance: none;
    appearance: none;
    width: 28px;
    height: 28px;
    border-radius: 50%;
    background: #00bfa5;
    cursor: pointer;
    border: none;
    margin-top: -8px;
  }
  input[type=range]::-moz-range-thumb {
    width: 28px;
    height: 28px;
    border-radius: 50%;
    background: #00bfa5;
    cursor: pointer;
    border: none;
  }
  .direction-container {
    display: flex;
    justify-content: center;
    gap: 1rem;
  }
  .direction-button {
    background-color: #282828;
    border: 2px solid #c8c8c8;
    color: #c8c8c8;
    padding: 1rem 2rem;
    border-radius: 12px;
    font-weight: 700;
    cursor: pointer;
    user-select: none;
    transition: background-color 0.3s, border-color 0.3s;
  }
  .direction-button.active {
    background-color: #00bfa5;
    border-color: #00bfa5;
    color: #000;
  }
  .speed-display {
    text-align: center;
    font-size: 1.5rem;
    margin-top: -1rem;
    margin-bottom: 2rem;
    font-weight: 700;
  }
</style>
</head>
<body>
<h1>ESP32 Motor Control</h1>
<div class="slider-container">
  <label class="slider-label" for="speedRange">Speed</label>
  <input type="range" min="0" max="255" value="0" id="speedRange" />
  <div class="speed-display" id="speedValue">0</div>
</div>
<div class="direction-container">
  <div id="forwardBtn" class="direction-button active">Forward</div>
  <div id="backwardBtn" class="direction-button">Backward</div>
</div>

<script>
  const speedRange = document.getElementById('speedRange');
  const speedValue = document.getElementById('speedValue');
  const forwardBtn = document.getElementById('forwardBtn');
  const backwardBtn = document.getElementById('backwardBtn');

  let currentSpeed = 0;
  let currentDir = 'forward';

  speedRange.oninput = function() {
    currentSpeed = this.value;
    speedValue.textContent = currentSpeed;
    sendControl();
  };

  forwardBtn.onclick = function() {
    if(currentDir !== 'forward'){
      currentDir = 'forward';
      forwardBtn.classList.add('active');
      backwardBtn.classList.remove('active');
      sendControl();
    }
  };

  backwardBtn.onclick = function() {
    if(currentDir !== 'backward'){
      currentDir = 'backward';
      backwardBtn.classList.add('active');
      forwardBtn.classList.remove('active');
      sendControl();
    }
  };

  function sendControl(){
    fetch(`/control?speed=${currentSpeed}&dir=${currentDir}`)
    .then(response => response.json())
    .then(data => {})
    .catch(err => {
      console.error('Error sending control:', err);
    });
  }
</script>
</body>
</html>
)rawliteral";

String processor(const String& var){
  return String();
}
