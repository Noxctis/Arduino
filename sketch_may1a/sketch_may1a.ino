#include <QTRSensors.h>
#include "DualVNH5019MotorShield.h"
#include <Encoder.h>

// ================= USER CONFIGURATION =================
// Motor PWM values (–400…+400)
const int pwmM1             = 400;
const int pwmM2             = 400;
// Battery voltage for telemetry
const float batteryVoltage = 12.6;
// Motor telemetry interval (ms)
const unsigned long TELE_MS = 100;
// QTR sensors
#define NUM_SENSORS 15
const uint8_t SENSOR_PINS[NUM_SENSORS] = {
  A0,A1,A2,A3,A4,A5,A6,
  A7,A8,A9,A10,A11,A12,
  A13,A14
};
// ======================================================

DualVNH5019MotorShield md(8,9,10,7,39,  1,2,28,0,40);
Encoder enc1(5,6), enc2(11,12);
QTRSensors qtr;
uint16_t sensorValues[NUM_SENSORS];

// PID parameters (coarse+fine)
int   kp_c = 10;   float kp_f = 0.1;
int   ki_c = 0;    float ki_f = 0.1;
int   kd_c = 0;    float kd_f = 0.1;
float kp = kp_c + kp_f, ki = ki_c + ki_f, kd = kd_c + kd_f;

// Telemetry state
unsigned long lastT1=0, lastT2=0;
long lastC1=0, lastC2=0;
float dist1=0, dist2=0;

// Convert encoder ticks→mm
const float WHEEL_D_MM = 43.0;
const float WHEEL_CIRC = WHEEL_D_MM * 3.14159;
const float CPR50      = 12.0 * 50.0;

void stopIfFault(){
  if(md.getM1Fault()){
    const char* m="ERROR: M1 fault!";
    Serial.println(m); Serial8.println(m);
    while(1);
  }
  if(md.getM2Fault()){
    const char* m="ERROR: M2 fault!";
    Serial.println(m); Serial8.println(m);
    while(1);
  }
}

// Handle incoming kp/ki/kd commands
void handlePIDCmd(String cmd){
  if(cmd.startsWith("kp:")){
    int c=cmd.substring(3,cmd.indexOf(',',3)).toInt();
    float f=cmd.substring(cmd.indexOf(',',3)+1).toFloat();
    kp_c=c; kp_f=f; kp = c+f;
    Serial.printf("Kp updated to %.3f\n",kp);
    Serial8.printf("Kp updated to %.3f\n",kp);
  }
  else if(cmd.startsWith("ki:")){
    int c=cmd.substring(3,cmd.indexOf(',',3)).toInt();
    float f=cmd.substring(cmd.indexOf(',',3)+1).toFloat();
    ki_c=c; ki_f=f; ki=c+f;
    Serial.printf("Ki updated to %.3f\n",ki);
    Serial8.printf("Ki updated to %.3f\n",ki);
  }
  else if(cmd.startsWith("kd:")){
    int c=cmd.substring(3,cmd.indexOf(',',3)).toInt();
    float f=cmd.substring(cmd.indexOf(',',3)+1).toFloat();
    kd_c=c; kd_f=f; kd=c+f;
    Serial.printf("Kd updated to %.3f\n",kd);
    Serial8.printf("Kd updated to %.3f\n",kd);
  }
}

// Simple PID line-following
unsigned long lastPid=0;
float integral=0, lastErr=0;
void applyPID(unsigned int pos){
  float center = 1000.0 * (NUM_SENSORS-1)/2;
  float err = pos - center;
  unsigned long now = millis();
  float dt = (now - lastPid)/1000.0;
  lastPid = now;
  integral += err*dt;
  float deriv = (err - lastErr)/dt;
  lastErr = err;
  float corr = kp*err + ki*integral + kd*deriv;
  float base = pwmM1;
  int m1 = constrain(base + corr, -400, 400);
  int m2 = constrain(base - corr, -400, 400);
  md.setM1Speed(m1);
  md.setM2Speed(m2);
}

void setup(){
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(115200);
  Serial8.begin(115200);

  qtr.setTypeAnalog();
  qtr.setSensorPins(SENSOR_PINS,NUM_SENSORS);
  qtr.setEmitterPin(QTRNoEmitterPin);
  for(int i=0;i<200;i++){ qtr.read(sensorValues,QTRReadMode::On); delay(10); }

  md.init();
  enc1.write(0); enc2.write(0);
  lastT1=lastT2=millis(); lastC1=lastC2=0;
}

void loop(){
  // 1) PID tuning commands
  if(Serial8.available()){
    String cmd=Serial8.readStringUntil('\n');
    handlePIDCmd(cmd);
  }

  // 2) Sensor broadcast
  qtr.read(sensorValues,QTRReadMode::On);
  for(uint8_t i=0;i<NUM_SENSORS;i++){
    Serial8.print(sensorValues[i]);
    Serial8.print(i<NUM_SENSORS-1?'\t':'\n');
  }

  // 3) PID line follow
  unsigned int pos = qtr.readLineBlack(sensorValues,QTRReadMode::On);
  applyPID(pos);

  // 4) Motor telemetry (as before)...
  unsigned long now=millis();
  if(now-lastT1>=TELE_MS){
    long c1=-enc1.read(), d1=c1-lastC1;
    float dt=(now-lastT1)/1000.0, rev=d1/CPR50;
    float rpm=rev/dt*60, spd=rev*WHEEL_CIRC/dt;
    dist1+=rev*WHEEL_CIRC/1000.0;
    lastC1=c1; lastT1=now;
    int cur=md.getM1CurrentMilliamps();
    float vs=fabs(pwmM1)/400.0, vout=batteryVoltage*vs;
    String t1=String("M1: RPM=")+rpm+", spd="+spd+"mm/s, dist="+dist1+"m, Vbat="+vout+"V, PWM%="+(vs*100.0)+"%, Cur="+cur+"mA";
    Serial8.println(t1);
  }
  if(now-lastT2>=TELE_MS){
    long c2=-enc2.read(), d2=c2-lastC2;
    float dt=(now-lastT2)/1000.0, rev=d2/CPR50;
    float rpm=rev/dt*60, spd=rev*WHEEL_CIRC/dt;
    dist2+=rev*WHEEL_CIRC/1000.0;
    lastC2=c2; lastT2=now;
    int cur=md.getM2CurrentMilliamps();
    float vs=fabs(pwmM2)/400.0, vout=batteryVoltage*vs;
    String t2=String("M2: RPM=")+rpm+", spd="+spd+"mm/s, dist="+dist2+"m, Vbat="+vout+"V, PWM%="+(vs*100.0)+"%, Cur="+cur+"mA";
    Serial8.println(t2);
  }

  stopIfFault();
  delay(20);
}
