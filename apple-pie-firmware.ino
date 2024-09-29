#include <MCP48xx.h>

// Pin Definitions
const int GATE_OUT1 = 2;
const int GATE_OUT2 = 3;
const int XTAL = 7;
const int CLOCK_PIN = 9;
const int LOCK_PIN = A4;
const int CV_1 = A0;
const int CV_2 = A1;
const int STEPS_PIN = A2;
const int SWITCH_PIN = A3;
const int DAC_PIN = 10;

MCP4822 dac(DAC_PIN);

// Global Setting Values
volatile uint16_t lockValueA = 0;
volatile uint16_t lockValueB = 0;
volatile uint8_t shiftRegisterLength = 16;
volatile bool switchPosition = true;

// Shift Register Arrays
volatile uint16_t shiftRegister1 = 0x0000;
volatile uint16_t shiftRegister2 = 0x0000;
uint16_t last_cv_1 = -1;
uint16_t last_cv_2 = -1;

// Function to read switch position
bool readSwitchPosition() {
    return analogRead(SWITCH_PIN) > 512;
}

uint8_t getShiftRegisterLength() {
    // Read the analog value from STEPS_PIN (range 0 to 1023)
    uint16_t analogValue = analogRead(STEPS_PIN);

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

void doClockCycle() {
  // Rising edge
  if (!digitalRead(CLOCK_PIN)) {
        
        digitalWrite(GATE_OUT1, shiftRegister1 >= 0x8000);
        shiftRegister1 = clearNthLeftBit((shiftRegister1 << 1), shiftRegisterLength) | (lockValueA < random(0,2048) ? (shiftRegister1 >> (shiftRegisterLength - 1)) : (~shiftRegister1 >> (shiftRegisterLength - 1)));
        digitalWrite(GATE_OUT2, shiftRegister2 >= 0x8000);
        // Invert functionality of lockValue when switch is in reverse position
        if (switchPosition) {
          shiftRegister2 = clearNthLeftBit((shiftRegister2 << 1), shiftRegisterLength) | (lockValueB < random(0,2048) ? (shiftRegister2 >> (shiftRegisterLength - 1)) : (~shiftRegister2 >> (shiftRegisterLength - 1)));
        } else {
          shiftRegister2 = clearNthLeftBit((shiftRegister2 << 1), shiftRegisterLength) | ((1024 - lockValueB) < random(0,2048) ? (shiftRegister2 >> (shiftRegisterLength - 1)) : (~shiftRegister2 >> (shiftRegisterLength - 1)));
        }
    } else {
        digitalWrite(GATE_OUT1, LOW);
        digitalWrite(GATE_OUT2, LOW); 
    }
}

void setup() {
    // Initialize Pin Modes
    pinMode(GATE_OUT1, OUTPUT);
    pinMode(GATE_OUT2, OUTPUT);
    pinMode(CLOCK_PIN, INPUT);

    dac.init();
    dac.turnOnChannelA();
    dac.turnOnChannelB();
    dac.setGainA(MCP4822::High);
    dac.setGainB(MCP4822::High);

  randomSeed(analogRead(XTAL));

  // set up Arduino interrupt
  attachInterrupt(digitalPinToInterrupt(CLOCK_PIN), doClockCycle, CHANGE);
}

void loop() {
    int lockValue = analogRead(LOCK_PIN);
    int cvA = analogRead(CV_1) * 1.7;
    int cvB = analogRead(CV_2) * 1.7;

    //Set lock values
    lockValueA = (uint16_t)(constrain((int) lockValue + (int) cvA - 525, 0, 1023));
    lockValueB = (uint16_t)(constrain((int) lockValue + (int) cvB - 525, 0, 1023));

    //Length of sequence (2,4,8,16) selected by STEPS_PIN
    shiftRegisterLength = getShiftRegisterLength();
    switchPosition = readSwitchPosition();

    //Send new CV if shiftRegisters are changed
    uint16_t new_cv_1 = shiftRegister1 >> 4;
    uint16_t new_cv_2 = shiftRegister2 >> 4;
    if (last_cv_1 != new_cv_1) {
      last_cv_1 = new_cv_1;
      dac.setVoltageA(last_cv_1);
    }
    if (last_cv_2 != new_cv_2) {
      last_cv_2 = new_cv_2;
      dac.setVoltageB(new_cv_2);
    }
    dac.updateDAC();
}
