#include <IridiumSBD.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <SD.h>
#include <StreamUtils.h>
#include <DS3232RTC.h>
#include "LowPower.h"
#include <OneWire.h>
#include <DallasTemperature.h>

#define TEMP_PIN 7

#define IridiumSerial Serial1
#define SCALE1 Serial2
#define SCALE2 Serial3

// Low = receive | High = transmit
#define SCALE1_MODE 3
#define SCALE2_MODE 4

#define SCALE1_POWER 5
#define SCALE2_POWER 6

#define UTRIG_PIN A2
#define UECHO_PIN A3

#define DIAGNOSTICS false // Change this to see diagnostics

#define MODE_SEND HIGH
#define MODE_RECV LOW

#define CHIP_SELECT 53

#define TIME_INTERVAL 12 // hours
#define WAKE_INTERRUPT_PIN 2

IridiumSBD modem(IridiumSerial);

OneWire oneWire(TEMP_PIN);
DallasTemperature temp_sensor(&oneWire);

int ultra_inches;

char message[80];
const int data_len = 8;
char buf[data_len];
int bytes_read = 0;
bool recv_done = false;
bool scale_responded = false;
const char end_char = '}';
const int dst_buf_len = 4;
char distance_buf[dst_buf_len];
bool msg_complete = false;
int calibration_weight = 15;

int scale_values = 0;

char buffer[99];
char json_buffer[100];

const char *config_name = "/config.txt";
const char *log_name = "/log.txt";

int log_num = 1;
bool sleep = false;

struct Config {
  long scale1_offset;
  long scale2_offset;

  float scale1_slope;
  float scale2_slope;

  float ultra_offset;
};

struct LogData {
  char time[32];
  float weight1;
  float weight2;
  float temp;
  float snow_depth;
};

Config config;
LogData logData;

void setup() {
  Serial.begin(115200);
  SCALE1.begin(9600);
  SCALE1.setTimeout(5000);
  SCALE2.begin(9600);
  SCALE2.setTimeout(5000);

  while (!SD.begin(CHIP_SELECT)) continue;
  
  pinMode(UTRIG_PIN, OUTPUT);
  pinMode(UECHO_PIN, INPUT);

  temp_sensor.begin();
  
  pinMode(SCALE1_POWER, OUTPUT);
  pinMode(SCALE2_POWER, OUTPUT);
  digitalWrite(SCALE1_POWER, LOW);
  digitalWrite(SCALE2_POWER, LOW);
  pinMode(SCALE1_MODE, OUTPUT);
  pinMode(SCALE2_MODE, OUTPUT);

  setSerialMode(MODE_RECV);

// Use this block of code to set time on RTC module
//  tmElements_t tm;
//  tm.Hour = 6;
//  tm.Minute = 40;
//  tm.Second = 10;
//  tm.Day = 26;
//  tm.Month = 8;
//  tm.Year = 2021 - 1970;
//  RTC.write(tm);

  // Clear all RTC alarms
  RTC.setAlarm(ALM1_MATCH_DATE, 0, 0, 0, 1);
  RTC.setAlarm(ALM2_MATCH_DATE, 0, 0, 0, 1);
  RTC.alarm(ALARM_1);
  RTC.alarm(ALARM_2);
  RTC.alarmInterrupt(ALARM_1, false);
  RTC.alarmInterrupt(ALARM_2, false);
  RTC.squareWave(SQWAVE_NONE);

//  pinMode(LED_BUILTIN, OUTPUT);
//  pinMode(WAKE_INTERRUPT_PIN, INPUT_PULLUP);
//  attachInterrupt(digitalPinToInterrupt(WAKE_INTERRUPT_PIN), onWake, FALLING);

  Serial.println("Type 'calibrate' at any time to calibrate the scales");

//  temp_sensor.requestTemperatures();
//  Serial.println(temp_sensor.getTempFByIndex(0));
  startRockBlock();

//  delay(60000);
  
  getData();
  sendViaRockBlock();
  setRTC();
//  testSignalQuality();

  
//  getData();
//  createTmpFile(1);
}

void loop() {
  handleCalibration();
  if (RTC.alarm(ALARM_1)) {
    Serial.println("I'm awake :)");
    getData();
    sendViaRockBlock();
    setRTC();
  }

  delay(1000);
}

#if DIAGNOSTICS
void ISBDConsoleCallback(IridiumSBD *device, char c)
{
  Serial.write(c);
}

void ISBDDiagsCallback(IridiumSBD *device, char c)
{
  Serial.write(c);
}
#endif

void startRockBlock () {
  int err;

  // Start the serial port connected to the satellite modem
  IridiumSerial.begin(19200);

  // Begin satellite modem operation
  Serial.println("Starting modem...");
  err = modem.begin();
  if (err != ISBD_SUCCESS)
  {
    Serial.print("Begin failed: error ");
    Serial.println(err);
    if (err == ISBD_NO_MODEM_DETECTED)
      Serial.println("No modem detected: check wiring.");
    return;
  }

  // Example: Print the firmware revision
  char version[12];
  err = modem.getFirmwareVersion(version, sizeof(version));
  if (err != ISBD_SUCCESS)
  {
     Serial.print("FirmwareVersion failed: error ");
     Serial.println(err);
     return;
  }
  Serial.print("Firmware Version is ");
  Serial.print(version);
  Serial.println(".");
}

void testSignalQuality() {
  // Test the signal quality.
  // This returns a number between 0 and 5.
  // 2 or better is preferred.
  int signalQuality = -1;
  int err = modem.getSignalQuality(signalQuality);
  if (err != ISBD_SUCCESS)
  {
    Serial.print("SignalQuality failed: error ");
    Serial.println(err);
    return;
  }

  Serial.print("On a scale of 0 to 5, signal quality is currently ");
  Serial.print(signalQuality);
  Serial.println(".");

//  if(signalQuality >= 2) {
//    getData();
//  }
}

void sendBinary(uint8_t buffer[], size_t bufferSize) {
  int err = modem.sendSBDBinary(buffer, bufferSize);
  if (err != ISBD_SUCCESS)
  {
    Serial.print("sendSBDBinary failed: error ");
    Serial.println(err);
    if (err == ISBD_SENDRECEIVE_TIMEOUT)
      Serial.println("Try again with a better view of the sky.");
  }

  else
  {
    Serial.println("Data sent!");
  }
}

float get_temp() {
  temp_sensor.requestTemperatures();
  return temp_sensor.getTempFByIndex(0);
}

float read_distance () {
  float sound_speed = 331.4 + 0.6 * get_temp();
  digitalWrite(UTRIG_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(UTRIG_PIN, HIGH); 
  delayMicroseconds(20);
  digitalWrite(UTRIG_PIN, LOW);
  float duration = pulseIn(UECHO_PIN, HIGH, 26000);
  return duration * (sound_speed / 25400) / 2; // In inches
}

void saveConfig() {
  SD.remove(config_name);

  File config_file = SD.open(config_name, FILE_WRITE);
  if (!config_file) {
    Serial.println("Failed to create config file");
    return;
  }

  StaticJsonDocument<256> doc;

  doc["scale1_offset"] = config.scale1_offset;
  doc["scale1_slope"] = config.scale1_slope;
  doc["scale2_offset"] = config.scale2_offset;
  doc["scale2_slope"]  = config.scale2_slope;

  doc["ultra_offset"] = config.ultra_offset;

//  WriteLoggingStream logging(config_file, Serial);
  int err = serializeJson(doc, config_file);
  if (err == 0) {
    Serial.println("Failed to write config file");
  }

  config_file.close();
}

float calculateWeight(int scale, long value) {
  File config_file = SD.open(config_name);

  StaticJsonDocument<128> doc;

//  ReadLoggingStream logging(config_file, Serial);
  DeserializationError err = deserializeJson(doc, config_file);
  if (err) {
    Serial.println("Error reading from config file");
    return;
  }

  switch(scale) {
    case 1:
      long scale1_offset = doc["scale1_offset"];
      float scale1_slope = doc["scale1_slope"];
      return ((value - scale1_offset) * scale1_slope) / 10000.0;
      
    case 2:
      long scale2_offset = doc["scale2_offset"];
      float scale2_slope = doc["scale2_slope"];
      return ((value - scale2_offset) * scale2_slope) / 10000.0;
  }

  config_file.close();
}

long getScale(int scale) {
  long value = 0;
  setSerialMode(MODE_RECV);

  for (int i=0; i<5; i++) {
    switch (scale) {
      case 1:
        digitalWrite(SCALE1_POWER, HIGH);
        value = readData(SCALE1);
        digitalWrite(SCALE1_POWER, LOW);
        break;
      case 2:
        digitalWrite(SCALE2_POWER, HIGH);
        value = readData(SCALE2);
        digitalWrite(SCALE2_POWER, LOW);
        break;
    }

    if (value !=0) {
      break;
    }
  }
  

  return value;
}

long readData(Stream &port) {
  // Allows 5 errors because it will occasionally throw an Empty Input Error
  for (int i=0; i<5; i++) {
    setSerialMode(MODE_RECV);
    
    StaticJsonDocument<64> doc;
    while(!port.available()){};
  
    ReadLoggingStream logging(port, Serial);
    DeserializationError err = deserializeJson(doc, logging);
  
    if (err == DeserializationError::Ok) {
      const char* status = doc["status"];
      long value = doc["value"];
      return value;
    } else if (err == DeserializationError::EmptyInput) {
      // This error is expected b/c deserialze doesn't read the last null byte
      Serial.println("Empty input");
    } else {
      Serial.println("Deserialization error");
    }
  }

  return 0;
  
}

void handleCalibration() {
  if (Serial.available()) {
    bool done = false;
    char buf[16];
    int bytes_read = 0;
    while (!done) {
      char c = Serial.read();
      buf[bytes_read] = c;
      bytes_read++;

      if (c == '\n') {
        done = true;
      }
    }

    while (Serial.available()) {
      Serial.read();
    }

    if (strstr(buf, "calibrate")) {
      Serial.println("Clear everything off of the scales and press enter when done");
    } else {
      Serial.println("Invalid command");
      while (Serial.available()) {Serial.read();};
      return;
    }

    while (!Serial.available()) {};
    while (Serial.available()) {
      Serial.read();
    }

    Serial.println("Taring, do not touch or move the scales");
    config.scale1_offset = getScale(1);
    config.scale2_offset = getScale(2);

    Serial.println(config.scale1_offset);
    Serial.println(config.scale2_offset);

    Serial.println("Done taring.");
    Serial.println("Put calibration weight onto a scale and enter the weight you used in pounds");
    
    while (!Serial.available()){};

    done = false;
    bytes_read = 0;
    float calibration_weight = 0;
    while (!done) {
      char c = Serial.read();
      buf[bytes_read] = c;
      bytes_read++;

      if (c == '\n') {
        done = true;
        buf[bytes_read] = '\0';
        calibration_weight = atoi(buf);
        Serial.print("Calibration weight: ");
        Serial.println(calibration_weight);
      }
    }

    long scale1_weighted = getScale(1);
    long scale2_weighted = getScale(2);

    config.scale1_slope = 0;
    config.scale2_slope = 0;
    if (abs(scale1_weighted - config.scale1_offset) > abs(scale2_weighted - config.scale2_offset)) {
      config.scale1_slope = (calibration_weight * 10000) / (scale1_weighted - config.scale1_offset);
      Serial.println("calibrated scale1");
    } else {
      config.scale2_slope = (calibration_weight * 10000) / (scale2_weighted - config.scale2_offset);
      Serial.println("calibrated scale2");
    }

    Serial.println("Calibrated one scale. Please move the weight to the other scale and press enter");
    while (!Serial.available());
    while (Serial.available()) {
      Serial.read();
    }

    while (!config.scale1_slope || !config.scale2_slope) {
      scale1_weighted = getScale(1);
      scale2_weighted = getScale(2);
      if (abs(scale1_weighted - config.scale1_offset) > abs(scale2_weighted - config.scale2_offset)) {
        if (config.scale1_slope == 0) {
          config.scale1_slope = (calibration_weight * 10000) / (scale1_weighted - config.scale1_offset);
          Serial.println("calibrated scale1");
        } else {
          Serial.println("It doesn't seem like you moved the weight, please move it now");
        }
      } else {
        if (config.scale2_slope == 0) {
          config.scale2_slope = (calibration_weight * 10000) / (scale2_weighted - config.scale2_offset);
          Serial.println("calibrated scale2");
        } else {
          Serial.println("It doesn't seem like you moved the weight, please move it now");
        }
      }
      delay(1000);
    }

    Serial.print("scale1_offset: ");
    Serial.println(config.scale1_offset);
    Serial.print("scale1_slope: ");
    Serial.println(config.scale1_slope);

    Serial.print("scale2_offset: ");
    Serial.println(config.scale2_offset);
    Serial.print("scale2_slope: ");
    Serial.println(config.scale2_slope);

    Serial.println("Make sure there is nothing blocking the ultrasonic sensor, then press enter");
    while (!Serial.available());
    while (Serial.available()) {
      Serial.read();
    }

    float total = 0;
    for (int i=0; i<5; i++) {
      total += read_distance();
    }
    config.ultra_offset = total/5;
    Serial.print("Ultrasonic offset: ");
    Serial.println(config.ultra_offset);

    saveConfig();

    for (int i=0; i<5; i++) {
      Serial.print("Scale 1: ");
      Serial.println(calculateWeight(1, getScale(1)));
      Serial.print("Scale 2: ");
      Serial.println(calculateWeight(2, getScale(2)));
      Serial.print("Snow depth: ");
      Serial.println(config.ultra_offset - read_distance());
      delay(1000);
    }
  }
}

void setSerialMode(int mode) {
  digitalWrite(SCALE1_MODE, mode);
  digitalWrite(SCALE2_MODE, mode);
  delay(1);
}

void setRTC() {
  time_t t;
  t = RTC.get();
  int new_time = (hour(t) + TIME_INTERVAL) % 24;
  RTC.setAlarm(ALM1_MATCH_HOURS, 0, 0, new_time, 0);
  Serial.print("Sending at hour: ");
  Serial.println(new_time);
  RTC.alarm(ALARM_1);
//  RTC.squareWave(SQWAVE_NONE);
//  RTC.alarmInterrupt(ALARM_1, true);
}

// Doesn't behave well with Serial, so I do all processing outside of this function
void onWake() {
//  Serial.println("I'm awake :)");
//  Serial.println("Gathering data, one moment...");
//  getData();
//  createTmpFile(log_num);
//
//  if (log_num < 4) {
//    Serial.println("Saved data to temp file");
//    Serial.print("Sending to RockBlock in ");
//    Serial.print(4-log_num);
//    Serial.println(" iteration(s)");
//    log_num++;
//  } else {
//    log_num = 1;
//    Serial.println("Sending to RockBLOCK");
//    delay(1000);
//    Serial.println("Done!");
//  }
//  setRTC();
}

void goToSleep() {
  sleep = true;
}

void getData() {
  time_t t = RTC.get();
  char buf[128];
  int time_size = sprintf(buf, "%02d:%02d:%02d", hour(t), minute(t), second(t));
  memmove(logData.time, buf, time_size+1);
  logData.weight1 = calculateWeight(1, getScale(1));
  logData.weight2 = calculateWeight(2, getScale(2));
  logData.temp = get_temp();
  logData.snow_depth = config.ultra_offset - read_distance();
}

void createTmpFile() {
  char file_buf[16];
  sprintf(file_buf, "/tmp%d.txt", log_num);
  SD.remove(file_buf);
  File tmp_file = SD.open(file_buf, FILE_WRITE);
  if (!tmp_file) {
    Serial.println("Failed to open tmp file");
  }

  StaticJsonDocument<128> doc;
  doc["time"]       = logData.time;
  
  char weight1_buf[8];
  dtostrf(logData.weight1, 4, 3, weight1_buf);
  doc["weight1"]    = weight1_buf;
  
  char weight2_buf[8];
  dtostrf(logData.weight2, 4, 3, weight2_buf);
  doc["weight2"]    = weight2_buf;
  
  char temp_buf[8];
  dtostrf(logData.temp, 4, 3, temp_buf);
  doc["temp"]    = weight1_buf;
  
  char snow_buf[8];
  dtostrf(logData.snow_depth, 4, 3, snow_buf);
  doc["snow_depth"]    = snow_buf;

  int err = serializeJson(doc, tmp_file);
  if (err == 0) {
    Serial.println("Failed to write tmp file");
  }

  tmp_file.close();
}

void sendViaRockBlock() {
  StaticJsonDocument<100> doc;

  time_t t = RTC.get();
  char buf[64];
  sprintf(buf, "%d-%02d-%02dT%02d:%02d:%02dZ", year(t), month(t), day(t), hour(t), minute(t), second(t));
  doc["time"] = buf;
  
  char weight1_buf[8];
  dtostrf(logData.weight1, 4, 3, weight1_buf);
  doc["1"] = weight1_buf;
  
  char weight2_buf[8];
  dtostrf(logData.weight2, 4, 3, weight2_buf);
  doc["2"] = weight2_buf;
  
  char temp_buf[8];
  dtostrf(logData.temp, 4, 3, temp_buf);
  doc["tmp"] = temp_buf;
  
  char snow_buf[8];
  dtostrf(logData.snow_depth, 4, 3, snow_buf);
  doc["depth"]    = snow_buf;
  
  serializeJson(doc, buffer, 98);
  Serial.println(sizeof(buffer));
  Serial.println(buffer);

  size_t bufferSize = sizeof(buffer);
//  testSignalQuality();
  sendBinary(buffer, 98);
  
//  for (int i=1; i<5; i++) {
//    StaticJsonDocument<128> tmp_doc;
//    char file_buf[16];
//    sprintf(file_buf, "/tmp%d.txt", i);
//    File tmp_file = SD.open(file_buf, FILE_READ);
//    if (!tmp_file) {
//      Serial.println("Failed to open tmp file");
//    }
//
//    DeserializationError err = deserializeJson(tmp_doc, tmp_file);
//    if (err) {
//      Serial.println("Error reading from tmp file");
//      return;
//    }
//
//    char reading_buf[4];
//    sprintf(reading_buf, "%d", i);
//    JsonObject tmp_data = doc.createNestedObject(reading_buf);
////    const char* time_p  = tmp_doc["time"];
//
//    tmp_data["time"]       = tmp_doc["time"];
//    tmp_data["weight1"]    = tmp_doc["weight1"];
//    tmp_data["weight2"]    = tmp_doc["weight2"];
//    tmp_data["temp"]       = tmp_doc["temp"];
//    tmp_data["snow_depth"] = tmp_doc["snow_depth"];
//
//      tmp_file.close();
////      tmp_data.clear();
//      tmp_doc.clear();
//  }
  
}

void writeLog() {
//
////  File log_file = SD.open(log_name, FILE_WRITE);
////  if (!log_file) {
////    Serial.println("Failed to open log file");
////    return;
////  }
//
//  char buf[256];
//  sprintf(buf, "%d-%02d-%02dT%02d:%02d:%02dZ", year(t), month(t), day(t), hour(t), minute(t), second(t));
//  Serial.println(buf);
//
////  log_file.close();
}
