#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <EMailSender.h>
#include <StreamUtils.h>

#define DEBUG false;

const char* ssid = "CLZuehlke";
const char* password = "6083487392";
 
uint8_t connection_state = 0;
uint16_t reconnect_interval = 10000;
 
EMailSender emailSend("snowmelt2022@gmail.com", "uhamanaudrjqzqtn"); // This is an app password because google changed its security settings
EMailSender::EMailMessage message;

const char* arrayOfEmail[] = {"kohlerzuehlke@gmail.com", "johnmzuehlke@yahoo.com", "polebitskia@uwplatt.edu"};

uint8_t WiFiConnect(const char* nSSID = nullptr, const char* nPassword = nullptr)
{
    static uint16_t attempt = 0;
//    Serial.print("Connecting to ");
    if(nSSID) {
        WiFi.begin(nSSID, nPassword);
//        Serial.println(nSSID);
    }
 
    uint8_t i = 0;
    while(WiFi.status()!= WL_CONNECTED && i++ < 50)
    {
        delay(200);
//        Serial.print("."); 
    }
    ++attempt;
//    Serial.println("");
    if(i == 51) {
//        Serial.print("Connection: TIMEOUT on attempt: ");
//        Serial.println(attempt);
//        if(attempt % 2 == 0)
//            Serial.println("Check if access point available or SSID and Password\r\n");
        return false;
    }
//    Serial.println("Connection: ESTABLISHED");
//    Serial.print("Got IP address: ");
//    Serial.println(WiFi.localIP());
    return true;
}
 
void Awaits()
{
    uint32_t ts = millis();
    while(!connection_state)
    {
        delay(50);
        if(millis() > (ts + reconnect_interval) && !connection_state){
            connection_state = WiFiConnect();
            ts = millis();
        }
    }
}

void handleMessage() {
  if (Serial.available() > 5) {
    StaticJsonDocument<256> doc;  // ESP8266 needs double the memory of the Mega
//    ReadLoggingStream logging(Serial, Serial);
    DeserializationError err = deserializeJson(doc, Serial);

    if (err == DeserializationError::Ok) {
      char buffer[256];
      serializeJson(doc, buffer);
      sendMessage(buffer);
    } else if (err == DeserializationError::EmptyInput) {
      // This error is expected b/c deserialze doesn't read the last null byte
//      Serial.println("Empty input");
    } else {
//      Serial.println("Deserialization error");
    }
  }
}

void sendMessage(const char *arr) {
  message.message = arr;
  EMailSender::Response resp = emailSend.send(arrayOfEmail, 3, message);
  if (resp.code.equals("0")) {
    char stat_buf[32];
    StaticJsonDocument<32> stat;
    stat["status"] = "OK";
    serializeJson(stat, stat_buf);
    Serial.write(stat_buf);
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(10000);

  message.subject = "Snowmelt Data";
 
  connection_state = WiFiConnect(ssid, password);
  if(!connection_state)  // if not connected to WIFI
      Awaits();          // constantly trying to connect
}

void loop() {
  Awaits();          // constantly checks connection
  handleMessage();
  delay(1000);
}
