#include <IridiumSBD.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <SD.h>
#include <StreamUtils.h>

#define SCALE1 Serial
#define SCALE2 Serial1
#define SCALE3 Serial2
#define SCALE4 Serial3

#define SCALE4_MODE 7 // Low = receive | High = transmit

#define UTRIG_PIN 4
#define UECHO_PIN 5

#define SCALE_POWER_PIN 6

#define DIAGNOSTICS false // Change this to see diagnostics
#define IRIDIUM_RX 2
#define IRIDIUM_TX 3

SoftwareSerial IridiumSerial(IRIDIUM_RX, IRIDIUM_TX);
IridiumSBD modem(IridiumSerial);

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

int scale_values = 0;

char buffer[100];
char json_buffer[100];

File config_file;
const char *config_name = "/config.txt";

struct Config {
  long scale1_offset;
  long scale2_offset;
  long scale3_offset;
  long scale4_offset;

  float scale1_slope;
  float scale2_slope;
  float scale3_slope;
  float scale4_slope;
};

Config config;

void setup() {
  Serial.begin(115200);
  SCALE4.begin(4800);
//  ScaleSerial.begin(9600);

//  while (!SD.begin()) continue;

  pinMode(UTRIG_PIN, OUTPUT);
  pinMode(UECHO_PIN, INPUT);
  pinMode(SCALE_POWER_PIN, OUTPUT);
  pinMode(SCALE4_MODE, OUTPUT);
  digitalWrite(SCALE4_MODE, HIGH);

  setConfig();
  
//  startRockBlock();
//  testSignalQuality();
//  while(!ScaleSerial);

//  req_scale();

//  size_t bufferSize = sizeof(buffer);
//  sendBinary(buffer, bufferSize);
}

void loop() {
  

    
//  while (ScaleSerial.available() && !msg_complete) {
//    char c = ScaleSerial.read();
//    buf[bytes_read] = c;
//    bytes_read++;
//    
//    if (c == end_char) {
//      int message_size = add_to_msg(buf, data_len);
//      float val = atof(buf);
//      bytes_read = 0;
//      scale_values++;
//      if (scale_values < 3) {
//        req_scale();
//      } else {
//          msg_complete = true;
//  //        for (int i=0; i<sizeof(message); i++) {
//  //          if (message[i] == '\0') {
//  //            msg_size = i;
//  //          }
//  //        }
//          Serial.println(message);
//          Serial.print("Length: ");
//          Serial.println(message_size);
//          Serial.println();
//          Serial.println("Press enter to send with RockBLOCK");
//          while (Serial.available() == 0) {};
//          while (Serial.available()) {
//            Serial.read();
//          }
//
//          Serial.println("Getting signal quality");
//          Serial.println();
//          testSignalQuality();
//          
//          Serial.println("Press enter again to confirm");
//          while (Serial.available() == 0) {};
//          while (Serial.available()) {
//            Serial.read();
//          }
//          
//          Serial.println("Sending message");
//          sendBinary(message, message_size);
//      }
//    }
//  }
//  int val = read_distance();
//  itoa(val, distance_buf, 10);
//  for (int i=0; i<dst_buf_len; i++) {
//    Serial.print(distance_buf[i], DEC);
//    Serial.print(" ");
//  }Serial.println();
////  Serial.println(distance_buf);
//  delay(1000);

//  for (int i=0; i<sizeof(message); i++) {
//    Serial.print(message[i], DEC);
//    Serial.print(" ");
//  } Serial.println();
//  delay(5000);
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

//void req_scale() {
//    ScaleSerial.write("send\r\n", 6);
//}

int add_to_msg(char a[], int len) {
  int msg_i;
  for (msg_i=0; msg_i<sizeof(message); msg_i++) {
    if (message[msg_i] == '\0') {
      message[msg_i] = ';';
      break;
    }
  }

  int arr_i;
  for (arr_i=0; arr_i<len; arr_i++) {
    if (a[arr_i] == '\n') {
      break;
    } else {
      message[msg_i+arr_i+1] = a[arr_i];
    }
  }
//  Serial.println(message);
//  Serial.println(message[msg_i + arr_i]);
  return msg_i + arr_i;
}

int read_distance () {
  digitalWrite(UTRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(UTRIG_PIN, HIGH); 
  delayMicroseconds(10);
  digitalWrite(UTRIG_PIN, LOW);
  int duration = pulseIn(UECHO_PIN, HIGH);
  return round(duration / 74 / 2); // In inches
}

void format_data(char scale1[], char scale2[], char scale3[], char scale4[]) {
  json_buffer[0] = '{';
  
}

void saveConfig() {
  SD.remove(config_name);

  config_file = SD.open(config_name);
  if (!config_file) {
    Serial.println("Failed to create file");
    return;
  }

  StaticJsonDocument<64> doc;

  doc["scale1"]["offset"] = config.scale1_offset;
  doc["scale1"]["slope"] = config.scale1_slope;

  doc["scale2"]["offset"] = config.scale2_offset;
  doc["scale3"]["slope"] = config.scale2_slope;

  doc["scale3"]["offset"] = config.scale3_offset;
  doc["scale3"]["slope"] = config.scale4_slope;

  doc["scale4"]["offset"] = config.scale4_offset;
  doc["scale4"]["offset"] = config.scale4_slope;
}

void loadConfig() {
  
}

void setConfig() {
  digitalWrite(SCALE4_MODE, HIGH);
  StaticJsonDocument<16> doc;

  doc["command"] = "send";

  serializeJson(doc, SCALE4);
  SCALE4.flush(); // Waits for command to finish transmitting
  digitalWrite(SCALE4_MODE, LOW);
  delay(50);
  
  readData();
//
//  bool scale4_done = false;
//  while (!scale4_done) {
//    StaticJsonDocument<32> doc;
//    readSerial(doc);
//  }
}

void readData() {
  StaticJsonDocument<32> doc;
  
  while (!SCALE4.available()) continue;
  bool done = false;
  while (!done) {
    while(SCALE4.available()) {
      char c = SCALE4.read();
      Serial.print(c);
      Serial.print(" ");
      if (c == '}') {
        done = true;
      }
    }
  }
  
//  ReadLoggingStream loggingStream(SCALE4, Serial);
//  DeserializationError err = deserializeJson(doc, loggingStream);

//  if (err == DeserializationError::Ok) {
//    const char* status = doc["status"];
//    handleStatus(status);
//  } else {
//    Serial.println("Serial read error");
//  }
}

void handleStatus(char status[]) {
  Serial.print("Scale: ");
  if (strstr(status, "ok")) {
    Serial.println("ok");
  } else if (strstr(status, "error")) {
    Serial.println("error");
  } else if (strstr(status, "timeout")) {
    Serial.println("timeout");
  }
}

void powerScales(bool power) {
  if (power) {
    digitalWrite(SCALE_POWER_PIN, HIGH);
  } else {
    digitalWrite(SCALE_POWER_PIN, LOW);
  }
}
