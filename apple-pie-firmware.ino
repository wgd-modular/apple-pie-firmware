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

// Function to Read Analog Input
bool readSwitchPosition() {
    return analogRead(A3) > 512;
}

uint8_t getShiftRegisterLength() {
    // Read the analog value from pin A2 (range 0 to 1023)
    uint16_t analogValue = analogRead(A2);

    // Map the analog value to one of the specific return values: 2, 4, 8, or 16
    if (analogValue < 256) {
        return 2;
    } else if (analogValue < 512) {
        return 4;
    } else if (analogValue < 768) {
        return 8;
    } else {
        return 16;
    }
}

uint16_t clearNthLeftBit(uint16_t value, uint8_t n) {
    // Calculate the bit position from the left
    uint8_t bitPosition = 16 - n;

    // Create a mask with all bits set to 1 except the nth leftmost bit
    uint16_t mask = ~(1 << bitPosition);

    // Clear the nth leftmost bit by applying the mask
    return value & mask;
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
    int cvA = readAnalogInput(CV_1) * 1.7;
    int cvB = readAnalogInput(CV_2) * 1.7;
    uint16_t lockValueA = (uint16_t)(constrain((int) lockValue + (int) cvA - 520, 0, 1023));
    uint16_t lockValueB = (uint16_t)(constrain((int) lockValue + (int) cvB - 520, 0, 1023));
    uint8_t shiftRegisterLength = getShiftRegisterLength();
    
    // Detect rising edge
    if (currentClockState == HIGH && clockState1 == LOW) {
        clockState1 = HIGH;
        
        dac.setVoltageA(shiftRegister1 >> 4);
        dac.setVoltageB(shiftRegister2 >> 4);
        dac.updateDAC();
        
        digitalWrite(GATE_OUT1, shiftRegister1 >= 0x8000);
        shiftRegister1 = clearNthLeftBit((shiftRegister1 << 1), shiftRegisterLength) | (lockValueA < random(0,2048) ? (shiftRegister1 >> (shiftRegisterLength - 1)) : (~shiftRegister1 >> (shiftRegisterLength - 1)));
        digitalWrite(GATE_OUT2, shiftRegister2 >= 0x8000);
        // Invert functionality of lockValue when switch is in reverse position
        if (readSwitchPosition()) {
          shiftRegister2 = clearNthLeftBit((shiftRegister2 << 1), shiftRegisterLength) | (lockValueB < random(0,2048) ? (shiftRegister2 >> (shiftRegisterLength - 1)) : (~shiftRegister2 >> (shiftRegisterLength - 1)));
        } else {
          shiftRegister2 = clearNthLeftBit((shiftRegister2 << 1), shiftRegisterLength) | ((1024 - lockValueB) < random(0,2048) ? (shiftRegister2 >> (shiftRegisterLength - 1)) : (~shiftRegister2 >> (shiftRegisterLength - 1)));
        }
    } else if (currentClockState == LOW) {
        // Reset the state when the clock goes low
        clockState1 = LOW;
        digitalWrite(GATE_OUT1, LOW);
        digitalWrite(GATE_OUT2, LOW); 
    }
}
