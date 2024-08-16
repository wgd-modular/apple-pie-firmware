#include <MCP48xx.h>

// Pin Definitions
const int GATE_OUT1 = 2;
const int GATE_OUT2 = 3;
const int CLOCK_IN = 9;
const int LOCK_PIN = A4;
const int CV_1 = A0;
const int CV_2 = A1;

// State Variable for Clock
volatile bool clockState1 = LOW;
volatile bool clockState2 = LOW;


// Shift Register Arrays
uint16_t shiftRegister1 = 0x0000; // Example initial value
uint16_t shiftRegister2 = 0x0000; // Example initial value

// Function to Read Analog Input
uint16_t readAnalogInput(uint8_t pin) {
    return analogRead(pin);
}

MCP4822 dac(10);

void setup() {
    // Initialize Pin Modes
    pinMode(GATE_OUT1, OUTPUT);
    pinMode(GATE_OUT2, OUTPUT);
    pinMode(CLOCK_IN, INPUT);

    dac.init();
    dac.turnOnChannelA();
    dac.turnOnChannelB();
    dac.setGainA(MCP4822::High);
    dac.setGainB(MCP4822::High);

    randomSeed(analogRead(7));
}

void loop() {
    // Read the clock input state
    bool currentClockState = digitalRead(CLOCK_IN);
    int lockValue = readAnalogInput(LOCK_PIN);
    int cvA = readAnalogInput(CV_1) * 1.6;
    int cvB = readAnalogInput(CV_2) * 1.6;
    uint16_t lockValueA = (uint16_t)(constrain((int) lockValue + (int) cvA - 500, 0, 1023));
    uint16_t lockValueB = (uint16_t)(constrain((int) lockValue + (int) cvB - 500, 0, 1023));
    
    // Detect rising edge
    if (currentClockState == HIGH && clockState1 == LOW) {
        clockState1 = HIGH;
        dac.setVoltageA(shiftRegister1 >> 4);
        dac.setVoltageB(shiftRegister2 >> 4);
        dac.updateDAC();
        digitalWrite(GATE_OUT1, shiftRegister1 >= 0x8000);
        shiftRegister1 = (shiftRegister1 << 1) | (lockValueA < random(0,2048) ? (shiftRegister1 >> 15) : (~shiftRegister1 >> 15));
        digitalWrite(GATE_OUT2, shiftRegister2 >= 0x8000);
        shiftRegister2 = (shiftRegister2 << 1) | (lockValueB < random(0,2048) ? (shiftRegister2 >> 15) : (~shiftRegister2 >> 15));
    } else if (currentClockState == LOW) {
        // Reset the state when the clock goes low
        clockState1 = LOW;
        digitalWrite(GATE_OUT1, LOW);
        digitalWrite(GATE_OUT2, LOW); 
    }
}
