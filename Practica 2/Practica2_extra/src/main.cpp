#include <Arduino.h>

// ================= PINES =================
#define LED_PIN     2
#define BTN_UP      18
#define BTN_DOWN    19

// ================= TIMER =================
hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// ================= VARIABLES =================
volatile uint32_t blinkPeriod = 500;   // ms
volatile uint32_t counterMs = 0;
volatile bool ledState = false;

// ---- Evento para impresión ----
enum ButtonEvent {
    NONE,
    INCREASE,
    DECREASE
};

volatile ButtonEvent buttonEvent = NONE;

// --- Estructura botón ---
struct Button {
    uint8_t pin;
    uint8_t stableState;
    uint8_t lastStableState;
    uint8_t debounceCounter;
};

volatile Button btnUp   = {BTN_UP, HIGH, HIGH, 0};
volatile Button btnDown = {BTN_DOWN, HIGH, HIGH, 0};

const uint8_t debounceThreshold = 5;
const uint32_t minPeriod = 50;
const uint32_t maxPeriod = 2000;

// ======================================================
// FUNCIONES
// ======================================================

void initGPIO() {
    pinMode(LED_PIN, OUTPUT);
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
}

void initTimer() {
    timer = timerBegin(0, 80, true);          // 1 µs por tick
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, 1000, true);       // 1 ms
    timerAlarmEnable(timer);
}

void updateBlink() {
    counterMs++;
    if (counterMs >= blinkPeriod) {
        counterMs = 0;
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState);
    }
}

void processButton(Button &btn, bool increaseFreq) {

    if (btn.lastStableState == HIGH && btn.stableState == LOW) {

        if (increaseFreq) {
            if (blinkPeriod > minPeriod)
                blinkPeriod -= 50;
            buttonEvent = INCREASE;
        } else {
            if (blinkPeriod < maxPeriod)
                blinkPeriod += 50;
            buttonEvent = DECREASE;
        }
    }

    btn.lastStableState = btn.stableState;
}

void readButton(Button &btn, bool increaseFreq) {

    uint8_t rawState = digitalRead(btn.pin);

    if (rawState == btn.stableState) {
        btn.debounceCounter = 0;
    } else {
        btn.debounceCounter++;
        if (btn.debounceCounter >= debounceThreshold) {
            btn.stableState = rawState;
            btn.debounceCounter = 0;
            processButton(btn, increaseFreq);
        }
    }
}

void readButtons() {
    readButton((Button&)btnUp, true);
    readButton((Button&)btnDown, false);
}

// ======================================================
// ISR
// ======================================================

void IRAM_ATTR onTimer() {
    portENTER_CRITICAL_ISR(&timerMux);

    updateBlink();
    readButtons();

    portEXIT_CRITICAL_ISR(&timerMux);
}

// ======================================================
// SETUP Y LOOP
// ======================================================

void setup() {
    Serial.begin(115200);
    initGPIO();
    initTimer();

    Serial.println("Sistema iniciado");
}

void loop() {

    if (buttonEvent != NONE) {

        portENTER_CRITICAL(&timerMux);
        ButtonEvent event = buttonEvent;
        buttonEvent = NONE;
        uint32_t currentPeriod = blinkPeriod;
        portEXIT_CRITICAL(&timerMux);

        if (event == INCREASE) {
            Serial.print("SUBE frecuencia - Nuevo periodo: ");
            Serial.print(currentPeriod);
            Serial.println(" ms");
        }

        if (event == DECREASE) {
            Serial.print("BAJA frecuencia - Nuevo periodo: ");
            Serial.print(currentPeriod);
            Serial.println(" ms");
        }
    }
}