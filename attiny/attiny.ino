#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#define TriggerDuration 300
#define TriggerFrequency 480

enum state_t {
  S_Initializing = 0,
  S_WaitingTrigger = 1,
  S_TriggerCandidate = 2,
  S_Triggered = 3,
  S_Playing = 4,
};

const long TurnOnDuration = 35000;
volatile byte currentState = S_Initializing;
volatile long lastStateChangeTime;
volatile int pulsesReceived = 0;

// interrupt
ISR(INT0_vect)
{
  if (currentState == S_WaitingTrigger) {
    goToState(S_TriggerCandidate);
    pulsesReceived = 1;
  } else if (currentState == S_TriggerCandidate) {
    pulsesReceived++;
  }
}

void setup() {

  DDRB |= _BV(DDB0); // pin 5 output
  DDRB &= ~_BV(DDB2); // pin 7 input

  // interrupt setup
  GIMSK |= _BV(INT0);
  MCUCR &= ~_BV(ISC00);
  MCUCR &= ~_BV(ISC01);

  ADCSRA &= ~_BV(ADEN); // switch ADC off so it doesn't use current

  goToState(S_WaitingTrigger);
  sleep();
  
}

void loop() {
  long duration;

  switch (currentState) {
    case S_WaitingTrigger:
      // shouldn't happen actually, but still
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
      PORTB |= _BV(PB0);
      goToState(S_Playing);
      break;
    case S_Playing:
      duration = millis() - lastStateChangeTime;
      if (TurnOnDuration < duration) {
        PORTB &= ~_BV(PB0);
        goToState(S_WaitingTrigger);
        sleep();
      }
      break;
  }
}

void sleep() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  cli();
  sleep_enable();
  sleep_bod_disable();
  sei();
  sleep_cpu();
  sleep_disable();
  sei();
}

void goToState(state_t state) {
  currentState = state;
  lastStateChangeTime = millis();
}

