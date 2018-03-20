#include <SD.h>
#include <TMRpcm.h>

#define SDChipSelectPin 4
#define MusicPCMPin 9
#define LightPin 6
#define BatteryVoltageDividerPin 5
#define LightPhaseDuration 800

const char music[] = "mrm.wav";
const long MusicDuration = 34500;
TMRpcm tmrpcm;
long startTime;

void setup() {

  Serial.begin(115200);

  tmrpcm.speakerPin = MusicPCMPin;

  if (!SD.begin(SDChipSelectPin)) {
    message("fail SD");
    return;
  } else {
    message("ok SD");
  }

  pinMode(LightPin, OUTPUT);
  pinMode(BatteryVoltageDividerPin, OUTPUT);
  digitalWrite(BatteryVoltageDividerPin, HIGH);

  tmrpcm.play(music);
  startTime = millis();

  message("Startup ok");
}

void loop() {
  long duration = millis() - startTime;
  if (MusicDuration < duration) {
    digitalWrite(LightPin, LOW);
    digitalWrite(MusicPCMPin, LOW);
    while (true) ; // wait for death
  } else {
    digitalWrite(LightPin, duration / LightPhaseDuration % 2 == 0 ? HIGH : LOW);
  }
}

void message(char str[]) {
  return;
  Serial.println(str);
  Serial.flush();
}

void message(char str[], int i) {
  return;
  Serial.print(str);
  Serial.println(i);
  Serial.flush();
}

void message(long i) {
  return;
  Serial.println(i);
  Serial.flush();
}

