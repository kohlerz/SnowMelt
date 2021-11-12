#include "HX711.h"
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <StreamUtils.h>

#define EXIT_FAILURE -1
#define EXIT_SUCCESS 0
#define READ_SIZE 10
#define MIN_READINGS 8
#define MAX_DEVIATION 250
#define SEND_LEN 8
#define RECV_LEN 6
#define LINK_MODE 10 // Low = receive | High = transmit
#define LINK_RX 11
#define LINK_TX 12
#define MODE_SEND HIGH
#define MODE_RECV LOW

long dummy_value = 180000;

int clean_size = 0;
const int DOUT_PIN = 2;
const int SCK_PIN = 3;
HX711 scale;

long offset = -227491;
long weighted_value = 0;
float slope = 0.82;
int calibration_weight = 0;

//char buf[RECV_LEN];
const char end_char = '}';
char float2str_buf[SEND_LEN];
long dirty_readings[READ_SIZE];
int bytes_read = 0;
bool recv_done = false;
const int input_max = 64;
int scale_tries = 0;
const int MAX_SCALE_TRIES = 10;
bool scale_timeout = false;

SoftwareSerial link(12, 11); // RX, TX

char send_req[RECV_LEN] = {'s','e','n','d','\r','\n'};

struct Data {
  long value;
  char status[8];
};

Data data;

void setup() {
  // Start serial connection for debug
  Serial.begin(115200);

  // Setup serial link to master
  pinMode(LINK_MODE, OUTPUT);
  link.begin(9600);
  link.setTimeout(5000);

//  readCommand();

  // Enable HX711
  scale.begin(DOUT_PIN, SCK_PIN);
  while(!scale.is_ready());

  readScale();
  sendData();

  // Run calibration (mostly for testing)
//  calibrate();
//  Serial.println("Press enter at any time to recalibrate");
}

void loop() {
  delay(5000);
  readScale();
  sendData();
}

// The first four values are always wrong for some reason
void readScale() {
  long start_time = millis();
  bool done = false;
  long total = 0;
  int num = 0;

  if (scale.is_ready()) {
    while (!done) {
      if ((millis() - start_time > 5000)) {
        sprintf(data.status, "timeout");
        break;
      }
  
      Serial.print(millis() - start_time);
      // Make sure the scale is stable
      for (int i=0; i<10; i++) {scale.read();};
      
      for (int i=0; i<READ_SIZE; i++) {
        dirty_readings[i] = scale.read();
      }
    
      long average = avg(dirty_readings, READ_SIZE);
      for (int i=0; i<READ_SIZE; i++) {
        if (abs(dirty_readings[i] - average) < MAX_DEVIATION) {
          total += dirty_readings[i];
          num++;
        }
      }
  
      if (num >= MIN_READINGS) {
        done = true;
        clean_size = num;
        sprintf(data.status, "ok");
        data.value = total/num;
      }
    }
  } else {
    Serial.println("HX711 not ready, trying again");
    delay(100);
    if (scale_tries < MAX_SCALE_TRIES) {
      scale_tries++;
      readScale();
    } else {
      data.value = 0;
      Serial.println("HX711 not responding");
    }
  }
}

long avg(long a[], int len) {
  long sum = 0;

  for (int i=0; i<len; i++) {
    sum += a[i];
  }

  return round(sum / len);
}

void calibrate() {
  StaticJsonDocument<16> doc;
  DeserializationError err = deserializeJson(doc, link);

  if (err == DeserializationError::Ok) {
    calibration_weight = doc["weight"];

  } else if (err == DeserializationError::EmptyInput) {
    // This error is expected b/c deserialze doesn't read the last null byte
    calibrate();
  } else {
    Serial.println("deserialization error");
  }

  char cal_buf[32];
  sprintf(cal_buf, "Calibrating %i lbs...............", calibration_weight);
  Serial.print(cal_buf);

  readScale();
//  bubble_sort();
//  clean_data();
//  weighted_value = avg(clean_readings, clean_size);

  slope = (calibration_weight * 10000.0) / (data.value - offset);
  Serial.print("Slope: ");
  Serial.println(slope);
}

void recalibrate() {
  char num_buf[4];
  int bytes_read = 0;
  int calibration_weight = -1;
  while (Serial.available()) {
    char c = Serial.read();
    num_buf[bytes_read] = c;   
    if (c == 10)  {
      num_buf[bytes_read] = '\0';
      calibration_weight = atoi(num_buf);
//      for (int i=0; i<sizeof(num_buf); i++) {
//        Serial.print(num_buf[i]);
//        Serial.print(" ");
//      }Serial.println();
      if (calibration_weight == 0) {
        readScale();
        Serial.println("Scale tared");
      } else {
        Serial.println(calibration_weight);
        readScale();
        slope = (calibration_weight * 10000.0) / (data.value - offset);
        Serial.print("New slope: ");
        Serial.println(slope);
      }
      
    }
    bytes_read++;
  }
}

void readCommand() {
  setSerialMode(MODE_RECV);
  StaticJsonDocument<32> doc;

  while(!link.available()){};

//  ReadLoggingStream loggingStream(link, Serial);
  DeserializationError err = deserializeJson(doc, link);
  link.read();

  if (err == DeserializationError::Ok) {
    const char* command = doc["command"];
    handleCommand(command);
  } else if (err == DeserializationError::EmptyInput) {
    // This error is expected b/c deserialze doesn't read the last null byte
    readCommand();
  } else {
//    Serial.println("other error");
  }
}

void handleCommand(char cmd[]) {
  if (strstr(cmd, "send")) {
    Serial.println("send");
    sendData();
  } else if (strstr(cmd, "calibrate")) {
    Serial.println("Calibrating..........");
    calibrate();
  }
}

// Data is expected to be correct by this point
void sendData() {
  setSerialMode(MODE_SEND);
  StaticJsonDocument<64> doc;
  doc["value"] = data.value;
  doc["status"] = data.status;

  WriteLoggingStream logging(link, Serial);
  serializeJson(doc, logging);
  delay(1);
}

void sendReady() {
  data.value = 0;
  sprintf(data.status, "ready");
  sendData();
}

void setSerialMode(int mode) {
  digitalWrite(LINK_MODE, mode);
  delay(1);
}

void flushSerial() {
  while (link.available()) {
    link.read();
  }
}
