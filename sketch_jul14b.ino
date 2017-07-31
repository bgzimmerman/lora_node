// Include the libraries we need
#include <OneWire.h>
#include <DallasTemperature.h>
#include "SX1272.h"
#include <LowPower.h>
#include "SparkFunBME280.h"

//*********************************************************************************************
#define BATT_MONITOR_EN A3 //enables battery voltage divider to get a reading from a battery, disable it to save power
#define BATT_MONITOR  A7   //through 1Meg+470Kohm and 0.1uF cap from battery VCC - this ratio divides the voltage to bring it below 3.3V where it is scaled to a readable range
#define BATT_CYCLES   1    //read and report battery voltage every this many sleep cycles (ex 30cycles * 8sec sleep = 240sec/4min. For 450 cyclesyou would get ~1 hour intervals
#define BATT_FORMULA(reading) reading * 0.00322 * 1.475  // >>> fine tune this parameter to match your voltage when fully charged
unsigned short idlePeriod = 35; // >>> 1 idle period is equal to about 8 seconds
//*****************************************************************************************************************************



// Definitions for the Radio
#define LORAMODE 1
#define DEFAULT_DEST_ADDR 1
#define NODE_ADDR 38
#define MAX_DBM 14
const uint32_t DEFAULT_CHANNEL = CH_05_900;

#define numBusses 4
BME280 bme280;

float tempC;
uint8_t message[100];
char str1[10], str2[10], str3[10], str4[10];
uint8_t app_key_offset = 0;
uint8_t c_size;
int pl;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  sx1272.ON();
  int loraMode = LORAMODE;
  sx1272.setMode(loraMode);
  //sx1272._enableCarrierSense = true;
  sx1272.setChannel(DEFAULT_CHANNEL);
  sx1272._needPABOOST = true;
  sx1272.setPowerDBM((uint8_t)MAX_DBM);
  sx1272.setNodeAddress(NODE_ADDR);

}

int i;
unsigned long nextTransmissionTime = 0;
int counter = 0;
void loop() {
  if (millis() > nextTransmissionTime) {
    for (i=4; i<4 + numBusses; i++) {
      sendBusTemp(i);
    }
    sendWeatherData();
    if (counter < BATT_CYCLES) {
    counter++;
    }
    else {
      readBattery();
      counter = 0;
    }
    nextTransmissionTime = millis() + idlePeriod;
    
  }
  else {
    delay(1);
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  }

}

void sendBusTemp(int busPin) {

  OneWire oneWire(busPin);
  DallasTemperature sensors(&oneWire);
  DeviceAddress one, two, three, four;
  sensors.begin();
  oneWire.search(one);
  oneWire.search(two);
  oneWire.search(three);
  oneWire.search(four);
  
  sensors.requestTemperatures();
  
  tempC = sensors.getTempC(one);
  dtostrf(tempC, 3, 2, str1);
  tempC = sensors.getTempC(two);
  dtostrf(tempC, 3, 2, str2);
  tempC = sensors.getTempC(three);
  dtostrf(tempC, 3, 2, str3);
  tempC = sensors.getTempC(four);
  dtostrf(tempC, 3, 2, str4); 
  
  c_size = sprintf((char*)message + app_key_offset, 
    "\\!%X%X%X%X%X%X%X%X/%s/%X%X%X%X%X%X%X%X/%s/%X%X%X%X%X%X%X%X/%s/%X%X%X%X%X%X%X%X/%s", 
    one[0],one[1],one[2],one[3],one[4],one[5],one[6],one[7], str1, 
    two[0],two[1],two[2],two[3],two[4],two[5],two[6],two[7], str2,
    three[0],three[1],three[2],three[3],three[4],three[5],three[6],three[7], str3,
    four[0],four[1],four[2],four[3],four[4],four[5],four[6],four[7], str4);
  Serial.println((char*)message + app_key_offset);
  pl = c_size + app_key_offset;
  sx1272.setPacketType(PKT_TYPE_DATA);
  sx1272.sendPacketTimeout(DEFAULT_DEST_ADDR, message, pl);
}

void sendWeatherData() {
  
  bme280.settings.runMode = 3;
  bme280.settings.tempOverSample = 1;
  bme280.settings.pressOverSample = 1;
  bme280.settings.humidOverSample = 1;
  bme280.begin(); delay(20);
 
  
  tempC = bme280.readTempC();
  dtostrf(tempC, 3, 2, str1);
  tempC = bme280.readFloatPressure();
  dtostrf(tempC, 3, 2, str2);
  tempC = bme280.readFloatHumidity();
  dtostrf(tempC, 3, 2, str3);  
  
  c_size = sprintf((char*)message + app_key_offset, 
    "\\!nodeTemp/%s/nodePressure/%s/nodeHumidity/%s", 
    str1, str2, str3);
  Serial.println((char*)message + app_key_offset);
  pl = c_size + app_key_offset;
  sx1272.setPacketType(PKT_TYPE_DATA);
  sx1272.sendPacketTimeout(DEFAULT_DEST_ADDR, message, pl);
  bme280.reset();
}

void readBattery()
{
  unsigned int readings=0;
  
  //enable battery monitor on WeatherShield (via mosfet controlled by A3)
  pinMode(BATT_MONITOR_EN, OUTPUT);
  digitalWrite(BATT_MONITOR_EN, LOW);

  for (byte i=0; i<5; i++) //take several samples, and average
    readings+=analogRead(BATT_MONITOR);
  
  //disable battery monitor
  pinMode(BATT_MONITOR_EN, INPUT); //highZ mode will allow p-mosfet to be pulled high and disconnect the voltage divider on the weather shield
    
  float batteryVolts = BATT_FORMULA(readings / 5.0);
  dtostrf(batteryVolts,3,2, str1); //update the BATStr which gets sent every BATT_CYCLES or along with the MOTION message
   c_size = sprintf((char*)message + app_key_offset, 
    "\\!battVoltage/%s", 
    str1);
  Serial.println((char*)message + app_key_offset);
  pl = c_size + app_key_offset;
  sx1272.setPacketType(PKT_TYPE_DATA);
  sx1272.sendPacketTimeout(DEFAULT_DEST_ADDR, message, pl);
}

