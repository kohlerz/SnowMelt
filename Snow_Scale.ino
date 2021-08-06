#include "HX711.h"
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <StreamUtils.h>

#define EXIT_FAILURE -1
#define EXIT_SUCCESS 0
#define READ_SIZE 10
#define MIN_READINGS 8
#define TEST_WEIGHT 15
#define MAX_DEVIATION 250
#define SEND_LEN 8
#define RECV_LEN 6
#define LINK_MODE 10 // Low = receive | High = transmit
#define LINK_RX 11
#define LINK_TX 12

long dummy_value = 180000;

int clean_size = 0;
const int DOUT_PIN = 3;
const int SCK_PIN = 2;
HX711 scale;

long offset = -227491;
long weighted_value = 0;
float slope = 0.82;

//char buf[RECV_LEN];
const char end_char = '}';
char float2str_buf[SEND_LEN];
long dirty_readings[READ_SIZE];
int bytes_read = 0;
bool recv_done = false;
const int input_max = 64;
char buffer[input_max];
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
  digitalWrite(LINK_MODE, LOW); // Receive
  link.begin(4800);

  // Enable HX711
//  scale.begin(DOUT_PIN, SCK_PIN);

  readSerial();

  // Run calibration (mostly for testing)
//  calibrate();
//  Serial.println("Press enter at any time to recalibrate");
}

void loop() {
//  Serial.println(scale.read());

//  float weight = (read_scale()-offset) * slope / 10000.0;
//  Serial.println(weight);
}

// The first four values are always wrong for some reason
long read_scale() {
//  long start_time = millis();
//  bool done = false;
//  long total = 0;
//  int num = 0;
//
//  while (!done || (millis() - start_time > 5000)) {
//    // Make sure the scale is stable
//    for (int i=0; i<10; i++) {scale.read();};
//    
//    for (int i=0; i<READ_SIZE; i++) {
//      dirty_readings[i] = scale.read();
//    }
//  
//    long average = avg(dirty_readings, READ_SIZE);
//    for (int i=0; i<READ_SIZE; i++) {
//      if (abs(dirty_readings[i] - average) < MAX_DEVIATION) {
//        total += dirty_readings[i];
//        num++;
//      }
//    }
//
//    if (num >= MIN_READINGS) {
//      done = true;
//      clean_size = num;
//      return total/num;
//    }
//  }
//
  // Only returns on timeout
//  return null;
  return dummy_value;
}

long avg(long a[], int len) {
  long sum = 0;

  for (int i=0; i<len; i++) {
    sum += a[i];
  }

  return round(sum / len);
}

void calibrate() {
  Serial.print("Calibrating.....................");
  scale.power_down();
  delay(500);
  scale.power_up();
  delay(500);

  offset = read_scale();

  Serial.println(offset);
//  bubble_sort();
//  clean_data();

//  for (int i=0; i<READ_SIZE; i++) {
//    Serial.print(dirty_readings[i]);
//    Serial.print(" ");
//  }
//  Serial.println(clean_size);

//  offset = value;
//
//  Serial.println(offset);
  char cal_buf[32];
  sprintf(cal_buf, "Calibrating %i lbs...............", TEST_WEIGHT);
  Serial.print(cal_buf);

  while (Serial.available() == 0) {};
  Serial.read();

  long weighted = read_scale();
//  bubble_sort();
//  clean_data();
//  weighted_value = avg(clean_readings, clean_size);

  slope = (TEST_WEIGHT * 10000.0) / (weighted - offset);
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
        offset = read_scale();
        Serial.println("Scale tared");
      } else {
        Serial.println(calibration_weight);
        slope = (calibration_weight * 10000.0) / (read_scale() - offset);
        Serial.print("New slope: ");
        Serial.println(slope);
      }
      
    }
    bytes_read++;
  }
}

int send_current_weight() {
  float weight = (read_scale()-offset) * slope / 10000.0;
  if (weight > 999.99) {
    Serial.println("Scale value impossible");
    return -1;
  } else {
    float2str(weight); // Restult stored in float2str_buf
    Serial.println(float2str_buf);
    link.write(float2str_buf, SEND_LEN);
    return 0;
  }
}

bool arr_matches(char arrA[], int lenA, char arrB[], int lenB) {
  if (lenA != lenB) {
    return false;
  }

  for (int i=0; i<lenA; i++) {
    if (arrA[i] != arrB[i]) {
      return false;
    }
  }

  return true;
}

void float2str(float num) {
  Serial.println(num);
  dtostrf(num, SEND_LEN-2, 2, float2str_buf);
  float2str_buf[SEND_LEN-2] = '\0';
  float2str_buf[SEND_LEN-1] = '\n';
  if (float2str_buf[1] == '-') { // Equal to a space
    float2str_buf[0] = '-';
    float2str_buf[1] = '0';
  } else if(float2str_buf[0] == ' ') {
    float2str_buf[0] = '0';
  }
}

void readSerial() {
  digitalWrite(LINK_MODE, LOW);
  StaticJsonDocument<32> doc;

  while(!link.available()) continue;
  
  ReadLoggingStream loggingStream(link, Serial);
  DeserializationError err = deserializeJson(doc, loggingStream);

  if (err == DeserializationError::Ok) {
    Serial.println("OK");
    const char* command = doc["command"];
    handleCommand(command);
  } else {
    sprintf(data.status, "error");
    Serial.println("Serial read error");
  }
}

void handleCommand(char cmd[]) {
  if (strstr(cmd, "send")) {
    data.value = read_scale();
    if (data.value) {
      sprintf(data.status, "ok");
    } else {
      sprintf(data.status, "timeout");
    }

    sendData();
  }
}

// Data is expected to be correct by this point
void sendData() {
  digitalWrite(LINK_MODE, HIGH);
  StaticJsonDocument<32> doc;
  doc["value"] = data.value;
  doc["status"] = data.status;

  delay(1);

  serializeJson(doc, link);
  link.flush();
}
