#include <MCP48xx.h>
#include <YetAnotherPcInt.h>

// Pin Definitions
#define GATE_OUT1 2
#define GATE_OUT2 3
#define XTAL 7
#define CLOCK_PIN 9 //PCINT22?
#define LOCK_PIN A4
#define CV_1 A0
#define CV_2 A1
#define STEPS_PIN A2
#define SWITCH_PIN A3
#define DAC_PIN 10

//Debug via serial
//#define DEBUG true

MCP4822 dac(DAC_PIN);

// Global Setting Values
uint16_t lockValueA = 0;
uint16_t lockValueB = 0;
uint8_t shiftRegisterLength = 16;
bool switchPosition = true;
volatile bool clockLeading = false;
volatile bool clockTrailing = false;

// Shift Register Arrays
uint16_t shiftRegister1 = 0x0000;
uint16_t shiftRegister2 = 0x0000;
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

void doClockCycle(bool clockstate) {
  // Rising edge
  if (clockstate) {
    clockLeading = true;
    clockTrailing = false;
  } else {
    clockTrailing = true;
    clockLeading = false;
  }
}

void setup() {
  #ifdef DEBUG
  Serial.begin(115200);
  Serial.println("Starting...");
  #endif

  // Initialize Pin Modes
  pinMode(GATE_OUT1, OUTPUT);
  pinMode(GATE_OUT2, OUTPUT);

  // set up Clock interrupt
  pinMode(CLOCK_PIN, INPUT_PULLUP);
  PcInt::attachInterrupt(CLOCK_PIN, doClockCycle, CHANGE);

  dac.init();
  dac.turnOnChannelA();
  dac.turnOnChannelB();
  dac.setGainA(MCP4822::High);
  dac.setGainB(MCP4822::High);

  randomSeed(analogRead(XTAL));
}

void loop() {

  int lockValue = analogRead(LOCK_PIN);
  int cvA = analogRead(CV_1) * 1.7;
  int cvB = analogRead(CV_2) * 1.7;

  //Set lock values
  lockValueA = (uint16_t)(constrain((int) lockValue + (int) cvA - 525, 0, 1023));
  lockValueB = (uint16_t)(constrain((int) lockValue + (int) cvB - 525, 0, 1023));

  if (clockLeading)
  {
    #ifdef DEBUG
    if (shiftRegister1 >= 0x8000) Serial.println("Sending Gate A");
    #endif
    shiftRegister1 = clearNthLeftBit((shiftRegister1 << 1), shiftRegisterLength) | (lockValueA < random(0,2048) ? (shiftRegister1 >> (shiftRegisterLength - 1)) : (~shiftRegister1 >> (shiftRegisterLength - 1)));
    #ifdef DEBUG
    if (shiftRegister2 >= 0x8000) Serial.println("Sending Gate B");
    #endif
    // Invert functionality of lockValue when switch is in reverse position
    if (readSwitchPosition()) {
      shiftRegister2 = clearNthLeftBit((shiftRegister2 << 1), shiftRegisterLength) | (lockValueB < random(0,2048) ? (shiftRegister2 >> (shiftRegisterLength - 1)) : (~shiftRegister2 >> (shiftRegisterLength - 1)));
    } else {
      shiftRegister2 = clearNthLeftBit((shiftRegister2 << 1), shiftRegisterLength) | ((1024 - lockValueB) < random(0,2048) ? (shiftRegister2 >> (shiftRegisterLength - 1)) : (~shiftRegister2 >> (shiftRegisterLength - 1)));
    }
    //Send new CV if shiftRegisters are changed
    uint16_t new_cv_1 = shiftRegister1 >> 4;
    uint16_t new_cv_2 = shiftRegister2 >> 4;
    
    if (last_cv_1 != new_cv_1) {
      dac.setVoltageA(new_cv_1);
      last_cv_1 = new_cv_1;
      #ifdef DEBUG
      Serial.print("New CV1: ");
      Serial.println(new_cv_1);  
      #endif
    }
    if (last_cv_2 != new_cv_2) {
      dac.setVoltageB(new_cv_2);
      last_cv_2 = new_cv_2;
      #ifdef DEBUG
      Serial.print("New CV2: ");
      Serial.println(new_cv_2);  
      #endif
    }
    dac.updateDAC();
    digitalWrite(GATE_OUT1, shiftRegister1 >= 0x8000);
    digitalWrite(GATE_OUT2, shiftRegister2 >= 0x8000);
    clockLeading = false;
  }
  else if (clockTrailing)
  {
    digitalWrite(GATE_OUT1, LOW);
    digitalWrite(GATE_OUT2, LOW);
    clockTrailing = false;
  }

  //Length of sequence (2,4,8,16) selected by STEPS_PIN
  shiftRegisterLength = getShiftRegisterLength();
  switchPosition = readSwitchPosition();
}
