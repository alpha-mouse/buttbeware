#include <avr/io.h>

#include <avr/sleep.h>

#define TriggerDuration 300
#define TriggerFrequency 480
#define AlarmingOwnBatteryLevel 3600

enum state_t {
  S_Initializing = 0,
  S_WaitingTrigger = 1,
  S_TriggerCandidate = 2,
  S_Triggered = 3,
  S_Playing = 4,
};

const long TurnOnDuration = 39000; // really less, but attiny counts fast
volatile byte currentState = S_Initializing;
volatile long lastStateChangeTime;
volatile int pulsesReceived = 0;

volatile boolean ownBatteryLow = false;
volatile boolean otherBatteryLow = false;
volatile boolean batteryLowWarningLit = false;

// RC interrupt
ISR(INT0_vect)
{
  if (currentState == S_WaitingTrigger) {
    MCUCR |= _BV(ISC01); // switch to falling edge interruption
    goToState(S_TriggerCandidate);
    pulsesReceived = 1;
  } else if (currentState == S_TriggerCandidate) {
    pulsesReceived++;
  }
}

int watchdogIteration = 0;
// watchdog low battry level signaling
ISR(WDT_vect)
{
  if (++watchdogIteration == 8) { // so that low battery warning lights are lit once a minute approx.
    watchdogIteration = 0;
    if (ownBatteryLow) {
      PORTB |= _BV(PB0);
    }
    if (otherBatteryLow) {
      PORTB |= _BV(PB1);
    }
    batteryLowWarningLit = true;
  }
}

void setup() {

  DDRB &= ~_BV(DDB3); // pin 2 input downstream voltage
  DDRB |= _BV(DDB4); // pin 3 output switch downstream
  
  DDRB |= _BV(DDB0); // pin 5 output own voltage warning
  DDRB |= _BV(DDB1); // pin 6 output downstream voltage warning

  DDRB &= ~_BV(DDB2); // pin 7 input RC interrupt

  // RC interrupt setup
  GIMSK |= _BV(INT0); // enable interrupt
  MCUCR &= ~(_BV(ISC01) | _BV(ISC00)); // low level, because otherwise attiny wont wake-up

  // check batteries on startup
  PORTB |= _BV(PB4); // turn on downstream just to check voltage
  delay(10);
  checkBatteries();
  PORTB &= ~_BV(PB4); // turn off again
  if (ownBatteryLow || otherBatteryLow) {
    // flash for 2sec if some battery is low
    if (ownBatteryLow) {
      PORTB |= _BV(PB0);
    }
    if (otherBatteryLow) {
      PORTB |= _BV(PB1);
    }
    delay(2000);
    PORTB &= ~(_BV(PB0) | _BV(PB1));
    startLowBatteryWatchdog();
  } else {
    // flash short if all fine
    PORTB |= _BV(PB0) | _BV(PB1);
    delay(5);
    PORTB &= ~(_BV(PB0) | _BV(PB1));
  }

  ADCSRA &= ~_BV(ADEN); // switch ADC off so it doesn't use current
  goToState(S_WaitingTrigger);
  sleep();

}

void loop() {
  long duration;

  if (batteryLowWarningLit) {
    delay(1);
    PORTB &= ~(_BV(PB0) | _BV(PB1));
    batteryLowWarningLit = false;
  }

  switch (currentState) {
    case S_WaitingTrigger:
      sleep();
      break;
    case S_TriggerCandidate:
      duration = millis() - lastStateChangeTime;
      if (duration < TriggerDuration) {
        break;
      } else {
        if (pulsesReceived < (long)TriggerFrequency * duration / 1000 / 2) {
          // false positive
          goToState(S_WaitingTrigger);
          sleep();
        } else {
          goToState(S_Triggered);
        }
        break;
      }
    case S_Triggered:
      PORTB |= _BV(PB4);
      goToState(S_Playing);
      break;
    case S_Playing:
      duration = millis() - lastStateChangeTime;
      if (TurnOnDuration < duration) {
        checkBatteries();
        if (ownBatteryLow || otherBatteryLow) {
          startLowBatteryWatchdog();
        }
        PORTB &= ~_BV(PB4);
        goToState(S_WaitingTrigger);
        sleep();
      }
      break;
  }
}

void sleep() {
  MCUCR &= ~(_BV(ISC01) | _BV(ISC00)); // low level, because otherwise attiny wont wake-up
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  cli();
  sleep_enable();
  sleep_bod_disable();
  sei();
  sleep_cpu();
  sleep_disable();
  sei();
}

void startLowBatteryWatchdog() {
  WDTCR |= _BV(WDP3) | _BV(WDP0);// watchdog prescaler 1k ~8sec
  WDTCR |= _BV(WDIE); // enable watchdog
}

void goToState(state_t state) {
  currentState = state;
  lastStateChangeTime = millis();
}

void checkBatteries() {
  ADCSRA |= _BV(ADEN); // turn ADC on

  int ownVoltage = readOwnVcc();

  ownBatteryLow = ownVoltage < AlarmingOwnBatteryLevel;
  otherBatteryLow = digitalRead(3) == LOW;

  ADCSRA &= ~_BV(ADEN); // switch ADC off again
}


int readOwnVcc() {

  // From http://digistump.com/wiki/digispark/quickref
  // Read 1.1V reference against AVcc
  ADMUX = _BV(MUX3) | _BV(MUX2);

  delay(10); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both

  long result = (high << 8) | low;

  result = 1042000L / result; // Calculate Vcc (in mV); 1125300 = 1.017*1023*1000 (1.017 from adjusting oneself)

  return (int)result; // Vcc in millivolts
}

