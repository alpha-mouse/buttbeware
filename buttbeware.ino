#include <LowPower.h>
#include <SD.h>
#include <TMRpcm.h>
#include <SPI.h>

#define SDChipSelectPin 4
#define MusicPCMPin 9
#define RCLedPin 2
#define RelayPin 5
#define LightPin 6

#define TriggerDuration 300
#define TriggerFrequency 480
#define LightPhaseDuration 800

enum state_t {
  S_Initializing = 0,
  S_WaitingTrigger = 1,
  S_TriggerCandidate = 2,
  S_Triggered = 3,
  S_Playing = 4,
};

const char music[] = "mrm.wav";
const long MusicLength = 34500;
TMRpcm tmrpcm;
volatile byte currentState = S_Initializing;
volatile long lastStateChangeTime;
volatile int pulsesReceived = 0;


void setup(){

  tmrpcm.speakerPin = MusicPCMPin;

  Serial.begin(115200);
  if (!SD.begin(SDChipSelectPin)) {
    Serial.println("SD fail");
    return;
  } else {
    Serial.println("SD ok");
  }

  pinMode(RelayPin, OUTPUT);
  pinMode(LightPin, OUTPUT);
  pinMode(RCLedPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(RCLedPin), rcSignal, RISING);

  Serial.println("Startup ok");
  goToState(S_WaitingTrigger);
  sleep();
}


void loop(){  
  long duration;

  switch(currentState) {
    case S_WaitingTrigger:
      sleep();
      break;
    case S_TriggerCandidate:
      duration = millis() - lastStateChangeTime;
      if (duration < TriggerDuration) {
        break;
      } else {
        if (pulsesReceived < (long)TriggerFrequency * duration / 1000 / 2){
          // false positive
          goToState(S_WaitingTrigger);
          sleep();
        } else {
          goToState(S_Triggered);
        }
        break;
      }
    case S_Triggered:
      digitalWrite(RelayPin, HIGH);
      tmrpcm.play(music);
      goToState(S_Playing);
      break;
    case S_Playing:
      duration = millis() - lastStateChangeTime;
      if (MusicLength < duration) {
        digitalWrite(RelayPin, LOW);
        digitalWrite(LightPin, LOW);
        digitalWrite(MusicPCMPin, LOW);
        goToState(S_WaitingTrigger);
        sleep();
        break;
      } else {
        digitalWrite(LightPin, duration / LightPhaseDuration % 2 == 0 ? HIGH : LOW);
        break;
      }
  }
}

void sleep() {
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF); 
}

void goToState(state_t state) {
  currentState = state;
  lastStateChangeTime = millis();
}

void rcSignal(){
  if (currentState == S_WaitingTrigger) {
    goToState(S_TriggerCandidate);
    pulsesReceived = 1;
  } else if (currentState == S_TriggerCandidate) {
    pulsesReceived++;
  }
}

