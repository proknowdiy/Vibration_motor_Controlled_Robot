#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Servo.h>

#define SIGNAL_TIMEOUT 1000  

// Pins
#define SERVO_PIN 12
#define IN1 13
#define IN2 14
#define head_light_pin 4
#define back_light_pin 5

unsigned long lastRecvTime = 0;

typedef struct PacketData
{
  byte throttle;    
  byte steering;    
  bool head_light;
  bool back_light;
  
} PacketData;

PacketData receiverData;

Servo steeringServo;

// Default values
void setInputDefaultValues()
{
  receiverData.throttle = 127;
  receiverData.steering = 127;
  receiverData.head_light = 1;
  receiverData.back_light = 1;
}

// 🚗 MOTOR + SERVO + LIGHT CONTROL

// void mapAndWriteValues()
// {
//   // 🎯 Steering Servo
//   steeringServo.write(map(receiverData.steering, 0, 254, 0, 180));

//   // 🚗 PWM MOTOR CONTROL
//   int throttle = receiverData.throttle;

//   int deadzone = 15;   // small neutral zone
//   int center = 127;

//   if (throttle > center + deadzone) {
//     int speed = map(throttle, center + deadzone, 254, 0, 1023);
//     analogWrite(IN1, speed);
//     digitalWrite(IN2, LOW);
//   }
//   else if (throttle < center - deadzone) {
//     int speed = map(throttle, 0, center - deadzone, 1023, 0);
//     analogWrite(IN2, speed);
//     digitalWrite(IN1, LOW);
//   }
//   else {
//     // Neutral → coast
//     digitalWrite(IN1, HIGH);   //LOW/LOW → smooth coast    //HIGH/HIGH → aggressive brake
//     digitalWrite(IN2, HIGH);
//   }

//   // 💡 Lights
//   digitalWrite(head_light_pin, receiverData.head_light);
//   digitalWrite(back_light_pin, receiverData.back_light);
// }

void mapAndWriteValues()
{
  steeringServo.write(map(receiverData.steering, 0, 254, 0, 180));

  int throttle = receiverData.throttle;
  int steering = receiverData.steering;

  int deadzone = 15;
  int center = 127;

  static unsigned long lastSwitchTime = 0;
  static bool directionFlag = false;

  unsigned long currentTime = millis();

  // Speed from throttle (or fixed if needed)
  int speed = map(throttle, center + deadzone, 254, 300, 1023);
  if (speed < 300) speed = 300;

  bool forward = (throttle > center + deadzone);
  bool left    = (steering < center - deadzone);
  bool right   = (steering > center + deadzone);

  // ================= PRIORITY: TURNING =================

  if (left)
  {
    // Rotate LEFT (CW)
    analogWrite(IN1, speed);
    digitalWrite(IN2, LOW);
  }
  else if (right)
  {
    // Rotate RIGHT (CCW)
    analogWrite(IN2, speed);
    digitalWrite(IN1, LOW);
  }

  // ================= FORWARD =================

  else if (forward)
  {
    // Rapid switching → straight motion
    if (currentTime - lastSwitchTime >= 250 )  //5
    {
      lastSwitchTime = currentTime;
      directionFlag = !directionFlag;

      if (directionFlag)
      {
        analogWrite(IN1, speed);
        digitalWrite(IN2, LOW);
      }
      else
      {
        analogWrite(IN2, speed);
        digitalWrite(IN1, LOW);
      }
    }
  }

  // ================= STOP =================

  else
  {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
  }

  // ================= LIGHTS =================
  digitalWrite(head_light_pin, receiverData.head_light);
  digitalWrite(back_light_pin, receiverData.back_light);
}

// 📡 Receive callback
void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) 
{
  if (len == 0) return;

  memcpy(&receiverData, incomingData, sizeof(receiverData));

  Serial.printf("T:%d S:%d HL:%d BL:%d\n",
    receiverData.throttle,
    receiverData.steering,
    receiverData.head_light,
    receiverData.back_light);

  mapAndWriteValues();  
  lastRecvTime = millis(); 
}

// Setup pins
void setUpPinModes()
{
  steeringServo.attach(SERVO_PIN, 1000, 2000);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(head_light_pin, OUTPUT);
  pinMode(back_light_pin, OUTPUT);

  analogWriteFreq(20000);   // 20kHz (silent, smooth)
  analogWriteRange(1023);   // max resolution

  setInputDefaultValues();
  mapAndWriteValues();
}

void setup() 
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("Receiver Booting...");

  setUpPinModes();

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != 0) 
  {
    Serial.println("ESP-NOW Init Failed");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(OnDataRecv);

  Serial.println("ESP-NOW Ready");
}

void loop()
{
  // 🔒 FAILSAFE
  if (millis() - lastRecvTime > SIGNAL_TIMEOUT) 
  {
    setInputDefaultValues();
    mapAndWriteValues();  
  }
}