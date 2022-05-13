#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
Adafruit_MPU6050 mpu;

const unsigned int MAX_MESSAGE_LENGTH = 36;
static char message [MAX_MESSAGE_LENGTH];
static unsigned int message_pos = 0;
char inByte;
void setup() 
{
  // Begin the Serial at 9600 Baud
  
  Serial.begin(9600);
  delay(100);
}

void loop() 
{
  while(Serial.available() > 0)
  {
    inByte = Serial.read();
    if(inByte != '\n' && (message_pos < MAX_MESSAGE_LENGTH - 1))
    {
      message[message_pos] = inByte;
      message_pos++;
    }
    else
    {
      message[message_pos] = '\0';
      Serial.println(message);
      message_pos = 0;
    }
  }
}
