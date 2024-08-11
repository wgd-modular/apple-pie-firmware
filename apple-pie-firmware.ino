#include <MCP48xx.h>
#include <avr/interrupt.h>

MCP4822 dac(10);

const int GATE_OUT1 = 2;
const int GATE_OUT2 = 3;
const int CLOCK_IN = 9;

volatile bool clockState = LOW;
volatile int clockCount = 0;

void setup() {
    Serial.begin(9600);

    dac.init();
    dac.turnOnChannelA();
    dac.turnOnChannelB();
    dac.setGainA(MCP4822::High);
    dac.setGainB(MCP4822::High);

    randomSeed(analogRead(7));

    pinMode(GATE_OUT1, OUTPUT);
    pinMode(GATE_OUT2, OUTPUT);

    pinMode(CLOCK_IN, INPUT);
    attachInterrupt(digitalPinToInterrupt(CLOCK_IN), handleClock, RISING);
}

void loop() {
    digitalWrite(GATE_OUT1, clockState);

    if (clockCount % 2 == 0) {
        digitalWrite(GATE_OUT2, clockState);
    }

    int voltageA = random(0, 4096);
    int voltageB = random(0, 4096);

    dac.setVoltageA(voltageA);
    dac.setVoltageB(voltageB);

    dac.updateDAC();

    delay(10);
}

void handleClock() {
    clockState = !clockState;
    clockCount++;
}

uint16_t readAnalogInput(uint8_t pin) {
    return analogRead(pin);
}
