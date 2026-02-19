// -------- INCLUDE NOTES --------
#include "pitches.h"

// -------- PIN SETUP --------
const int piezoPins[] = {A0, A1, A2, A3};
const int numPiezos = 4;

const int speakerPin = 8;

// -------- SENSITIVITY SETTINGS --------
const int deltaThreshold = 3;     // VERY sensitive
const unsigned long debounceTime = 8; // short but stable

// -------- RECORDING SETUP --------
const unsigned long recordDuration = 10000;
unsigned long startTime;

int impactCount = 0;
bool isRecording = false;

// Per-piezo tracking
int lastSensorValue[4] = {0,0,0,0};
int smoothedValue[4]   = {0,0,0,0};
unsigned long lastImpactTime[4] = {0,0,0,0};

// -------- PLAYBACK SETUP --------
unsigned long noteInterval = 500;
unsigned long lastNoteTime = 0;

// -------- ARCHIVE SETUP --------
const int maxSessions = 5;

int sessionImpact[maxSessions];
float sessionBPM[maxSessions];
int sessionScale[maxSessions];

int totalSessions = 0;
int currentSessionIndex = -1;

// -------- SCALES --------
int scalePentatonic[] = { NOTE_C4, NOTE_D4, NOTE_E4, NOTE_G4, NOTE_A4 };
int scaleMinor[] = { NOTE_C4, NOTE_D4, NOTE_DS4, NOTE_F4,
                     NOTE_G4, NOTE_GS4, NOTE_AS4 };
int scaleWholeTone[] = { NOTE_C4, NOTE_D4, NOTE_E4,
                         NOTE_FS4, NOTE_GS4, NOTE_AS4 };

int* currentScale;
int scaleSize = 0;
int lastNoteIndex = 0;

// -----------------------------------

void setup() {

  Serial.begin(9600);
  delay(1000);

  randomSeed(analogRead(A5));

  Serial.println("Rain Archive Ready.");
  Serial.println("Ultra-sensitive mode (stable).");
  Serial.println("r = record");
  Serial.println("n = next session");
  Serial.println("a1, a2... = choose session");

  // Initialize baseline values
  for (int i = 0; i < numPiezos; i++) {
    int initial = analogRead(piezoPins[i]);
    lastSensorValue[i] = initial;
    smoothedValue[i] = initial;
  }
}

// -----------------------------------

void loop() {

  handleSerialInput();

  if (isRecording) {
    recordRain();
  }
  else if (currentSessionIndex >= 0) {
    playGeneratedMusic();
  }
}

// -----------------------------------
// SERIAL INPUT
// -----------------------------------

void handleSerialInput() {

  if (Serial.available() > 0) {

    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input == "r" && !isRecording) {
      startRecording();
    }

    else if (input == "n") {
      loadNextSession();
    }

    else if (input.startsWith("a")) {

      if (input.length() > 1) {

        int requested = input.substring(1).toInt();

        if (requested > 0 && requested <= totalSessions) {
          loadSession(requested - 1);
        }
        else {
          Serial.println("Invalid session number.");
        }
      }
    }
  }
}

// -----------------------------------
// START RECORDING
// -----------------------------------

void startRecording() {

  Serial.println("\n--- Recording New Session ---");

  noTone(speakerPin);
  isRecording = true;
  impactCount = 0;
  startTime = millis();

  for (int i = 0; i < numPiezos; i++) {
    int val = analogRead(piezoPins[i]);
    lastSensorValue[i] = val;
    smoothedValue[i] = val;
    lastImpactTime[i] = 0;
  }
}

// -----------------------------------
// ULTRA-SENSITIVE BUT STABLE DETECTION
// -----------------------------------

void recordRain() {

  unsigned long currentTime = millis();

  if (currentTime - startTime >= recordDuration) {
    finishRecording();
    return;
  }

  for (int i = 0; i < numPiezos; i++) {

    int raw = analogRead(piezoPins[i]);

    // Light smoothing to reduce jitter
    smoothedValue[i] = (smoothedValue[i] * 3 + raw) / 4;

    int delta = raw - smoothedValue[i];

    // Detect sharp positive spike
    if (delta > deltaThreshold &&
        currentTime - lastImpactTime[i] > debounceTime) {

      impactCount++;
      lastImpactTime[i] = currentTime;

      Serial.print("Impact P");
      Serial.print(i);
      Serial.print(" | Strength: ");
      Serial.print(delta);
      Serial.print(" | Total: ");
      Serial.println(impactCount);
    }

    lastSensorValue[i] = raw;
  }
}

// -----------------------------------
// FINISH RECORDING
// -----------------------------------

void finishRecording() {

  isRecording = false;

  float durationSeconds = recordDuration / 1000.0;
  float rawBPM = (impactCount / durationSeconds) * 60.0;
  float scaledBPM = constrain(rawBPM * 0.5, 60, 280);

  if (totalSessions < maxSessions) {

    sessionImpact[totalSessions] = impactCount;
    sessionBPM[totalSessions] = scaledBPM;

    if (scaledBPM < 40) sessionScale[totalSessions] = 0;
    else if (scaledBPM < 100) sessionScale[totalSessions] = 1;
    else sessionScale[totalSessions] = 2;

    totalSessions++;
  }
  else {
    Serial.println("Archive full.");
  }

  Serial.println("\n--- Session Stored ---");
  Serial.print("Total Sessions: ");
  Serial.println(totalSessions);

  currentSessionIndex = totalSessions - 1;
  loadSession(currentSessionIndex);
}

// -----------------------------------
// LOAD NEXT
// -----------------------------------

void loadNextSession() {

  if (totalSessions == 0) {
    Serial.println("No sessions stored.");
    return;
  }

  currentSessionIndex++;
  if (currentSessionIndex >= totalSessions)
    currentSessionIndex = 0;

  loadSession(currentSessionIndex);
}

// -----------------------------------
// LOAD SESSION
// -----------------------------------

void loadSession(int index) {

  float bpm = sessionBPM[index];
  noteInterval = 60000 / bpm;

  int scaleIndex = sessionScale[index];

  if (scaleIndex == 0) {
    currentScale = scalePentatonic;
    scaleSize = 5;
  }
  else if (scaleIndex == 1) {
    currentScale = scaleMinor;
    scaleSize = 7;
  }
  else {
    currentScale = scaleWholeTone;
    scaleSize = 6;
  }

  Serial.println("\n--- Playing Session ---");
  Serial.print("Session ");
  Serial.print(index + 1);
  Serial.print(" / ");
  Serial.println(totalSessions);

  Serial.print("Impacts: ");
  Serial.println(sessionImpact[index]);

  Serial.print("BPM: ");
  Serial.println(bpm);

  lastNoteIndex = 0;
}

// -----------------------------------
// GENERATIVE PLAYBACK
// -----------------------------------

void playGeneratedMusic() {

  unsigned long now = millis();

  if (now - lastNoteTime >= noteInterval) {

    int step = random(-1, 2);
    int newIndex = constrain(lastNoteIndex + step, 0, scaleSize - 1);
    lastNoteIndex = newIndex;

    tone(speakerPin,
         currentScale[newIndex],
         noteInterval * 0.6);

    lastNoteTime = now;
  }
}


