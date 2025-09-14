void tapInput() {
  long now = micros();
  if (now - lastTapTime < minimumTapInterval) {
    return; // Debounce
  }

  if (timesTapped == 0) {
    firstTapTime = now;
  }

  timesTapped++;
  lastTapTime = now;
  //Serial.println("Tap!");
}

void startOrStop() {
  if (!playing) {
    //Serial.println("Start playing");
    Serial.write(MIDI_START);
  } else {
    //Serial.println("Stop playing");
    Serial.write(MIDI_STOP);
  }
  playing = !playing;
}

void sendClockPulse() {
  // Write the timing clock byte
#ifdef MIDI_OUT
  Serial.write(MIDI_TIMING_CLOCK);
#endif

  blinkCount = (blinkCount + 1) % CLOCKS_PER_BEAT;
  //Serial.println(blinkCount);
  if (blinkCount == 0) {
    // Turn led on
#ifdef BLINK_OUTPUT_PIN
    analogWrite(BLINK_OUTPUT_PIN, 255 - BLINK_PIN_POLARITY);
#endif

#ifdef SYNC_OUTPUT_PIN
    // Set sync pin to HIGH
    analogWrite(SYNC_OUTPUT_PIN,  255 - SYNC_PIN_POLARITY);
#endif

#ifdef SYNC2_OUTPUT_PIN  
 // Set sync pin to HIGH
      analogWrite(SYNC2_OUTPUT_PIN, 255 - SYNC_PIN_POLARITY);   
#endif

#ifdef SYNC3_OUTPUT_PIN
  // Set sync pin to HIGH
analogWrite(SYNC3_OUTPUT_PIN, 255 - SYNC_PIN_POLARITY);
   
#endif

#ifdef SYNC4_OUTPUT_PIN
    // Set sync pin to HIGH
  analogWrite(SYNC4_OUTPUT_PIN, 255 - SYNC_PIN_POLARITY);
#endif


  } else {
#ifdef SYNC_OUTPUT_PIN
    if (blinkCount == 1) {
      // Set sync pin to LOW
      analogWrite(SYNC_OUTPUT_PIN,  0 + SYNC_PIN_POLARITY);
      analogWrite(SYNC2_OUTPUT_PIN, 0 + SYNC_PIN_POLARITY);
      analogWrite(SYNC3_OUTPUT_PIN, 0 + SYNC_PIN_POLARITY);
      analogWrite(SYNC4_OUTPUT_PIN, 0 + SYNC_PIN_POLARITY); 
    }
#endif

#ifdef BLINK_OUTPUT_PIN
    if (blinkCount == BLINK_TIME) {
      // Turn led on
      analogWrite(BLINK_OUTPUT_PIN, 0 + BLINK_PIN_POLARITY);
    }
#endif
  }
}


void updateBpm(long now) {
  // Update the timer
  long interval = calculateIntervalMicroSecs(bpm);
  Timer1.setPeriod(interval);

#ifdef EEPROM_ADDRESS
  // Save the BPM in 2 bytes, MSB LSB
  EEPROM.write(EEPROM_ADDRESS, bpm / 256);
  EEPROM.write(EEPROM_ADDRESS + 1, bpm % 256);
#endif

  //Serial.print("Set BPM to: ");
  //Serial.print(bpm / 10);
  //Serial.print('.');
  //Serial.println(bpm % 10);

#ifdef TM1637_DISPLAY
  setDisplayValue(bpm);
#endif
}

long calculateIntervalMicroSecs(int bpm) {
  // Take care about overflows!
  return 60L * 1000 * 1000 * 10 / bpm / CLOCKS_PER_BEAT;
}
