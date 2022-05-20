//Libraries
#include <PID_v1.h>     //PID library
#include <EnableInterrupt.h>  //library that enables to read pwm values
#include <Adafruit_MPU6050.h> //gyro library
#include <Adafruit_Sensor.h>  //library to run adafruit sensors
#include <Wire.h>   //library that enables serail communications
#include <GyroToVelocity.h>   //geometry functions for HIP and KNEE actuators
//Serial port baud and Arduino pins
#define SERIAL_PORT_SPEED 9600 
#define PWM_NUM  2 //number of PWM signals
#define PWMS  0 //speed is stored in the 0 position of the array
#define PWMP  1 //position is stored in the 1 position of the array
#define PWMS_INPUT  A0 //analog pin for the speed reading
#define PWMP_INPUT  A1 //analog pin for the position reading
#define ANV 5 //analog voltage speed control pin
#define IN1 2 //direction of actuation
#define IN2 3 //direction of actuation
#define TO_STANDING 53
//zero rate offset in radians/s
//recalibrate gyros periodically, probably once a month or before testing
const double gyro1[] = {-0.0213, -0.0192, -0.03};
const double gyro2[] = {-0.0919, 0.052, -0.019};
const double gyro3[] = {-0.0511, -0.0076, -0.0019};
const double gyro4[] = {-0.0588, -0.0185, -0.0298};
const double gyro5[] = {-0.0231, -0.0526, -0.0486};
const double gyro6[] = {-0.0254, -0.0077, -0.0045};
const double gyro7[] = {-0.1000, -0.0370, -0.0039};
const double gyro8[] = {-0.0791, -0.027, -0.0346};
//variable that holds the specific gyro offset
double X_OFFSET = 0;
double Y_OFFSET = 0;
double Z_OFFSET = 0;
//interrupt arrays
uint32_t pwm_start[PWM_NUM]; // stores the time when PWM square wave begins
uint16_t pwm_values[PWM_NUM]; //stores the width of the PWM pulse in microseconds
volatile uint16_t pwm_shared[PWM_NUM]; //array used in the interrupt routine to hold the pulse width values

Adafruit_MPU6050 mpu1;
Adafruit_MPU6050 mpu2;
float x_val1, y_val1, z_val1, result1;
float x_square1, y_square1, z_square1;
float accel_angle_x1, accel_angle_y1;


float x_val2, y_val2, z_val2, result2;
float x_square2, y_square2, z_square2;
float accel_angle_x2, accel_angle_y2;


float accel_center_x = 0.66;
float accel_center_y = -0.12;
float accel_center_z = 8.60;
double corrected_X, corrected_Y, corrected_Z; //variables that store the angular speed after the offset has been applied

bool lock_in_place = false;

//PID_runtime
double currentSpeed, currentSpeed_abs, outputSpeed ,desiredSpeed, desiredSpeed2;
double displacement; //position variable
double HP = 50, HI = 20, HD = 0;
PID loadCompensator(&currentSpeed_abs, &outputSpeed ,&desiredSpeed, HP, HI, HD,P_ON_M, DIRECT);
bool runtime_status = true; //keeps track if PID is on or off

/*
PID_stop

This PID is needed as this PID goes by displacment instead of speed
*/
double to_standing_error;
double setpoint = 6.125;   //the point that is considered to standing postion in inches
double HP_stop = 50, HI_stop = 20, HD_stop = 0;  //these are the parameters to the PID
PID StoppingPID(&displacement, &outputSpeed ,&setpoint, HP_stop, HI_stop, HD_stop,P_ON_M, DIRECT);
bool to_standing_status = false; //keeps track if PID is on or off
//PID tilt
double tilt_error;
double tilt_pilot, tilt_suit;
double HP_tilt, HI_tilt, HD_tilt;
PID tiltPID(&tilt_pilot, &outputSpeed, &tilt_suit, HP_tilt, HI_tilt, HD_tilt, P_ON_M, DIRECT);
bool tilt_status = true;

void setup() 
{
  //begin serial monitor
  Serial.begin(SERIAL_PORT_SPEED); //from 9600 to 115200


  //establish all pins I/O
  pinMode(PWMS_INPUT, INPUT);
  pinMode(PWMP_INPUT, INPUT);
  pinMode(TO_STANDING,INPUT);
  pinMode(ANV,OUTPUT);
  pinMode(IN1,OUTPUT);
  pinMode(IN2,OUTPUT);
  pinMode(22,OUTPUT); //dummy pin
  pinMode(LED_BUILTIN,OUTPUT);
  digitalWrite(22,HIGH); //always HIGH

  //interrupts used to read the linear actuator feedback
  enableInterrupt(PWMS_INPUT, calc_speed, CHANGE);
  enableInterrupt(PWMP_INPUT, calc_position, CHANGE); 
  //PI Controller for sending to soft stop
  //placed here as I will toggle the High Pin when turning off this PID
  StoppingPID.SetMode(MANUAL);
  StoppingPID.SetOutputLimits(0,255);
  StopPID_status(true); // turn off this PID
  
  //PI Controller for load compensator
  pwm_read_values(); 
  desiredSpeed = 0;
  loadCompensator.SetMode(AUTOMATIC);
  loadCompensator.SetOutputLimits(0,255);
  tiltPID.SetMode(AUTOMATIC);
  tiltPID.SetOutputLimits(0,255);
  HorK(HIP); //Hip or Knee geometry
  //Checks for gyroscope
  tilt_setup();
  offsetSwitch(7); //Check the gyro number connected to the arduino
  //Wire.SetWireTimeout(3000,true);
}

void loop() 
{
  sensors_event_t a1, g1, temp1;
  sensors_event_t a2, g2, temp2;
  mpu1.getEvent(&a1, &g1, &temp1);
  mpu2.getEvent(&a2, &g2, &temp2);
  x_val1 = a1.gyro.x - accel_center_x;
  y_val1 = a1.gyro.y - accel_center_y;
  z_val1 = a1.gyro.z - accel_center_z;
  x_val2 = a2.gyro.x - accel_center_x;
  y_val2 = a2.gyro.y - accel_center_y;
  z_val2 = a2.gyro.z - accel_center_z;
  tilt(x_val1, y_val1,z_val1, 0);
  tilt(x_val2, y_val2,z_val2, 1);
  tilt_suit = accel_angle_y2;
  tilt_pilot = accel_angle_y1;
  tilt_error = tilt_pilot - tilt_suit;
  bool tru = true;
  //if(abs(tilt_error) > 20*PI/360)
  //if(tru == false)
  if(tru == false)
  {
    loadCompensator.SetMode(MANUAL);
    tiltPID.SetMode(AUTOMATIC);
    Mdirection(tilt_error);
    tiltPID.Compute();
    analogWrite(ANV,outputSpeed);
    Serial.println("tilt");
  }
  else
  {
    loadCompensator.SetMode(AUTOMATIC);
    tiltPID.SetMode(MANUAL);
    //Sets the gyro value to 0 if it is below the threshold
    if(abs(g2.gyro.x - X_OFFSET) <= 0.3)  corrected_X = 0;
    else  corrected_X = g2.gyro.x - X_OFFSET;   
  
    pwm_read_values();
    desiredSpeed = abs(geometry(corrected_X, displacement));
    desiredSpeed2 = geometry(corrected_X, displacement);
    Mdirection(-corrected_X);
    loadCompensator.Compute();
    analogWrite(ANV,outputSpeed);
    //analogWrite(ANV,outputSpeed/5);
    Serial.print(g2.gyro.x);Serial.print("   ");Serial.print(corrected_X);Serial.print("   ");Serial.print(currentSpeed);Serial.print("   ");Serial.println(desiredSpeed);
  }
//////////////////////// TO standing implementation

  if((digitalRead(TO_STANDING))) toStanding();
  
////////////////////////
}
void toStanding()
{
  loadCompensator.SetMode(MANUAL);
  StoppingPID.SetMode(AUTOMATIC);
  pwm_read_values();
  
  Serial.println(".....Begin Move to Standing.....");
  String message = "Understood to be at distance " + String(displacement) +" Setpoint is : " + String(setpoint);
  Serial.println(message);
  to_standing_error = setpoint-displacement;
  //given 0.2 inch error within the setpoint
  while(abs(to_standing_error)>0.2)
  {
    //collect the current actuator speed and displacement
    pwm_read_values();  // this will update current speed and the displacment
    //compute the displacement with the PID
    to_standing_error = setpoint - displacement;
    Serial.println(to_standing_error);
    Mdirection(to_standing_error);
    StoppingPID.Compute(); 
    //write the PID result to the actuator
    analogWrite(ANV, outputSpeed);
   }
   Mdirection(0);
   Serial.println("Pause for durration");
   delay(500);
   Serial.println(".....resuming System.....");
   loadCompensator.SetMode(AUTOMATIC);
   StoppingPID.SetMode(MANUAL);
   lock_in_place = true;
   while(lock_in_place)
   {
    if(digitalRead(TO_STANDING))lock_in_place = false;
   }
   delay(500);
}
void Mdirection(float dir)
{
  if(dir > 0)
  {
    //extend
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
  }
  else if(dir < 0)
  {
    //retract
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
  }
  else
  {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
  }
}
void pwm_read_values() 
{
  noInterrupts(); //Disables interrupts to make sure it doesnt mess up the memory copy below
  memcpy(pwm_values, (const void *)pwm_shared, sizeof(pwm_shared)); //memcpy handles memory to memory copy, copies from pwm_shared to pwm_values
  interrupts(); //Enables interrupts
  currentSpeed = ((double)(pwm_values[PWMS] - 510)/26+1/13); //calculates the real value [inch/sec] for the actuator speed
  currentSpeed_abs = abs(currentSpeed);
  displacement = abs(7.5 / 990 * pwm_values[PWMP] - 7.5); //calculates the real value [inch] for the actuator position
  if(displacement > 7.5){displacement = 7.5;}
}

void calc_input(uint8_t channel, uint8_t input_pin) //Does the math for the PWM data
{
  if (digitalRead(input_pin) == HIGH) //Measures how long the PWM is that its peak
  {
    pwm_start[channel] = micros(); //Micros returns the number of microseconds since the board began running
  } 
  else 
  {
    uint16_t pwm_compare = (uint16_t)(micros() - pwm_start[channel]);
    if(pwm_compare > 1002)
    {
      pwm_compare = 0;
    }
    pwm_shared[channel] = pwm_compare;
  }
}
void calc_speed() { calc_input(PWMS, PWMS_INPUT); }
void calc_position() { calc_input(PWMP, PWMP_INPUT); }

/*
 * This function is used to turn the stopping PID on and off
 *  IMPORTANT NOTE: This will also toggle the other PID on/off
 */

void StopPID_status(bool PID_off)
{
  
  if(PID_off)
  {
    StoppingPID.SetMode(MANUAL); //Manual - PI is deactivated
    to_standing_status = MANUAL;
    
    //turn other PID on
    loadCompensator.SetMode(AUTOMATIC);//Automatic - PI is activated
    runtime_status = AUTOMATIC;
  }
  else
  {
    StoppingPID.SetMode(AUTOMATIC);//Automatic - PI is activated
    to_standing_status = AUTOMATIC;

    //turn other PID off
    loadCompensator.SetMode(MANUAL); //Manual - PI is deactivated
    runtime_status = MANUAL;
  }
}


//the code below is going through testing
void PIDtoggle(bool hardstop) 
{
  if(hardstop == true || displacement >= 7.5 || displacement <= 0.05)
  {
    loadCompensator.SetMode(MANUAL); //Manual - PI is deactivated
    runtime_status = MANUAL;
    digitalWrite(22,HIGH);
  }
  else
  {
    loadCompensator.SetMode(AUTOMATIC);//Automatic - PI is activated
    runtime_status = AUTOMATIC;
    digitalWrite(22,LOW);
  }
}
void offsetSwitch(int choice)
{
    switch(choice)
    {
        case 1:
            X_OFFSET = gyro1[1];
            Y_OFFSET = gyro1[2];
            Z_OFFSET = gyro1[3];
        break;

        case 2:
            X_OFFSET = gyro2[1];
            Y_OFFSET = gyro2[2];
            Z_OFFSET = gyro2[3];
        break;

        case 3:
            X_OFFSET = gyro3[1];
            Y_OFFSET = gyro3[2];
            Z_OFFSET = gyro3[3];
        break;
       
        case 4:
            X_OFFSET = gyro2[1];
            Y_OFFSET = gyro2[2];
            Z_OFFSET = gyro2[3];
        break;

        case 5:
            X_OFFSET = gyro2[1];
            Y_OFFSET = gyro2[2];
            Z_OFFSET = gyro2[3];
        break;

        case 6:
            X_OFFSET = gyro6[1];
            Y_OFFSET = gyro6[2];
            Z_OFFSET = gyro6[3];
        break;

        case 7:
            X_OFFSET = gyro7[1];
            Y_OFFSET = gyro7[2];
            Z_OFFSET = gyro7[3];
        break;

        case 8:
            X_OFFSET = gyro8[1];
            Y_OFFSET = gyro8[2];
            Z_OFFSET = gyro8[3];
        break;

    }
}
void tilt_setup()
{
  if (!mpu1.begin(0x68)) 
  {
    digitalWrite(LED_BUILTIN,HIGH); //built in led high means the arduino failed to recognize the gyro
    while (1) 
    {
      delay(10);
    }
  }
  else  digitalWrite(LED_BUILTIN,LOW); //led low means gyro is connected properly
  //range settings
  mpu1.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu1.setGyroRange(MPU6050_RANGE_1000_DEG);
  mpu1.setFilterBandwidth(MPU6050_BAND_21_HZ);
  
  if (!mpu2.begin(0x69)) 
  {
    digitalWrite(LED_BUILTIN,HIGH); //built in led high means the arduino failed to recognize the gyro
    while (1) 
    {
      delay(10);
    }
  }
  else  digitalWrite(LED_BUILTIN,LOW); //led low means gyro is connected properly
  //range settings
  mpu2.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu2.setGyroRange(MPU6050_RANGE_1000_DEG);
  mpu2.setFilterBandwidth(MPU6050_BAND_21_HZ);
}
void tilt(float a_x,float a_y,float a_z,bool gyro)
{
  x_square1 = (a_x*a_x);
  y_square1 = (a_y*a_y);
  z_square1 = (a_z*a_z);
  result1 = sqrt(y_square1 + z_square1);
  result1 = x_val1/result1;
  accel_angle_x1 = atan(result1);
  //Serial.print(accel_angle_x1); Serial.print("   ");
  result1 = sqrt(x_square1 + z_square1);
  result1 = y_val1/result1;
  accel_angle_y1 = atan(result1);
  if(gyro) accel_angle_y1 = atan(result1);
  else accel_angle_y2 = atan(result1);
}
