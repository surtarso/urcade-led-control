/*
URCade LED Control for Emulators using Arduino
---------------------------------------
This script manages LED presets for emulators using an attached controller/joystick.
It works in conjunction with URCade's daemon running to send the number of buttons.

Features:
- Action button LEDs: PWM pins 3, 5, 6, 9, 10, 11 for brightness control using analogWrite. (pins with ~)
- HITBOX arrow LEDs: Digital pins 7, 8, 12, 13 for on/off using digitalWrite for hitbox arrows
- Hitbox toggle switch: Connected to Digital pin 4 (or just short pin 4 and ground for always on, default is off)
- Light Mode Toggle Button: Connected to digital pin 2 to switch between idle modes.
- LDR: Analog input pin A0 uses a light sensor to turn off all leds and the computer screen 
  if light conditions are low (the arcade lid is closed) and restores the previous state when
  light conditions are high (lid is open).
- Idle Mode: All LEDs off, all LEDs on or random effect every 15 seconds.
- Emulator Presets: Determined by the number received from a daemon/PC.
  The number corresponds to the button count of the original emulator controller.

Tarso Galvão - Fri Jan 21 11:28:57 PM EST 2025
*/
#include <Arduino.h>

// VARIABLES ---------------------------------------------------------

// --------------- IMPORTANT true/false------------------------
// Light Sensor (LDR)
// uses a light sensor to turn off all leds and computer screen
// this is usefull if your arcade has a lid but no way to detect its closed.
bool usingLightSensor = false;  // Use 'false' if you don't use a lid or sensor.
int lightSensorPin = A0;  // Pin where the LDR (light sensor) is connected
// --------------------------------------------------
// LDR sensivity settings
int lightLimit = 200;  // minimum light to consider OFF
bool lightStatus = true;  // tracks whether light is detected

// --------------- IMPORTANT true/false------------------------
// Hitbox arrow LEDs on/off switch
// Keep 'false' if you are using a standard arcade stick or a physical toggle switch.
// These are optional LED on arrow keys for a hitbox controller arrows
// They can be toggled using 'hitbox = true;' bellow  -- OR --
// you can add a toggle switch but keep 'usingHitbox = false' if using one!
// Yet another option is to just permanently short pin 4 and ground.
// You need to reset the arduino for the changes to take effect.
// The pins are ordered to left, down, right, up keys.
bool usingHitbox = false; // keep 'false' if using a physical switch!
int hitboxSwitchPin = 4; // toggle on/off switch for hitbox mode.
int hitboxArrowPins[] = {7, 8, 12, 13}; // left/down/right/up (hitbox)
// --------------------------------------------------

// Action Button LEDS
// Array to store the button LED pin numbers
// These are the pins that connect the arcade button leds to the arduino
// They are relative to snes controller configuration (A, B, X, Y, L, R)
int actionButtonPins[] = {3, 5, 6, 9, 10, 11};
// Led brightness and fading variables
int brightness = 0;   // Brightness for fading
int maxBrightness = 255; // Maximum brightness value
int fadeAmount = 5;   // How much to fade the LEDs by on each loop

// Calculate the total number of LEDs based on the size of the array
int numButtonLeds = sizeof(actionButtonPins) / sizeof(actionButtonPins[0]);
int numArrowLeds = sizeof(hitboxArrowPins) / sizeof(hitboxArrowPins[0]);

// Variable to track the current system state (frontend menu or in-game specifics)
// States range from 0 (on/off/random) to 6 (all LEDs on)
int systemState = 0;  // 0 = Effects Mode, 1 = 1 LED, ..., 6 = All LEDs
int lastSystemState = systemState;
int lastEffect = -1;

// Effect selection (this will play once on boot unless turned off (-1))
// -1 off, 0 lightTrail, 1 blinkAll, 2 fadeAll, 3 wave, 4 chaoticBlink,
// 5 running, 6 sparkle, 7 cylon, 8 gradient, 9 chase
int currentRandomEffect = -1;   // Sets the boot effect (1st to run)
unsigned long lastSwitchTime = 0; // Time since the last effect switch
const unsigned long effectToggleInterval = 15000; // 15 seconds in milliseconds

// Effect-Mode toggle push button
int effectButtonPin = 2;   // Pin for the optional toggle button
// ----------------- IMPORTANT ---------------------
// Toggle idle state: 0 all leds off, 1 all leds on, 2 random effect
// These can only be cycled by pressing the toggle button (optional)
// or shorting pins D4 and GND with a wire...
// This value is permanent if a dedicated button is not used!
// Its the standard state if the emulator running is not detected
// and for navigating the front-end menu. Use 0 or 1 if you dont plan
// to use the toggle button. Emulator specifics will still be handled!
int effectState = 1; // Initial (or fixed) state for effect mode toggle
// --------------------------------------------------
int lastEffectState = effectState;
unsigned long lastDebounceTime = 0; // For debouncing button presses
const unsigned long debounceDelay = 50; // Debounce delay in milliseconds
bool effectButtonState = HIGH;      // Current button state
bool lastEffectButtonState = HIGH; // Previous button state

// SETUP ---------------------------------------------------------
// The setup routine runs once when the Arduino starts or is reset
void setup() {
  // Set LIGHT SENSOR pin as input to receive data from it
  if (usingLightSensor) {
    pinMode(lightSensorPin, INPUT);
  }

  // Read HITBOX switch state (1=off(no switch), 0=on(shorted))
  // You need to restart the device for changes to take effect.
  pinMode(hitboxSwitchPin, INPUT_PULLUP);
  bool reading = digitalRead(hitboxSwitchPin);
  if (reading == 0){
    usingHitbox = true;
    // Set each hitbox arrow LED pin as an output 
    // This allows the Arduino to control the arrow LEDs
    for (int i = 0; i < numArrowLeds; i++) {
      pinMode(hitboxArrowPins[i], OUTPUT);
    }
  };
  
  // Set each ACTION BUTTON LED pin as an output 
  // This allows the Arduino to control the button LEDs
  for (int i = 0; i < numButtonLeds; i++) {
    pinMode(actionButtonPins[i], OUTPUT);
  }

  // Set the EFFECT STATE BUTTON pin as an input with pull-up resistor
  // This allows the Arduino to detect button presses to change effect mode on the fly.
  pinMode(effectButtonPin, INPUT_PULLUP);

  // Initialize serial communication at 9600 baud rate
  // This allows the Arduino to communicate with the computer
  Serial.begin(9600);
  // Print a message to the serial monitor to confirm communication
  Serial.println("Listening at 9600 baud rate");

  // Seed the random number generator
  // This is used by the idle effects to generate random values
  randomSeed(analogRead(A0));
}

// FUNCTIONS ---------------------------------------------------------
// EFFECT MODES CONTROL ----------------------------------
// Function to turn OFF all LEDs
void clearLEDs() { // effectState = 0
  for (int i = 0; i < numButtonLeds; i++) {
    analogWrite(actionButtonPins[i], 0);
  }
  if (usingHitbox){
    for (int i = 0; i < numArrowLeds; i++) {
      digitalWrite(hitboxArrowPins[i], LOW);
    }
  }
}

// Function to turn ON all LEDs
void UseAllLEDs() { // effectState = 1
  for (int i = 0; i < numButtonLeds; i++) {
    analogWrite(actionButtonPins[i], maxBrightness);
  }
  if (usingHitbox){
    for (int i = 0; i < numArrowLeds; i++) {
      digitalWrite(hitboxArrowPins[i], HIGH);
    }
  }
}

// Function to set the LED states based on the system state (emulator buttons)
// This function will receive a state value from the daemon and update the LEDs
// according to the relative number of buttons on the emulator's original controller.
// This function is only called when an emulator is running (systemState != 0)
void setLEDsForSystem(int state) { // systemState = 1 to 6
  // Turn all LEDs off initially
  clearLEDs();

  // Turn on LEDs up to the specified state (number of active buttons)
  // For example, if state = 3, the first 3 LEDs will be lit
  for (int i = 0; i < state && i < numButtonLeds; i++) {
    analogWrite(actionButtonPins[i], maxBrightness);
  }
  if (usingHitbox){
    // turn on all arrow keys
    for (int i = 0; i < numArrowLeds; i++) {
      digitalWrite(hitboxArrowPins[i], HIGH);
    }
  }
}

// LIGHT SENSOR CONTROL ------------------------------------
// Save states and turn all LEDS off (lid closed)
void saveAndResetStates() {
  // Save current states
  lastEffectState = effectState;
  lastSystemState = systemState;

  // Reset states to 0
  effectState = 0;
  systemState = 0;
}

// Restore previous LED state (lid open)
void restoreStates() {
  // Restore saved states
  effectState = lastEffectState;
  systemState = lastSystemState;

  // If an emulator is running, set correct LED state
  if (systemState != 0) {
    setLEDsForSystem(systemState);
  }
}

// EFFECTS -----------------------------------------------------------
// Light Trail Effect
void lightTrailEffect() { // cycleEffectsState() case 0
  static int currentButtonLED = 0; // Tracks the current LED in the trail
  static int currentArrowLED = 0;

  // Turn off all LEDs
  clearLEDs();

  // Turn on the current LED
  analogWrite(actionButtonPins[currentButtonLED], maxBrightness);
  if (usingHitbox){
    digitalWrite(hitboxArrowPins[currentArrowLED], HIGH);
  }

  // Move to the next LED
  currentButtonLED = (currentButtonLED + 1) % numButtonLeds;
  currentArrowLED = (currentArrowLED + 1) % numArrowLeds;

  delay(400); // Delay for smooth trail movement
}

// Blinking All LEDs Effect
void blinkAllEffect() { // cycleEffectsState() case 1
  static bool state = false; // Tracks whether LEDs are on or off

  // Turn all LEDs on or off
  for (int i = 0; i < numButtonLeds; i++) {
    analogWrite(actionButtonPins[i], state ? maxBrightness : 0);
  }
  // Turn on arrow keys
  if (usingHitbox){
    for (int i = 0; i < numArrowLeds; i++) {
      digitalWrite(hitboxArrowPins[i], state ? HIGH : LOW);
    }
  }
  // Toggle the state
  state = !state;

  delay(1500); // Delay between blinks
}

// Fading All LEDs Effect
void fadeAllEffect() { // cycleEffectsState() case 2
  // Set all LEDs to the current brightness
  for (int i = 0; i < numButtonLeds; i++) {
    analogWrite(actionButtonPins[i], brightness);
  }

  // Update the brightness for fading
  brightness += fadeAmount;

  // Reverse the fading direction at the limits
  if (brightness <= 0 || brightness >= maxBrightness) {
    fadeAmount = -fadeAmount;
  }

  if (usingHitbox){
    if (brightness == 0) {
      // Turn off arrow keys
      for (int i = 0; i < numArrowLeds; i++) {
        digitalWrite(hitboxArrowPins[i], LOW);
      }
    }

    if (brightness == maxBrightness) {
      // Turn on arrow keys
      for (int i = 0; i < numArrowLeds; i++) {
        digitalWrite(hitboxArrowPins[i], HIGH);
      }
    }
  }

  delay(30); // Smooth fading effect
}

// Wave Effect (LEDs light up one by one, then back down)
void waveEffect() { // cycleEffectsState() case 3
  static int direction = 1; // 1 = forward, -1 = backward
  static int arrowDirection = 1;
  static int currentButtonLED = 0; // Tracks the current LED
  static int currentArrowLED = 0;

  // Turn off all LEDs
  clearLEDs();

  // Turn on the current LED
  analogWrite(actionButtonPins[currentButtonLED], maxBrightness);
  if (usingHitbox){
    digitalWrite(hitboxArrowPins[currentArrowLED], HIGH);
  }
  // Move to the next LED
  currentButtonLED += direction;
  currentArrowLED += arrowDirection;

  // Reverse direction at the ends
  if (currentButtonLED == numButtonLeds - 1 || currentButtonLED == 0) {
    direction = -direction;
  }

  if (currentArrowLED == numArrowLeds -1 || currentArrowLED == 0) {
    arrowDirection = -arrowDirection;
  }

  delay(400); // Delay for smooth wave movement
}

// Chaotic Random Blinking Effect
void chaoticBlinkEffect() { // cycleEffectsState() case 4
  // Loop through all LEDs
  for (int i = 0; i < numButtonLeds; i++) {
    // Randomly decide if the LED should be on or off
    int randomBrightness = random(0, 2) * maxBrightness; // 0 or 255
    analogWrite(actionButtonPins[i], randomBrightness);
  }

  if (usingHitbox){
    for (int i = 0; i < numArrowLeds; i++) {
      // Randomly decide if the LED should be on or off
      int randomPower = random(0, 2) == 0 ? LOW : HIGH; // LOW or HIGH
      digitalWrite(hitboxArrowPins[i], randomPower);
    }
  }

  delay(random(50, 300)); // Random delay between updates for chaotic effect
}

// Running Lights Effect
void runningLightsEffect() { // cycleEffectsState() case 5
  static int position = 0;

  for (int i = 0; i < numButtonLeds; i++) {
    float brightness = (sin((i + position) * PI / numButtonLeds) + 1) * 255; // Smooth sine wave
    analogWrite(actionButtonPins[i], brightness);
  }

  position = (position + 1) % numButtonLeds;

  delay(50); // Delay for smooth running lights
}

// Sparkle Effect
void sparkleEffect() { // cycleEffectsState() case 6
  static int sparkleLED = -1;
  static int sparkleArrowLED = -1;

  // Randomly pick an LED to sparkle
  sparkleLED = random(0, numButtonLeds);
  sparkleArrowLED = random(0, numArrowLeds);

  // Turn off all LEDs
  clearLEDs();

  // Turn on the sparkle LED
  analogWrite(actionButtonPins[sparkleLED], maxBrightness);
  if (usingHitbox){
    digitalWrite(hitboxArrowPins[sparkleArrowLED], HIGH);
  }

  delay(random(50, 200)); // Random sparkle duration
}

// Cylon Effect (Knight Rider)
void knightRiderEffect() { // cycleEffectsState() case 7
  static int direction = 1; // 1 = forward, -1 = backward
  static int arrowDirection = 1;
  static int currentButtonLED = 0;
  static int currentArrowLED = 0;

  // Turn off all LEDs
  clearLEDs();

  // Turn on the current LED with maximum brightness
  analogWrite(actionButtonPins[currentButtonLED], maxBrightness);
  if (usingHitbox){
    digitalWrite(hitboxArrowPins[currentArrowLED], HIGH);
  }

  // Move to the next LED
  currentButtonLED += direction;
  currentArrowLED += arrowDirection;

  // Reverse direction at the ends
  if (currentButtonLED == numButtonLeds - 1 || currentButtonLED == 0) {
    direction = -direction;
  }

  if (currentArrowLED == numArrowLeds -1 || currentArrowLED == 0) {
    arrowDirection = -arrowDirection;
  }

  delay(100); // Smooth bouncing movement
}

// Gradient Effect
void gradientEffect() { // cycleEffectsState() case 8
  static int offset = 0;

  for (int i = 0; i < numButtonLeds; i++) {
    int brightness = (sin((i + offset) * PI / numButtonLeds) + 1) * 25; // Smooth sine wave
    analogWrite(actionButtonPins[i], brightness);
  }

  offset = (offset + 1) % numButtonLeds;

  if (usingHitbox){
    for (int i = 0; i < numArrowLeds; i++) {
      // Randomly decide if the LED should be on or off
      int randomPower = random(0, 2) == 0 ? LOW : HIGH; // LOW or HIGH
      digitalWrite(hitboxArrowPins[i], randomPower);
    }
  }

  delay(150); // Smooth gradient transition
}

// Chase Effect
void chaseEffect() { // cycleEffectsState() case 9
  static int headButtonLED = 0;
  static int headArrowLED = 0;

  // Dim all button LEDs
  for (int i = 0; i < numButtonLeds; i++) {
    int brightness = analogRead(actionButtonPins[i]) - 180; // Gradually fade out
    brightness = brightness < 0 ? 0 : brightness;
    analogWrite(actionButtonPins[i], brightness);
  }

  if (usingHitbox){
    // Turn off arrow keys
    for (int i = 0; i < numArrowLeds; i++) {
      digitalWrite(hitboxArrowPins[i], LOW);
    }
  }
  // Brighten the head LED
  analogWrite(actionButtonPins[headButtonLED], maxBrightness);
  if (usingHitbox){
    digitalWrite(hitboxArrowPins[headArrowLED], HIGH);
  }

  // Move to the next LED
  headButtonLED = (headButtonLED + 1) % numButtonLeds;
  headArrowLED = (headArrowLED + 1) % numArrowLeds;

  delay(250); // Smooth chasing effect
}

// IDLE EFFECTS CONTROL ------------------------------------------------------
// Function to handle NO emulators running (systemState == 0)
// when effects mode is ON (effectState == 2)
// This function will switch between random effects at regular intervals
void cycleEffectsState() { // effectState = 2
  // Check if it's time to switch effects if the toggle button is not pressed
  if ((millis() - lastSwitchTime > effectToggleInterval) && (effectButtonState == HIGH)) {
    lastSwitchTime = millis(); // Update the last switch time

    int newEffect;
    do {
        newEffect = random(0, 10); // Randomly select an effect (0 to 9)
    } while (newEffect == lastEffect); // Ensure it doesn't repeat the last effect

    currentRandomEffect = newEffect; // Assign the new effect
    lastEffect = newEffect;    // Update the last effect

    // Debugging output to the serial monitor
    Serial.print("Switched to random effect: ");
    Serial.println(currentRandomEffect);
  }

  // Execute the selected effect
  switch (currentRandomEffect) {
    case 0:
      lightTrailEffect();
      break;
    case 1:
      blinkAllEffect();
      break;
    case 2:
      fadeAllEffect();
      break;
    case 3:
      waveEffect();
      break;
    case 4:
      chaoticBlinkEffect();
      break;
    case 5:
      runningLightsEffect();
      break;
    case 6:
      sparkleEffect();
      break;
    case 7:
      knightRiderEffect();
      break;
    case 8:
      gradientEffect();
      break;
    case 9:
      chaseEffect();
      break;
  }
}

// MAIN LOOP ---------------------------------------------------------
// The loop routine runs continuously after setup() is completed
void loop() {
  // LIGHT SENSOR LOOP -------------------------------
  // Execute actions depending on light conditions
  if (usingLightSensor) {
  // create a variable to store the data of the LDR
    int sensorData = analogRead(lightSensorPin);
    // Debugging messages onto serial monitor
    //Serial.println(sensorData);
  
    if (sensorData < lightLimit && lightStatus) {
      // No light detected
      saveAndResetStates();
      Serial.println("screen_off"); // send message to the daemon
      lightStatus = false;
    } else if (sensorData >= lightLimit && !lightStatus) {
      // Light detected again
      restoreStates();
      Serial.println("screen_on"); // send message to the daemon
      lightStatus = true;
    }
  }

  // EMULATORS PRESET LOOP (systemState machine) -------------------------------------------------
  // This block will handle the LED states based on a number the daemon sends
  // This number is relative to the number of buttons on the original controller
  // of the emulator starting and will range from the 1st button as "A" and expect
  // B, X, Y, L, R as the physical order of the LEDs on the arcade joystick buttons.
  // Check if there is any data available on the serial input
  if (Serial.available() > 0) {
    // Read the incoming data as an integer
    int daemonInputState = Serial.parseInt();

    // Adjust daemonInputState if it exceeds the allowed maximum value
    // This will prevent the system daemon from trying to light more LEDs than available
    if (daemonInputState > numButtonLeds) {
      daemonInputState = numButtonLeds;
    }

    // Validate the input state if between 0 and the number of LEDs
    if (daemonInputState >= 0) {
      // Update the system state to the new value
      systemState = daemonInputState;
      // Call the function to update the LEDs to match the new system state
      // This will stop any effects (in systemState = 0) and set the LEDs accordingly
      setLEDsForSystem(systemState);
    } else {
      // Debugging output
      // Print an error message if the input is invalid
      Serial.println("Invalid input!");
    }

    // Clear any remaining data in the serial input buffer
    while (Serial.available() > 0) {
      Serial.read();
    }
  }

  // EFFECTS STATE BUTTON LOOP (effectState machine) --------------------------------------------
  // This block will handle the button press to cycle between idle modes
  // It will cycle between 0: (all LEDs off), 1: (all LEDs on), and 2: (random effects)
  // Read the button state
  bool reading = digitalRead(effectButtonPin);

  // Check if the button is pressed (LOW) and the system is in effects mode
  if (systemState == 0) {
    // Check for state change (debouncing)
    // debounceDelay is used to prevent rapid toggling
    if (reading != lastEffectButtonState) {
      lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > debounceDelay) {
      // Update the button state if stable
      if (reading != effectButtonState) {
        effectButtonState = reading;

        // When button is pressed (LOW to HIGH transition)
        if (effectButtonState == LOW) {
          effectState = (effectState + 1) % 3; // Cycle through 0, 1, 2
          // Debugging output to the serial monitor
          Serial.print("Toggle Mode State: ");
          Serial.println(effectState);
        }
      }
    }

    // Save the current reading as last state for the next loop
    lastEffectButtonState = reading;

    // Execute the selected effect mode
    switch (effectState) {
      case 0:
        clearLEDs(); // turn off all LEDs
        break;
      case 1:
        UseAllLEDs(); // turn on all LEDs
        break;
      case 2:
        cycleEffectsState(); // cycle through random effects
        break;
    }
  }
}
