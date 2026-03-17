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
// scaled CV value that produces zero effect on the lock (analogRead ~309, ~1.5V).
// Gain expressed as integer ratio (17/10 = 1.7) to avoid software float multiply on AVR.
const int CV_GAIN_NUM    = 17;
const int CV_GAIN_DEN    = 10;
const int CV_NEUTRAL_POINT = 525;

// Lock threshold range. Small lower bound ensures the
// potentiometer can reliably achieve a fully-locked sequence at minimum position.
const uint16_t LOCK_MIN = 6;
const uint16_t LOCK_MAX = 1024;

// Global State
volatile uint8_t clockEvent = EVT_NONE;
uint8_t  shiftRegisterLength = 16;
uint16_t srMask = 0xFFFF;  // bitmask for active LFSR window; kept in sync with shiftRegisterLength
bool switchPosition = true;

// Shift Register Arrays — initialised to complementary patterns so both
// channels start active and neither is stuck in an all-zero or all-one state.
uint16_t shiftRegister1 = 0xAAAA;
uint16_t shiftRegister2 = 0x5555;

// Returns true if the mode switch is in the forward position.
bool readSwitchPosition() {
  return analogRead(SWITCH_PIN) > 512;
}

// Returns the active LFSR length (2, 4, 8, or 16) debounced from the steps potentiometer.
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


// ISR: flags a leading-edge event on rising clock, drives gates low immediately on falling edge.
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

// Initialises pins, DAC, clock interrupt, and random seed.
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

// Main loop: advances LFSRs and updates gate/CV outputs on each clock edge.
void loop() {
  cli();
  uint8_t evt = clockEvent;
  clockEvent = EVT_NONE;
  sei();

  if (evt == EVT_LEADING) {
    // NOTE: window placement differs from the original reference sketch.
    // Here the active LFSR bits occupy the BOTTOM of the 16-bit register:
    // bits 0..(shiftRegisterLength-1), with the gate/output bit at position
    // (shiftRegisterLength-1).  The reference places the window at the TOP
    // (bits (16-n)..15, gate = bit 15), but its feedback path is also sourced
    // from bit (n-1) and inserted at bit 0, so the new bit can never propagate
    // into the top window for n < 16 — the sequence freezes after ~n clocks.
    // The bottom-window approach used here is correct for all supported lengths
    // (2, 4, 8, 16).
    bool gate1 = (shiftRegister1 >> (shiftRegisterLength - 1)) & 1;
    bool gate2 = (shiftRegister2 >> (shiftRegisterLength - 1)) & 1;

    // Scale active LFSR window to 12-bit DAC range.
    // Short sequences (n<=12) shift left to fill the range; n=16 shifts right.
    uint16_t cv1 = (shiftRegisterLength <= 12)
        ? ((shiftRegister1 & srMask) << (12 - shiftRegisterLength))
        : ((shiftRegister1 & srMask) >> (shiftRegisterLength - 12));
    uint16_t cv2 = (shiftRegisterLength <= 12)
        ? ((shiftRegister2 & srMask) << (12 - shiftRegisterLength))
        : ((shiftRegister2 & srMask) >> (shiftRegisterLength - 12));

    // Only update CV when the gate fires so CV holds its last value on silent steps,
    // preventing unwanted pitch/mod changes on downstream modules between gates.
    // Track last values so both channels can always be written before updateDAC,
    // preventing uninitialised/stale state being latched on a single-channel fire.
    static uint16_t lastCv1 = 0, lastCv2 = 0;
    if (gate1 || gate2) {
      if (gate1) lastCv1 = cv1;
      if (gate2) lastCv2 = cv2;
      dac.setVoltageA(lastCv1);
      dac.setVoltageB(lastCv2);
      dac.updateDAC();
    }

    // Drive gates using direct port manipulation. GATE_OUT1=pin2=PD2, GATE_OUT2=pin3=PD3.
    if (gate1) PORTD |= (1 << PD2); else PORTD &= ~(1 << PD2);
    if (gate2) PORTD |= (1 << PD3); else PORTD &= ~(1 << PD3);

    int lockValue = analogRead(LOCK_PIN);

    // Compute next state.
    // Each LFSR shifts left within the active window (srMask), then feeds back
    // its own output bit — either kept or inverted — based on lock probability.
    int cvA = (analogRead(CV_1) * CV_GAIN_NUM) / CV_GAIN_DEN;
    int cvB = (analogRead(CV_2) * CV_GAIN_NUM) / CV_GAIN_DEN;
    uint16_t lockValueA = (uint16_t)(constrain((int)lockValue + cvA - CV_NEUTRAL_POINT, 0, 1023));
    uint16_t lockValueB = (uint16_t)(constrain((int)lockValue + cvB - CV_NEUTRAL_POINT, 0, 1023));

    uint16_t newBit1 = (lockValueA < random(LOCK_MIN, LOCK_MAX)) ? gate1 : !gate1;
    shiftRegister1 = ((shiftRegister1 << 1) & srMask) | newBit1;

    // Switch inverts the lock probability for channel 2.
    uint16_t threshold2 = switchPosition ? lockValueB : (1024 - lockValueB);
    uint16_t newBit2 = (threshold2 < random(LOCK_MIN, LOCK_MAX)) ? gate2 : !gate2;
    shiftRegister2 = ((shiftRegister2 << 1) & srMask) | newBit2;
  } else {
    //Length of sequence (2,4,8,16) selected by STEPS_PIN
    shiftRegisterLength = getShiftRegisterLength();
    srMask = (shiftRegisterLength < 16) ? (uint16_t)((1U << shiftRegisterLength) - 1) : 0xFFFF;
    switchPosition = readSwitchPosition();
  }
}
