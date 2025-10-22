// Pin connected to the positive of the ultrasonic transmitter
const int txPin = 9;

// Parameters
const int cyclesPerSample = 40; // Number of 40kHz cycles per audio sample (approx 1 ms)
const int numAudioSamples = 32; // Size of audio sample array

// Example audio waveform (-100 to 100)
int audioSamples[numAudioSamples] = {
  0, 50, 100, 50, 0, -50, -100, -50,
  0, 50, 100, 50, 0, -50, -100, -50,
  0, 50, 100, 50, 0, -50, -100, -50,
  0, 50, 100, 50, 0, -50, -100, -50
};

void setup() {
  pinMode(txPin, OUTPUT);
}

void loop() {
  // Loop through each audio sample
  for (int i = 0; i < numAudioSamples; i++) {
    int amp = audioSamples[i];

    // Generate short burst of 40 kHz carrier, amplitude scaled by audio sample
    for (int j = 0; j < cyclesPerSample; j++) {
      // Scale HIGH and LOW time based on audio sample
      int halfPeriod = 12; // microseconds for half period at 40 kHz

      // Simple amplitude scaling: adjust pulse width slightly
      int adjustedHalfPeriod = halfPeriod * (100 - abs(amp)) / 100;

      digitalWrite(txPin, HIGH);
      delayMicroseconds(adjustedHalfPeriod);
      digitalWrite(txPin, LOW);
      delayMicroseconds(adjustedHalfPeriod);
    }
  }
}
