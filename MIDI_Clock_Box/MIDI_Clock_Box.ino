//#define DEBUG
#define MIDI_OUT

#include <TimerOne.h>

/*
 * For all features: if you do not define the pin, the feature will be disabled!
 */

/*
 * FEATURE: TAP BPM INPUT
 */
#define TAP_PIN 2
#define TAP_PIN_POLARITY FALLING

#define MINIMUM_TAPS 3
#define EXIT_MARGIN 150 // If no tap after 150% of last tap interval -> measure and set

/*
 * FEATURE: DIMMER BPM INPUT
 */
//#define DIMMER_INPUT_PIN A0
#define DIMMER_CHANGE_MARGIN 20 // Big value to make sure this doesn't interfere. Tweak as needed.

/*
 * FEATURE: DIMMER BPM INCREASE/DECREASE
 */
#define DIMMER_CHANGE_PIN A0
#define DEAD_ZONE 50
#define CHANGE_THRESHOLD 5000
#define RATE_DIVISOR 30

/*
 * FEATURE: BLINK TEMPO LED
 */
#define BLINK_OUTPUT_PIN 13
#define BLINK_PIN_POLARITY 0  // 0 = POSITIVE, 255 - NEGATIVE
#define BLINK_TIME 4 // How long to keep LED lit in CLOCK counts (so range is [0,24])

/*
 * FEATURE: SYNC PULSE OUTPUT
 */
#define SYNC_OUTPUT_PIN 8 // Can be used to drive sync analog sequencer (Korg Monotribe etc ...)
#define SYNC_PIN_POLARITY 0 // 0 = POSITIVE, 255 - NEGATIVE
#define SYNC2_OUTPUT_PIN 10 // Can be used to drive sync analog sequencer (Korg Monotribe etc ...)
#define SYNC3_OUTPUT_PIN 11 // Can be used to drive sync analog sequencer (Korg Monotribe etc ...)
#define SYNC4_OUTPUT_PIN 12 // Can be used to drive sync analog sequencer (Korg Monotribe etc ...)


/*
 * FEATURE: Send MIDI start/stop
 */
#define START_STOP_INPUT_PIN A1
#define START_STOP_PIN_POLARITY 1024 // 0 = POSITIVE, 1024 = NEGATIVE

#define MIDI_START 0xFA
#define MIDI_STOP 0xFC

#define DEBOUNCE_INTERVAL 500L // Milliseconds

/*
 * FEATURE: EEPROM BPM storage
 */
#define EEPROM_ADDRESS 0 // Where to save BPM
#ifdef EEPROM_ADDRESS
#include <EEPROM.h>
#endif

/*
 * FEATURE: MIDI forwarding
 */
#define MIDI_FORWARD

/*
 * FEATURE: TM1637 display BPM output
 */
#define TM1637_DISPLAY
#ifdef TM1637_DISPLAY
#include <TM1637Display.h>
#define TM1637_CLK_PIN 3
#define TM1637_DIO_PIN 4
#define TM1637_BRIGHTNESS 0x0A
#endif

/*
 * GENERAL PARAMETERS
 */
#define MIDI_TIMING_CLOCK 0xF8
#define CLOCKS_PER_BEAT 24
#define MINIMUM_BPM 400 // Used for debouncing
#define MAXIMUM_BPM 3000 // Used for debouncing

long intervalMicroSeconds;
int bpm;  // BPM in tenths of a BPM!!

bool initialized = false;
long minimumTapInterval = 60L * 1000 * 1000 * 10 / MAXIMUM_BPM;
long maximumTapInterval = 60L * 1000 * 1000 * 10 / MINIMUM_BPM;

volatile long firstTapTime = 0;
volatile long lastTapTime = 0;
volatile long timesTapped = 0;

volatile int blinkCount = 0;
int stepcount = 1;

int lastDimmerValue = 0;

bool playing = false;
long lastStartStopTime = 0;

#ifdef TM1637_DISPLAY
TM1637Display display(TM1637_CLK_PIN, TM1637_DIO_PIN);
uint8_t tm1637_data[4] = {0x00, 0x00, 0x00, 0x00};
#endif

#ifdef DIMMER_CHANGE_PIN
long changeValue = 0;
#endif

void setup() {
  //  Set MIDI baud rate:
Serial.begin(31250);

  // Set pin modes
#ifdef BLINK_OUTPUT_PIN
  pinMode(BLINK_OUTPUT_PIN, OUTPUT);
#endif
#ifdef SYNC_OUTPUT_PIN
  pinMode(SYNC_OUTPUT_PIN, OUTPUT);
#endif
#ifdef SYNC2_OUTPUT_PIN
  pinMode(SYNC2_OUTPUT_PIN, OUTPUT);
#endif
#ifdef SYNC3_OUTPUT_PIN
  pinMode(SYNC3_OUTPUT_PIN, OUTPUT);
#endif
#ifdef SYNC4_OUTPUT_PIN
  pinMode(SYNC4_OUTPUT_PIN, OUTPUT);
#endif

#ifdef DIMMER_INPUT_PIN
  pinMode(DIMMER_INPUT_PIN, INPUT);
#endif
#ifdef START_STOP_INPUT_PIN
  pinMode(START_STOP_INPUT_PIN, INPUT_PULLUP);
#endif

#ifdef EEPROM_ADDRESS
  // Get the saved BPM value from 2 stored bytes: MSB LSB
  bpm = EEPROM.read(EEPROM_ADDRESS) << 8;
  bpm += EEPROM.read(EEPROM_ADDRESS + 1);
  if (bpm < MINIMUM_BPM || bpm > MAXIMUM_BPM) {
    bpm = 1200;
  }
#endif

#ifdef TAP_PIN
  // Interrupt for catching tap events
  pinMode(TAP_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TAP_PIN), tapInput, TAP_PIN_POLARITY);
#endif

  // Attach the interrupt to send the MIDI clock and start the timer
  Timer1.initialize(intervalMicroSeconds);
  Timer1.setPeriod(calculateIntervalMicroSecs(bpm));
  Timer1.attachInterrupt(sendClockPulse);

#ifdef DIMMER_INPUT_PIN
  // Initialize dimmer value
  lastDimmerValue = analogRead(DIMMER_INPUT_PIN);
#endif

#ifdef TM1637_DISPLAY
  display.setBrightness(TM1637_BRIGHTNESS);
  setDisplayValue(bpm);
#endif
}

void loop() {
  long now = micros();

#ifdef TAP_PIN
  /*
   * Handle tapping of the tap tempo button
   */
  if (timesTapped > 0 && timesTapped < MINIMUM_TAPS && (now - lastTapTime) > maximumTapInterval) {
    // Single taps, not enough to calculate a BPM -> ignore!
    //    Serial.println("Ignoring lone taps!");
    timesTapped = 0;
  } else if (timesTapped >= MINIMUM_TAPS) {
    long avgTapInterval = (lastTapTime - firstTapTime) / (timesTapped - 1);
    if ((now - lastTapTime) > (avgTapInterval * EXIT_MARGIN / 100)) {
      bpm = 60L * 1000 * 1000 * 10 / avgTapInterval;
      updateBpm(now);
  
      // Update blinkCount to make sure LED blink matches tapped beat
      blinkCount = ((now - lastTapTime) * 24 / avgTapInterval) % CLOCKS_PER_BEAT;

      timesTapped = 0;
    }
  }
#endif

#ifdef DIMMER_INPUT_PIN
  /*
   * Handle change of the dimmer input
   */
  int curDimValue = analogRead(DIMMER_INPUT_PIN);
  if (curDimValue > lastDimmerValue + DIMMER_CHANGE_MARGIN
      || curDimValue < lastDimmerValue - DIMMER_CHANGE_MARGIN) {
    // We've got movement!!
    bpm = map(curDimValue, 0, 1024, MINIMUM_BPM, MAXIMUM_BPM);

    updateBpm(now);
    lastDimmerValue = curDimValue;
  }
#endif

#ifdef DIMMER_CHANGE_PIN
  int curDimValue = analogRead(DIMMER_CHANGE_PIN);
  if (bpm > MINIMUM_BPM && curDimValue < (512 - DEAD_ZONE)) {
    int val = (512 - DEAD_ZONE - curDimValue) / RATE_DIVISOR;
    changeValue += val * val;
  } else if (bpm < MAXIMUM_BPM && curDimValue > (512 + DEAD_ZONE)) {
    int val = (curDimValue - 512 - DEAD_ZONE) / RATE_DIVISOR;
    changeValue += val * val;
  } else {
    changeValue = 0;
  }
  if (changeValue > CHANGE_THRESHOLD) {
    bpm += curDimValue < 512 ? -1 : 1;
    updateBpm(now);
    changeValue = 0;
  }
#endif

#ifdef START_STOP_INPUT_PIN
  /*
   * Check for start/stop button pressed
   */
  bool startStopPressed = (START_STOP_PIN_POLARITY - analogRead(START_STOP_INPUT_PIN)) > 1024 / 2 ? true : false;
  if (startStopPressed && (lastStartStopTime + (DEBOUNCE_INTERVAL * 1000)) < now) {
    startOrStop();
    lastStartStopTime = now;
  }
#endif

#ifdef MIDI_FORWARD
  /*
   * Forward received serial data
   */
  while (Serial.available()) {
    int b = Serial.read();
    Serial.write(b);
  }
#endif
}


#ifdef TM1637_DISPLAY
void setDisplayValue(int value) {
  display.clear();
  //tm1637_data[0] = value >= 1000 ? display.encodeDigit(value / 1000) : 0x00;
  //tm1637_data[1] = value >= 100 ? display.encodeDigit((value / 100) % 10) : 0x00;
  //tm1637_data[2] = value >= 10 ? display.encodeDigit((value / 10) % 10) : 0x00;
  //tm1637_data[3] = display.encodeDigit(value % 10);
  display.showNumberDecEx(value, 0b00100000, true, 4,0 );
  //display.setSegments(tm1637_data);
}
#endif


