#include <MCP48xx.h>
#include <YetAnotherPcInt.h>

// Pin Definitions
#define GATE_OUT1 2
#define GATE_OUT2 3
#define ENTROPY_PIN A7
#define CLOCK_PIN 9
#define LOCK_PIN A4
#define STEPS_PIN A2
#define SWITCH_PIN A3
#define CV_1 A0
#define CV_2 A1
#define DAC_PIN 10

MCP4822 dac(DAC_PIN);

#define EVT_NONE     0
#define EVT_LEADING  1

// CV input scaling: gain expands 0-1023 ADC range; neutral offset is the
// scaled CV value that produces zero effect on the lock (analogRead ~309, ~1.5V)
const float CV_GAIN          = 1.7;
const int   CV_NEUTRAL_POINT = 525;

// Global State
volatile uint8_t clockEvent = EVT_NONE;
uint8_t shiftRegisterLength = 16;
bool switchPosition = true;

// Shift Register Arrays
uint16_t shiftRegister1 = 0xAAAA;
uint16_t shiftRegister2 = 0x5555;

// Function to read switch position
bool readSwitchPosition() {
  return analogRead(SWITCH_PIN) > 512;
}

uint8_t getShiftRegisterLength() {
  static uint8_t confirmedLength = 16;
  static uint8_t candidateLength = 16;
  static uint8_t stableCount = 0;
  const uint8_t STABLE_THRESHOLD = 8;

  uint16_t analogValue = analogRead(STEPS_PIN);
  uint8_t reading;
  if (analogValue < 256)      reading = 2;
  else if (analogValue < 512) reading = 4;
  else if (analogValue < 768) reading = 8;
  else                        reading = 16;

  // Confirm that potentiometer is stable (not moving) before read
  if (reading == candidateLength) {
    if (stableCount < STABLE_THRESHOLD) stableCount++;
    if (stableCount == STABLE_THRESHOLD) confirmedLength = candidateLength;
  } else {
    candidateLength = reading;
    stableCount = 0;
  }

  return confirmedLength;
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
  if (clockstate) {
    clockEvent = EVT_LEADING;
  } else {
    // Drive gates low directly in the ISR using port manipulation (faster than
    // digitalWrite) to minimise falling-edge latency to downstream eurorack modules.
    // GATE_OUT1 = pin 2 = PD2, GATE_OUT2 = pin 3 = PD3.
    PORTD &= ~((1 << PD2) | (1 << PD3));
  }
}

void setup() {
  // Initialize Pin Modes
  pinMode(GATE_OUT1, OUTPUT);
  pinMode(GATE_OUT2, OUTPUT);
  pinMode(CLOCK_PIN, INPUT_PULLUP);  // Pullup prevents spurious interrupts when no clock is patched in

  // set up Clock interrupt
  PcInt::attachInterrupt(CLOCK_PIN, doClockCycle, CHANGE);

  dac.init();
  dac.turnOnChannelA();
  dac.turnOnChannelB();
  dac.setGainA(MCP4822::High);
  dac.setGainB(MCP4822::High);

  randomSeed(analogRead(ENTROPY_PIN));
}

void loop() {
  cli();
  uint8_t evt = clockEvent;
  clockEvent = EVT_NONE;
  sei();

  if (evt == EVT_LEADING) {
    // The active LFSR bits are 0..(shiftRegisterLength-1); the current output bit
    // is the MSB of that window, i.e. bit (shiftRegisterLength-1).
    bool gate1 = (shiftRegister1 >> (shiftRegisterLength - 1)) & 1;
    bool gate2 = (shiftRegister2 >> (shiftRegisterLength - 1)) & 1;

    // Only update CV when the gate fires so CV holds its last value on silent steps,
    // preventing unwanted pitch/mod changes on downstream modules between gates.
    if (gate1) dac.setVoltageA(shiftRegister1 >> 4);
    if (gate2) dac.setVoltageB(shiftRegister2 >> 4);
    if (gate1 || gate2) dac.updateDAC();

    // Drive gates using direct port manipulation. GATE_OUT1=pin2=PD2, GATE_OUT2=pin3=PD3.
    if (gate1) PORTD |= (1 << PD2); else PORTD &= ~(1 << PD2);
    if (gate2) PORTD |= (1 << PD3); else PORTD &= ~(1 << PD3);

    int lockValue = analogRead(LOCK_PIN);

    // Compute next state
    int cvA = analogRead(CV_1) * CV_GAIN;
    int cvB = analogRead(CV_2) * CV_GAIN;
    uint16_t lockValueA = (uint16_t)(constrain((int)lockValue + cvA - CV_NEUTRAL_POINT, 0, 1023));
    uint16_t lockValueB = (uint16_t)(constrain((int)lockValue + cvB - CV_NEUTRAL_POINT, 0, 1023));
    shiftRegister1 = clearNthLeftBit(shiftRegister1 << 1, shiftRegisterLength) | (((lockValueA < random(6, 1024)) ? (shiftRegister1 >> (shiftRegisterLength - 1)) : (~shiftRegister1 >> (shiftRegisterLength - 1))) & 1);

    // Invert functionality of lockValue when switch is in reverse position
    if (switchPosition) {
      shiftRegister2 = clearNthLeftBit(shiftRegister2 << 1, shiftRegisterLength) | (((lockValueB < random(6, 1024)) ? (shiftRegister2 >> (shiftRegisterLength - 1)) : (~shiftRegister2 >> (shiftRegisterLength - 1))) & 1);
    } else {
      shiftRegister2 = clearNthLeftBit(shiftRegister2 << 1, shiftRegisterLength) | ((((1024 - lockValueB) < random(6, 1024)) ? (shiftRegister2 >> (shiftRegisterLength - 1)) : (~shiftRegister2 >> (shiftRegisterLength - 1))) & 1);
    }
  } else {
    //Length of sequence (2,4,8,16) selected by STEPS_PIN
    shiftRegisterLength = getShiftRegisterLength();
    switchPosition = readSwitchPosition();
  }
}
