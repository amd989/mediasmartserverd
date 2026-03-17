/////////////////////////////////////////////////////////////////////////////
/// @file mediasmartserver_leds_pico.ino
///
/// Raspberry Pi Pico firmware for controlling HP MediaSmart Server LEDs
/// via USB serial. Receives ASCII commands from the mediasmartserverd
/// daemon and drives LED GPIOs accordingly.
///
/// Uses the arduino-pico core (earlephilhower/arduino-pico).
/// All Pico GPIO pins support PWM, so brightness works on every pin.
///
/// Protocol (newline-terminated ASCII):
///   S <B|R> <0-3> <0|1>   - Set bay LED (color, bay index, on/off)
///   B <0-9>               - Set global brightness
///   Y <B|R|A> <0|1|2>     - Set system LED (color, off/on/blink)
///   U <0|1>               - Set USB LED
///   P                     - Ping (responds "OK")
///   R                     - Reset all LEDs off
///
/// Copyright (c) 2026 Alejandro Mora
///
/// Released under the zlib license. See LICENSE for details.
///
/////////////////////////////////////////////////////////////////////////////

//- configuration -----------------------------------------------------------

// Set to true if the daughter board LEDs are common-anode (active low).
// Set to false if common-cathode (active high).
const bool ACTIVE_LOW = false;

// Bay LED pins: 4 bays x 2 colors (Pico GP pins)
const int PIN_BLUE[] = {2, 3, 4, 5};
const int PIN_RED[]  = {6, 7, 8, 9};
const int NUM_BAYS = 4;

// System LED pins
const int PIN_SYS_BLUE = 10;
const int PIN_SYS_RED  = 11;

// USB indicator LED pin
const int PIN_USB = 12;

// Onboard LED (GP25) — optional status indicator
const int PIN_ONBOARD = 25;

// Blink interval in milliseconds
const unsigned long BLINK_INTERVAL = 500;

//- state -------------------------------------------------------------------

// Logical state of each LED (true = should be lit, ignoring blink)
bool bayBlue[NUM_BAYS];
bool bayRed[NUM_BAYS];
bool sysBlueOn  = false;
bool sysRedOn   = false;
bool sysBlueBlink = false;
bool sysRedBlink  = false;
bool usbOn = false;

// Global brightness: 0-9 maps to PWM 0-255
int brightness = 9;

// Blink toggle state
bool blinkPhase = false;
unsigned long lastBlinkTime = 0;

// Serial command buffer
char cmdBuf[32];
int cmdLen = 0;

//- helpers -----------------------------------------------------------------

void writePin(int pin, bool on) {
    bool hw = ACTIVE_LOW ? !on : on;
    digitalWrite(pin, hw ? HIGH : LOW);
}

void writePinPWM(int pin, bool on, int bright) {
    if (!on) {
        analogWrite(pin, ACTIVE_LOW ? 255 : 0);
        return;
    }
    // Map brightness 0-9 to PWM value
    int pwm = map(bright, 0, 9, 0, 255);
    if (ACTIVE_LOW) pwm = 255 - pwm;
    analogWrite(pin, pwm);
}

void updateBayLeds() {
    for (int i = 0; i < NUM_BAYS; i++) {
        writePinPWM(PIN_BLUE[i], bayBlue[i], brightness);
        writePinPWM(PIN_RED[i], bayRed[i], brightness);
    }
}

void updateSystemLeds() {
    bool blueVisible = sysBlueOn && (!sysBlueBlink || blinkPhase);
    bool redVisible  = sysRedOn  && (!sysRedBlink  || blinkPhase);
    writePinPWM(PIN_SYS_BLUE, blueVisible, brightness);
    writePinPWM(PIN_SYS_RED, redVisible, brightness);
}

void updateUsbLed() {
    writePin(PIN_USB, usbOn);
}

void resetAll() {
    for (int i = 0; i < NUM_BAYS; i++) {
        bayBlue[i] = false;
        bayRed[i]  = false;
    }
    sysBlueOn = false;
    sysRedOn  = false;
    sysBlueBlink = false;
    sysRedBlink  = false;
    usbOn = false;
    brightness = 9;
    blinkPhase = false;

    updateBayLeds();
    updateSystemLeds();
    updateUsbLed();
}

//- command processing ------------------------------------------------------

void processCommand(const char* cmd) {
    if (cmd[0] == 'P') {
        // Ping
        Serial.println("OK");
        return;
    }

    if (cmd[0] == 'R') {
        // Reset
        resetAll();
        return;
    }

    if (cmd[0] == 'S' && cmd[1] == ' ') {
        // Set bay LED: S <B|R> <0-3> <0|1>
        char color = cmd[2];
        int bay   = cmd[4] - '0';
        bool state = cmd[6] == '1';
        if (bay < 0 || bay >= NUM_BAYS) return;

        if (color == 'B') bayBlue[bay] = state;
        if (color == 'R') bayRed[bay]  = state;
        updateBayLeds();
        return;
    }

    if (cmd[0] == 'B' && cmd[1] == ' ') {
        // Brightness: B <0-9>
        int val = cmd[2] - '0';
        if (val < 0) val = 0;
        if (val > 9) val = 9;
        brightness = val;
        updateBayLeds();
        updateSystemLeds();
        return;
    }

    if (cmd[0] == 'Y' && cmd[1] == ' ') {
        // System LED: Y <B|R|A> <0|1|2>
        char color = cmd[2];
        int stateVal = cmd[4] - '0';

        bool on    = (stateVal == 1 || stateVal == 2);
        bool blink = (stateVal == 2);

        if (color == 'B' || color == 'A') {
            sysBlueOn    = on;
            sysBlueBlink = blink;
        }
        if (color == 'R' || color == 'A') {
            sysRedOn    = on;
            sysRedBlink = blink;
        }
        updateSystemLeds();
        return;
    }

    if (cmd[0] == 'U' && cmd[1] == ' ') {
        // USB LED: U <0|1>
        usbOn = (cmd[2] == '1');
        updateUsbLed();
        return;
    }
}

//- setup & loop ------------------------------------------------------------

void setup() {
    // Configure all LED pins as outputs
    for (int i = 0; i < NUM_BAYS; i++) {
        pinMode(PIN_BLUE[i], OUTPUT);
        pinMode(PIN_RED[i],  OUTPUT);
    }
    pinMode(PIN_SYS_BLUE, OUTPUT);
    pinMode(PIN_SYS_RED,  OUTPUT);
    pinMode(PIN_USB,      OUTPUT);
    pinMode(PIN_ONBOARD,  OUTPUT);

    // Start with all LEDs off
    resetAll();

    // USB serial at 115200 baud
    Serial.begin(115200);

    // Blink onboard LED to signal ready
    digitalWrite(PIN_ONBOARD, HIGH);
    delay(200);
    digitalWrite(PIN_ONBOARD, LOW);
}

void loop() {
    // Handle blink timing
    unsigned long now = millis();
    if (now - lastBlinkTime >= BLINK_INTERVAL) {
        lastBlinkTime = now;
        if (sysBlueBlink || sysRedBlink) {
            blinkPhase = !blinkPhase;
            updateSystemLeds();
        }
    }

    // Read serial commands
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (cmdLen > 0) {
                cmdBuf[cmdLen] = '\0';
                processCommand(cmdBuf);
                cmdLen = 0;
            }
        } else if (cmdLen < (int)(sizeof(cmdBuf) - 1)) {
            cmdBuf[cmdLen++] = c;
        }
    }
}
