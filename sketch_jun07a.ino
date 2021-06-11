#include "HX711.h"

// Must be an even number apart!
#define DIRTY_SIZE 60
#define CLEAN_SIZE 50

const int DOUT_PIN = 2;
const int SCK_PIN = 3;

HX711 scale;

long offset = 0;
long weighted_value = 0;
float slope = 0.0;

long dirty_readings[DIRTY_SIZE];
long clean_readings[CLEAN_SIZE];
long normalized_readings[CLEAN_SIZE];

int diff = (DIRTY_SIZE - CLEAN_SIZE) / 2;

void setup() {
  Serial.begin(9600);

  scale.begin(DOUT_PIN, SCK_PIN);

  calibrate();
}

void loop() {
  // put your main code here, to run repeatedly:
  scale.power_up();

  read_scale();
  bubble_sort();
  clean_data();
  normalize_data();
  Serial.println((avg(normalized_readings) * slope) / 10000.0);

//  for (int i=0; i<CLEAN_SIZE; i++) {
//    Serial.print(clean_readings[i] - offset);
//    if (i != CLEAN_SIZE-1){
//      Serial.print(',');
//    } else {
//      Serial.println();
//    }
//  }

  scale.power_down();
  delay(5000);
}

long calc_offset() {
  return avg(clean_readings);
}

void read_scale() {
  for (int i=0; i<DIRTY_SIZE; i++) {
    dirty_readings[i] = scale.read(); // Returns raw scale value
  }
}

void bubble_sort() {
  for (int i=0; i<DIRTY_SIZE-1; i++) {
    for (int j=0; j<DIRTY_SIZE-i-1; j++) {
      if(dirty_readings[j] > dirty_readings[j+1]) {
        long t = dirty_readings[j];
        dirty_readings[j] = dirty_readings[j+1];
        dirty_readings[j+1] = t;
      }
    }
  }
}

void clean_data() {
  for (int i=diff; i<DIRTY_SIZE-diff; i++) {
    clean_readings[i-diff] = dirty_readings[i];
  }
}

void normalize_data() {
  for (int i=0; i<CLEAN_SIZE; i++) {
    normalized_readings[i] = clean_readings[i] - offset;
  }
}

long avg(long a[]) {
  long sum = 0;
  int len = sizeof(a) / sizeof(int);

  for (int i=0; i<len; i++) {
    sum += a[i];
  }

  return round(sum / len);
}

void calibrate() {
  Serial.print("Calibrating.....................");
  scale.power_down();
  delay(5000);
  scale.power_up();
  delay(5000);

  read_scale();
  bubble_sort();
  clean_data();

  offset = calc_offset();

  Serial.println(offset);
  Serial.print("Calibrating 15lbs...............");

  while (Serial.available() == 0) {};

  read_scale();
  bubble_sort();
  clean_data();
  weighted_value = avg(clean_readings);

  slope = 150000.0 / (weighted_value - offset);
}
