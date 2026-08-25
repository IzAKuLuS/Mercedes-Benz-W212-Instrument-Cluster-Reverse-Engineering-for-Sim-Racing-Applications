/*
  W212 CAN-E unified instrument-cluster explorer

  Purpose:
    1. Test two published cross-model Mercedes ignition profiles on CAN ID 0x001.
    2. Test the W212 E350 DBC's otherwise-undecoded IGNITION message, CAN ID 0x245.
    3. Keep the already-observed W212 cluster wake/OK and door-state frames active.
    4. Provide the confirmed keyboard navigation controls on CAN ID 0x045.
    5. Transmit four equal wheel speeds on CAN ID 0x203 for speedometer tests.

  Hardware target:
    Arduino Mega 2560 + Seeed Studio CAN-BUS Shield V2.0 (MCP2515)

  IMPORTANT:
    - BENCH USE ONLY. Do not run this explorer or its brute-force modes on a
      complete vehicle.
    - Copy the CAN bit rate, MCP2515 clock, and CS pin from the sketch that
      already wakes your cluster. The defaults below are common examples only.
    - Byte numbering is zero-based: byte 0 ... byte 7.

  Serial monitor:
    115200 baud, newline ending.

  High-value commands:
    profile simple       0x001  04 00 00 00 00 00 00 00 @ 100 ms
    profile acc          0x001  C2 80 CF AD AA 07 10 55 @ 100 ms + key frame
    profile ign          0x001  CC 80 CF AD AA 07 10 55 @ 100 ms + key frame
    profile crank        0x001  07 80 CF AD AA 07 10 55 @ 100 ms + key frame
    profile off          0x001  CF 80 CF AD AA 07 10 55 @ 100 ms + key frame
    profile zero         0x001  00 00 00 00 00 00 00 00 @ 100 ms
    sequence start       OFF -> ACC -> IGN -> CRANK -> IGN -> OFF
    speed zero            Send stationary wheel data on 0x203 every 20 ms
    speed mph 80           Command 80 mph on all four wheel-speed fields
    speed raw 2047         Command an exact raw wheel-speed value
    speed off              Send zero once, then stop the speed transmitter

  W212 DBC fallback:
    target 245
    clear
    auto bit             64 single-bit candidates
    auto byte            selected one-byte candidates in all byte positions
    auto nibble          0x00..0x0F in all byte positions

  Other commands:
    help
    status
    wake on | wake off
    ok                   confirmed OK pulse (byte 5 = 0x02)
    button XX            pulse an arbitrary byte-5 value (hex)
    button next          pulse next value in the manual 0x01..0xFF scan
    button reset         reset the manual scan to 0x01
    button release       immediately return bytes 4-5 to 0xFF 0x00
    button4 XX           pulse an arbitrary byte-4 value; byte 5 stays 0x00
    keys on               enter raw keyboard-navigation mode
    keys off              leave keyboard-navigation mode
    !                     toggle keyboard/command mode at any time

  Keyboard-navigation mode:
    W = Up, A = Left, S = Down, D = Right, B = Back, Space = OK
    door closed | door flopen | door raw XX
    key on | key off
    rx on | rx off        print frames transmitted by the cluster
    target 001 | target 245
    set XX XX XX XX XX XX XX XX
    bit N                exact one-bit payload, N = 0..63
    toggle N             combine candidate bits
    period N             active test-frame period, 5..1000 ms
    timing ACTIVE GAP    automatic-test durations in ms
    auto bit | auto byte | auto nibble | auto off
    sequence start | sequence stop

  Source status:
    - 0x001 profiles are cross-model candidates observed in W204/W221 bench
      projects. They are not asserted to be a verified W212 decode.
    - 0x245 is explicitly named IGNITION in the supplied W212 E350 DBC, but the
      DBC contains no signal definitions for its eight payload bytes.
*/

#include <SPI.h>
#include <mcp_can.h>
#include <stdlib.h>
#include <string.h>

// Replace these settings with the values from your already-working sketch.
static const uint8_t CAN_CS_PIN = 9;
#define W212_CAN_SPEED CAN_500KBPS
#define W212_MCP_CLOCK MCP_16MHZ

MCP_CAN CAN0(CAN_CS_PIN);

static const uint16_t ID_DRIVER_CONTROLS = 0x045;
static const uint16_t ID_IGNITION_XMODEL = 0x001;
static const uint16_t ID_IGNITION_W212   = 0x245;
static const uint16_t ID_KEY_STATUS      = 0x2F8;
static const uint16_t ID_DOOR_SENSORS    = 0x283;
static const uint16_t ID_WHEEL_SPEEDS     = 0x203;

static uint8_t wakePayload[8] = {
  0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00
};

// DBC byte-0 bit map:
// bit 0 FL closed, bit 1 FL open, bit 2 FR closed, bit 3 FR open,
// bit 4 RL closed, bit 5 RL open, bit 6 RR closed, bit 7 RR open.
static uint8_t doorPayload[8] = {
  0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t testPayload[8] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t wheelSpeedPayload[8] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t keyStatusPayload[8] = {
  0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t PROFILE_SIMPLE[8] = {
  0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static const uint8_t PROFILE_ACC[8] = {
  0xC2, 0x80, 0xCF, 0xAD, 0xAA, 0x07, 0x10, 0x55
};
static const uint8_t PROFILE_IGN[8] = {
  0xCC, 0x80, 0xCF, 0xAD, 0xAA, 0x07, 0x10, 0x55
};
static const uint8_t PROFILE_CRANK[8] = {
  0x07, 0x80, 0xCF, 0xAD, 0xAA, 0x07, 0x10, 0x55
};
static const uint8_t PROFILE_OFF[8] = {
  0xCF, 0x80, 0xCF, 0xAD, 0xAA, 0x07, 0x10, 0x55
};
static const uint8_t PROFILE_ZERO[8] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static bool wakeEnabled = true;
static bool doorEnabled = true;
static bool testFrameEnabled = true;
static bool keyStatusEnabled = false;
static bool okPulseActive = false;
static bool rxLoggingEnabled = false;
static bool keyboardModeEnabled = false;
static bool wheelSpeedEnabled = false;

static uint16_t activeTestId = ID_IGNITION_XMODEL;
static uint32_t wakePeriodMs = 50;
static uint32_t doorPeriodMs = 100;
static uint32_t testPeriodMs = 100;
static uint32_t keyPeriodMs = 100;
static uint32_t okPulseMs = 140;
static uint32_t wheelSpeedPeriodMs = 20;

static uint32_t lastWakeTx = 0;
static uint32_t lastDoorTx = 0;
static uint32_t lastTestTx = 0;
static uint32_t lastKeyTx = 0;
static uint32_t okPulseStart = 0;
static uint32_t lastWheelSpeedTx = 0;
static uint16_t commandedWheelSpeedRaw = 0;
static uint8_t buttonScanValue = 0x01;

static uint32_t txOkCount = 0;
static uint32_t txErrorCount = 0;

enum AutoMode : uint8_t {
  AUTO_NONE = 0,
  AUTO_SINGLE_BITS,
  AUTO_BYTE_VALUES,
  AUTO_NIBBLE_VALUES
};

enum AutoPhase : uint8_t {
  PHASE_GAP = 0,
  PHASE_ACTIVE
};

static AutoMode autoMode = AUTO_NONE;
static AutoPhase autoPhase = PHASE_GAP;
static uint32_t autoPhaseStart = 0;
static uint32_t autoActiveMs = 2500;
static uint32_t autoGapMs = 1000;
static uint8_t autoBitIndex = 0;
static uint8_t autoByteIndex = 0;
static uint16_t autoValueIndex = 0;

static const uint8_t oneByteValues[] = {
  0x01, 0x02, 0x03, 0x04, 0x07, 0x08,
  0x0F, 0x10, 0x20, 0x40, 0x80, 0xFF
};
static const uint8_t oneByteValueCount =
  sizeof(oneByteValues) / sizeof(oneByteValues[0]);

static bool sequenceActive = false;
static uint8_t sequenceStage = 0;
static uint32_t sequenceStageStart = 0;

static char serialLine[160];
static uint8_t serialLineLength = 0;

static void copyPayload(uint8_t destination[8], const uint8_t source[8]) {
  memcpy(destination, source, 8);
}

static void clearTestPayload() {
  memset(testPayload, 0, sizeof(testPayload));
}

// Pack the experimentally validated extended wheel-speed value into each
// two-byte wheel field. Values through 8191 are accepted; speed mph is limited
// to the cluster's observed 160 mph display range.
static void setAllWheelSpeedsRaw(uint16_t rawSpeed) {
  if (rawSpeed > 0x1FFFU) {
    rawSpeed = 0x1FFFU;
  }

  commandedWheelSpeedRaw = rawSpeed;

  const uint8_t highBits =
    static_cast<uint8_t>((rawSpeed >> 8) & 0x1FU);
  const uint8_t lowByte = static_cast<uint8_t>(rawSpeed & 0xFFU);
  const uint8_t movingFlag = rawSpeed > 0 ? 0x40U : 0x00U;
  const uint8_t firstByte =
    static_cast<uint8_t>(highBits | movingFlag);

  for (uint8_t byteIndex = 0; byteIndex < 8; byteIndex += 2) {
    wheelSpeedPayload[byteIndex] = firstByte;
    wheelSpeedPayload[byteIndex + 1] = lowByte;
  }

  lastWheelSpeedTx = 0;
}

static bool setAllWheelSpeedsMph(uint32_t mph) {
  // DBC scale: mph = raw * 0.0375, so raw = mph * 80 / 3.
  const uint32_t rawSpeed = (mph * 80UL + 1UL) / 3UL;
  if (rawSpeed > 0x1FFFUL) {
    return false;
  }

  setAllWheelSpeedsRaw(static_cast<uint16_t>(rawSpeed));
  return true;
}

static void printHexByte(uint8_t value) {
  if (value < 0x10) {
    Serial.print('0');
  }
  Serial.print(value, HEX);
}

static void printHexId(uint16_t value) {
  Serial.print(F("0x"));
  if (value < 0x100) {
    Serial.print('0');
  }
  if (value < 0x10) {
    Serial.print('0');
  }
  Serial.print(value, HEX);
}

static void printPayload(const uint8_t payload[8]) {
  for (uint8_t i = 0; i < 8; ++i) {
    printHexByte(payload[i]);
    if (i != 7) {
      Serial.print(' ');
    }
  }
}

static bool sendStandardFrame(uint16_t id, const uint8_t payload[8]) {
  const byte result = CAN0.sendMsgBuf(id, 0, 8, const_cast<uint8_t *>(payload));
  if (result == CAN_OK) {
    ++txOkCount;
    return true;
  }

  ++txErrorCount;
  return false;
}

static void printAutoMode() {
  switch (autoMode) {
    case AUTO_SINGLE_BITS:
      Serial.print(F("bit"));
      break;
    case AUTO_BYTE_VALUES:
      Serial.print(F("byte"));
      break;
    case AUTO_NIBBLE_VALUES:
      Serial.print(F("nibble"));
      break;
    default:
      Serial.print(F("off"));
      break;
  }
}

static void printStatus() {
  Serial.println(F("--- STATUS ---"));

  Serial.print(F("wake="));
  Serial.print(wakeEnabled ? F("on") : F("off"));
  Serial.print(F(" period_ms="));
  Serial.print(wakePeriodMs);
  Serial.print(F(" payload="));
  printPayload(wakePayload);
  Serial.println();

  Serial.print(F("door="));
  Serial.print(doorEnabled ? F("on") : F("off"));
  Serial.print(F(" period_ms="));
  Serial.print(doorPeriodMs);
  Serial.print(F(" payload="));
  printPayload(doorPayload);
  Serial.println();

  Serial.print(F("test_frame="));
  Serial.print(testFrameEnabled ? F("on") : F("off"));
  Serial.print(F(" id="));
  printHexId(activeTestId);
  Serial.print(F(" period_ms="));
  Serial.print(testPeriodMs);
  Serial.print(F(" payload="));
  printPayload(testPayload);
  Serial.println();

  Serial.print(F("key_status="));
  Serial.print(keyStatusEnabled ? F("on") : F("off"));
  Serial.print(F(" id="));
  printHexId(ID_KEY_STATUS);
  Serial.print(F(" period_ms="));
  Serial.print(keyPeriodMs);
  Serial.print(F(" payload="));
  printPayload(keyStatusPayload);
  Serial.println();

  Serial.print(F("rx_logging="));
  Serial.println(rxLoggingEnabled ? F("on") : F("off"));

  Serial.print(F("keyboard_mode="));
  Serial.println(keyboardModeEnabled ? F("on") : F("off"));

  Serial.print(F("wheel_speed="));
  Serial.print(wheelSpeedEnabled ? F("on") : F("off"));
  Serial.print(F(" id="));
  printHexId(ID_WHEEL_SPEEDS);
  Serial.print(F(" period_ms="));
  Serial.print(wheelSpeedPeriodMs);
  Serial.print(F(" raw="));
  Serial.print(commandedWheelSpeedRaw);
  Serial.print(F(" payload="));
  printPayload(wheelSpeedPayload);
  Serial.println();

  Serial.print(F("sequence="));
  Serial.print(sequenceActive ? F("active") : F("off"));
  if (sequenceActive) {
    Serial.print(F(" stage="));
    Serial.print(sequenceStage);
  }
  Serial.println();

  Serial.print(F("auto="));
  printAutoMode();
  Serial.print(F(" phase="));
  Serial.print(autoPhase == PHASE_ACTIVE ? F("active") : F("gap"));
  Serial.print(F(" active_ms="));
  Serial.print(autoActiveMs);
  Serial.print(F(" gap_ms="));
  Serial.println(autoGapMs);

  Serial.print(F("tx_ok="));
  Serial.print(txOkCount);
  Serial.print(F(" tx_error="));
  Serial.println(txErrorCount);
  Serial.println(F("--------------"));
}

static void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  help"));
  Serial.println(F("  status"));
  Serial.println(F("  wake on | wake off"));
  Serial.println(F("  ok                         (confirmed byte-5 value 02)"));
  Serial.println(F("  button XX                  (pulse arbitrary hex byte-5 value)"));
  Serial.println(F("  button next                (manual scan, 01 through FF)"));
  Serial.println(F("  button reset               (restart manual scan at 01)"));
  Serial.println(F("  button release             (restore bytes 4-5 to FF 00)"));
  Serial.println(F("  button4 XX                 (pulse byte 4; byte 5 remains 00)"));
  Serial.println(F("  keys on | keys off         (keyboard-navigation mode)"));
  Serial.println(F("  !                          (toggle keyboard/command mode)"));
  Serial.println(F("Keyboard mode: W=Up A=Left S=Down D=Right B=Back Space=OK"));
  Serial.println(F("  speed on | speed off | speed zero"));
  Serial.println(F("  speed mph N       (whole mph, 0..160)"));
  Serial.println(F("  speed raw N       (raw value, 0..8191)"));
  Serial.println(F("  speed period N    (5..1000 ms)"));
  Serial.println(F("  door closed | door flopen | door raw XX"));
  Serial.println(F("  key on | key off"));
  Serial.println(F("  rx on | rx off"));
  Serial.println(F("  profile simple | acc | ign | crank | off | zero"));
  Serial.println(F("  sequence start | sequence stop"));
  Serial.println(F("  target 001 | target 245"));
  Serial.println(F("  clear"));
  Serial.println(F("  set XX XX XX XX XX XX XX XX"));
  Serial.println(F("  bit N        (N=0..63; exact one-bit payload)"));
  Serial.println(F("  toggle N     (N=0..63; combine discovered bits)"));
  Serial.println(F("  period N     (test-frame period 5..1000 ms)"));
  Serial.println(F("  timing ACTIVE GAP"));
  Serial.println(F("  auto bit | auto byte | auto nibble | auto off"));
}

static bool parseUnsigned(const char *token, uint32_t &value) {
  if (token == NULL || *token == '\0') {
    return false;
  }

  char *end = NULL;
  const unsigned long parsed = strtoul(token, &end, 0);
  if (end == token || *end != '\0') {
    return false;
  }

  value = static_cast<uint32_t>(parsed);
  return true;
}

static bool parseHexByte(const char *token, uint8_t &value) {
  if (token == NULL || *token == '\0') {
    return false;
  }

  char *end = NULL;
  const unsigned long parsed = strtoul(token, &end, 16);
  if (end == token || *end != '\0' || parsed > 0xFFUL) {
    return false;
  }

  value = static_cast<uint8_t>(parsed);
  return true;
}

static void beginButtonPulse(uint8_t buttonValue) {
  // DBC: 0x045 DRIVER_CONTROLS, STEERING_WHEEL_BUTTONS at bytes 4-5.
  // Keep the already-confirmed byte 4 wake value at 0xFF and vary byte 5.
  wakePayload[4] = 0xFF;
  wakePayload[5] = buttonValue;
  okPulseActive = true;
  okPulseStart = millis();
  lastWakeTx = 0;

  Serial.print(F("BUTTON_PRESS,id=0x045,byte4=0xFF,byte5=0x"));
  printHexByte(buttonValue);
  Serial.print(F(",hold_ms="));
  Serial.println(okPulseMs);
}

static void beginButton4Pulse(uint8_t button4Value) {
  // Active-low search candidate: vary byte 4 while keeping byte 5 released.
  wakePayload[4] = button4Value;
  wakePayload[5] = 0x00;
  okPulseActive = true;
  okPulseStart = millis();
  lastWakeTx = 0;

  Serial.print(F("BUTTON4_PRESS,id=0x045,byte4=0x"));
  printHexByte(button4Value);
  Serial.print(F(",byte5=0x00,hold_ms="));
  Serial.println(okPulseMs);
}

static void releaseButtonPulse() {
  // Known released/wake state for the two-byte steering-wheel field.
  wakePayload[4] = 0xFF;
  wakePayload[5] = 0x00;
  okPulseActive = false;
  lastWakeTx = 0;
  Serial.println(F("BUTTON_RELEASE,id=0x045,byte4=0xFF,byte5=0x00"));
}

static void cancelSequence(bool setOffProfile) {
  sequenceActive = false;
  if (setOffProfile) {
    activeTestId = ID_IGNITION_XMODEL;
    testPeriodMs = 100;
    keyStatusEnabled = true;
    copyPayload(testPayload, PROFILE_OFF);
    lastTestTx = 0;
    lastKeyTx = 0;
  }
}

static void stopAutomaticTest(bool clearPayload) {
  autoMode = AUTO_NONE;
  autoPhase = PHASE_GAP;
  if (clearPayload) {
    clearTestPayload();
    lastTestTx = 0;
  }
}

static void printSelectedFrame(const __FlashStringHelper *label) {
  Serial.print(F("PROFILE,"));
  Serial.print(label);
  Serial.print(F(",id="));
  printHexId(activeTestId);
  Serial.print(F(",period_ms="));
  Serial.print(testPeriodMs);
  Serial.print(F(",key="));
  Serial.print(keyStatusEnabled ? F("on") : F("off"));
  Serial.print(F(",payload="));
  printPayload(testPayload);
  Serial.println();
}

static void loadProfileInternal(const uint8_t profile[8], bool useKeyFrame,
                                const __FlashStringHelper *label) {
  activeTestId = ID_IGNITION_XMODEL;
  testPeriodMs = 100;
  testFrameEnabled = true;
  keyStatusEnabled = useKeyFrame;
  copyPayload(testPayload, profile);
  lastTestTx = 0;
  lastKeyTx = 0;
  printSelectedFrame(label);
}

static void selectUserProfile(const char *name) {
  stopAutomaticTest(false);
  cancelSequence(false);

  if (strcmp(name, "simple") == 0) {
    loadProfileInternal(PROFILE_SIMPLE, false, F("simple"));
  } else if (strcmp(name, "acc") == 0) {
    loadProfileInternal(PROFILE_ACC, true, F("acc"));
  } else if (strcmp(name, "ign") == 0 || strcmp(name, "ignition") == 0) {
    loadProfileInternal(PROFILE_IGN, true, F("ign"));
  } else if (strcmp(name, "crank") == 0) {
    loadProfileInternal(PROFILE_CRANK, true, F("crank"));
  } else if (strcmp(name, "off") == 0) {
    loadProfileInternal(PROFILE_OFF, true, F("off"));
  } else if (strcmp(name, "zero") == 0) {
    loadProfileInternal(PROFILE_ZERO, false, F("zero"));
  } else {
    Serial.println(F("ERR usage: profile simple|acc|ign|crank|off|zero"));
  }
}

static const __FlashStringHelper *sequenceStageName(uint8_t stage) {
  switch (stage) {
    case 0: return F("OFF");
    case 1: return F("ACC");
    case 2: return F("IGN");
    case 3: return F("CRANK");
    case 4: return F("IGN_AFTER_CRANK");
    case 5: return F("OFF_FINAL");
    default: return F("DONE");
  }
}

static uint32_t sequenceStageDuration(uint8_t stage) {
  switch (stage) {
    case 0: return 1500;
    case 1: return 2000;
    case 2: return 3500;
    case 3: return 450;
    case 4: return 3500;
    case 5: return 1500;
    default: return 0;
  }
}

static void loadSequenceStage(uint8_t stage) {
  switch (stage) {
    case 0:
    case 5:
      loadProfileInternal(PROFILE_OFF, true, sequenceStageName(stage));
      break;
    case 1:
      loadProfileInternal(PROFILE_ACC, true, sequenceStageName(stage));
      break;
    case 2:
    case 4:
      loadProfileInternal(PROFILE_IGN, true, sequenceStageName(stage));
      break;
    case 3:
      loadProfileInternal(PROFILE_CRANK, true, sequenceStageName(stage));
      break;
    default:
      break;
  }

  Serial.print(F("SEQUENCE_STAGE,"));
  Serial.print(stage);
  Serial.print(',');
  Serial.print(sequenceStageName(stage));
  Serial.print(F(",hold_ms="));
  Serial.println(sequenceStageDuration(stage));
}

static void startSequence() {
  stopAutomaticTest(false);
  sequenceActive = true;
  sequenceStage = 0;
  sequenceStageStart = millis();
  loadSequenceStage(sequenceStage);
  Serial.println(F("SEQUENCE_START,OFF->ACC->IGN->CRANK->IGN->OFF"));
}

static void serviceSequence() {
  if (!sequenceActive) {
    return;
  }

  const uint32_t now = millis();
  if (now - sequenceStageStart < sequenceStageDuration(sequenceStage)) {
    return;
  }

  ++sequenceStage;
  if (sequenceStage > 5) {
    sequenceActive = false;
    Serial.println(F("SEQUENCE_COMPLETE,final OFF profile remains active"));
    return;
  }

  sequenceStageStart = now;
  loadSequenceStage(sequenceStage);
}

static void logCurrentAutomaticCandidate() {
  Serial.print(F("TEST,"));
  Serial.print(millis());
  Serial.print(F(",id="));
  printHexId(activeTestId);
  Serial.print(F(",mode="));
  printAutoMode();
  Serial.print(',');

  if (autoMode == AUTO_SINGLE_BITS) {
    Serial.print(F("bit="));
    Serial.print(autoBitIndex);
    Serial.print(F(",byte="));
    Serial.print(autoBitIndex / 8);
    Serial.print(F(",mask=0x"));
    printHexByte(static_cast<uint8_t>(1U << (autoBitIndex % 8)));
  } else {
    Serial.print(F("byte="));
    Serial.print(autoByteIndex);
    Serial.print(F(",value=0x"));
    if (autoMode == AUTO_BYTE_VALUES) {
      printHexByte(oneByteValues[autoValueIndex]);
    } else {
      printHexByte(static_cast<uint8_t>(autoValueIndex));
    }
  }

  Serial.print(F(",period_ms="));
  Serial.print(testPeriodMs);
  Serial.print(F(",payload="));
  printPayload(testPayload);
  Serial.println();
}

static void loadCurrentAutomaticCandidate() {
  clearTestPayload();

  if (autoMode == AUTO_SINGLE_BITS) {
    const uint8_t byteIndex = autoBitIndex / 8;
    const uint8_t bitInByte = autoBitIndex % 8;
    testPayload[byteIndex] = static_cast<uint8_t>(1U << bitInByte);
  } else if (autoMode == AUTO_BYTE_VALUES) {
    testPayload[autoByteIndex] = oneByteValues[autoValueIndex];
  } else if (autoMode == AUTO_NIBBLE_VALUES) {
    testPayload[autoByteIndex] = static_cast<uint8_t>(autoValueIndex);
  }

  logCurrentAutomaticCandidate();
}

static bool advanceAutomaticCandidate() {
  if (autoMode == AUTO_SINGLE_BITS) {
    if (autoBitIndex >= 63) {
      return false;
    }
    ++autoBitIndex;
    return true;
  }

  if (autoMode == AUTO_BYTE_VALUES) {
    ++autoValueIndex;
    if (autoValueIndex >= oneByteValueCount) {
      autoValueIndex = 0;
      ++autoByteIndex;
    }
    return autoByteIndex < 8;
  }

  if (autoMode == AUTO_NIBBLE_VALUES) {
    ++autoValueIndex;
    if (autoValueIndex >= 16) {
      autoValueIndex = 0;
      ++autoByteIndex;
    }
    return autoByteIndex < 8;
  }

  return false;
}

static void startAutomaticTest(AutoMode requestedMode) {
  cancelSequence(false);
  autoMode = requestedMode;
  autoPhase = PHASE_GAP;
  autoPhaseStart = millis();
  autoBitIndex = 0;
  autoByteIndex = 0;
  autoValueIndex = 0;
  clearTestPayload();
  keyStatusEnabled = false;
  lastTestTx = 0;

  Serial.print(F("AUTO_START,id="));
  printHexId(activeTestId);
  Serial.print(F(",mode="));
  printAutoMode();
  Serial.print(F(",active_ms="));
  Serial.print(autoActiveMs);
  Serial.print(F(",gap_ms="));
  Serial.print(autoGapMs);
  Serial.print(F(",period_ms="));
  Serial.println(testPeriodMs);
}

static void serviceAutomaticTest() {
  if (autoMode == AUTO_NONE) {
    return;
  }

  const uint32_t now = millis();
  const uint32_t elapsed = now - autoPhaseStart;

  if (autoPhase == PHASE_GAP) {
    if (elapsed >= autoGapMs) {
      loadCurrentAutomaticCandidate();
      autoPhase = PHASE_ACTIVE;
      autoPhaseStart = now;
      lastTestTx = 0;
    }
    return;
  }

  if (elapsed >= autoActiveMs) {
    clearTestPayload();
    Serial.print(F("GAP,"));
    Serial.print(now);
    Serial.print(F(",id="));
    printHexId(activeTestId);
    Serial.println(F(",payload=00 00 00 00 00 00 00 00"));

    if (!advanceAutomaticCandidate()) {
      Serial.println(F("AUTO_COMPLETE"));
      stopAutomaticTest(true);
      return;
    }

    autoPhase = PHASE_GAP;
    autoPhaseStart = now;
    lastTestTx = 0;
  }
}

static void serviceReceiveFrames() {
  if (!rxLoggingEnabled) {
    return;
  }

  // Limit work per loop so a busy cluster cannot starve periodic transmissions.
  for (uint8_t frameCount = 0; frameCount < 12; ++frameCount) {
    if (CAN0.checkReceive() != CAN_MSGAVAIL) {
      break;
    }

    unsigned long rxId = 0;
    uint8_t rxLength = 0;
    uint8_t rxPayload[8] = {0};
    if (CAN0.readMsgBuf(&rxId, &rxLength, rxPayload) != CAN_OK) {
      Serial.println(F("RX_ERROR,readMsgBuf"));
      break;
    }

    Serial.print(F("RX,"));
    Serial.print(millis());
    Serial.print(F(",id=0x"));
    if ((rxId & 0x1FFFFFFFUL) < 0x100) {
      Serial.print('0');
    }
    if ((rxId & 0x1FFFFFFFUL) < 0x10) {
      Serial.print('0');
    }
    Serial.print(rxId & 0x1FFFFFFFUL, HEX);
    Serial.print(F(",dlc="));
    Serial.print(rxLength);
    Serial.print(F(",payload="));
    for (uint8_t i = 0; i < rxLength && i < 8; ++i) {
      printHexByte(rxPayload[i]);
      if (i + 1 < rxLength && i + 1 < 8) {
        Serial.print(' ');
      }
    }
    Serial.println();
  }
}

static void servicePeriodicFrames() {
  const uint32_t now = millis();

  if (okPulseActive && (now - okPulseStart >= okPulseMs)) {
    releaseButtonPulse();
  }

  if (wakeEnabled && (now - lastWakeTx >= wakePeriodMs)) {
    sendStandardFrame(ID_DRIVER_CONTROLS, wakePayload);
    lastWakeTx = now;
  }

  if (doorEnabled && (now - lastDoorTx >= doorPeriodMs)) {
    sendStandardFrame(ID_DOOR_SENSORS, doorPayload);
    lastDoorTx = now;
  }

  if (testFrameEnabled && (now - lastTestTx >= testPeriodMs)) {
    sendStandardFrame(activeTestId, testPayload);
    lastTestTx = now;
  }

  if (keyStatusEnabled && (now - lastKeyTx >= keyPeriodMs)) {
    sendStandardFrame(ID_KEY_STATUS, keyStatusPayload);
    lastKeyTx = now;
  }

  if (wheelSpeedEnabled && (now - lastWheelSpeedTx >= wheelSpeedPeriodMs)) {
    sendStandardFrame(ID_WHEEL_SPEEDS, wheelSpeedPayload);
    lastWheelSpeedTx = now;
  }
}

static void handleCommand(char *line) {
  char *command = strtok(line, " ,\t");
  if (command == NULL) {
    return;
  }

  if (strcmp(command, "help") == 0) {
    printHelp();
    return;
  }

  if (strcmp(command, "status") == 0) {
    printStatus();
    return;
  }

  if (strcmp(command, "ok") == 0) {
    beginButtonPulse(0x02);
    return;
  }

  if (strcmp(command, "button") == 0 || strcmp(command, "btn") == 0) {
    char *argument = strtok(NULL, " ,\t");

    if (argument == NULL) {
      Serial.println(F("ERR usage: button XX|next|reset|release"));
      return;
    }

    if (strcmp(argument, "next") == 0) {
      const uint8_t testedValue = buttonScanValue;
      beginButtonPulse(testedValue);

      if (buttonScanValue == 0xFF) {
        buttonScanValue = 0x01;
        Serial.println(F("BUTTON_SCAN_WRAP,next=0x01"));
      } else {
        ++buttonScanValue;
        Serial.print(F("BUTTON_SCAN_NEXT,0x"));
        printHexByte(buttonScanValue);
        Serial.println();
      }
      return;
    }

    if (strcmp(argument, "reset") == 0) {
      releaseButtonPulse();
      buttonScanValue = 0x01;
      Serial.println(F("BUTTON_SCAN_RESET,next=0x01"));
      return;
    }

    if (strcmp(argument, "release") == 0) {
      releaseButtonPulse();
      return;
    }

    uint8_t buttonValue = 0;
    if (!parseHexByte(argument, buttonValue)) {
      Serial.println(F("ERR usage: button XX|next|reset|release"));
      return;
    }

    beginButtonPulse(buttonValue);
    return;
  }

  if (strcmp(command, "button4") == 0 || strcmp(command, "btn4") == 0) {
    char *argument = strtok(NULL, " ,\t");
    uint8_t button4Value = 0;

    if (!parseHexByte(argument, button4Value)) {
      Serial.println(F("ERR usage: button4 XX"));
      return;
    }

    beginButton4Pulse(button4Value);
    return;
  }

  if (strcmp(command, "keys") == 0 || strcmp(command, "keyboard") == 0) {
    char *state = strtok(NULL, " ,\t");

    if (state != NULL && strcmp(state, "on") == 0) {
      keyboardModeEnabled = true;
      serialLineLength = 0;
      Serial.println(F("KEYBOARD_MODE,on,W=Up,A=Left,S=Down,D=Right,B=Back,Space=OK"));
    } else if (state != NULL && strcmp(state, "off") == 0) {
      keyboardModeEnabled = false;
      Serial.println(F("KEYBOARD_MODE,off"));
    } else {
      Serial.println(F("ERR usage: keys on|off"));
    }
    return;
  }

  if (strcmp(command, "wake") == 0) {
    char *state = strtok(NULL, " ,\t");
    if (state != NULL && strcmp(state, "on") == 0) {
      wakeEnabled = true;
      lastWakeTx = 0;
      Serial.println(F("wake=on"));
    } else if (state != NULL && strcmp(state, "off") == 0) {
      wakeEnabled = false;
      Serial.println(F("wake=off"));
    } else {
      Serial.println(F("ERR usage: wake on|off"));
    }
    return;
  }

  if (strcmp(command, "door") == 0) {
    char *state = strtok(NULL, " ,\t");
    if (state != NULL && strcmp(state, "closed") == 0) {
      doorPayload[0] = 0x55;
      doorEnabled = true;
      lastDoorTx = 0;
      Serial.println(F("door=all closed, byte0=0x55"));
    } else if (state != NULL && strcmp(state, "flopen") == 0) {
      doorPayload[0] = 0x56;
      doorEnabled = true;
      lastDoorTx = 0;
      Serial.println(F("door=FL open, other three closed, byte0=0x56"));
    } else if (state != NULL && strcmp(state, "raw") == 0) {
      uint8_t rawValue = 0;
      char *valueToken = strtok(NULL, " ,\t");
      if (!parseHexByte(valueToken, rawValue)) {
        Serial.println(F("ERR usage: door raw XX"));
      } else {
        doorPayload[0] = rawValue;
        doorEnabled = true;
        lastDoorTx = 0;
        Serial.print(F("door raw byte0=0x"));
        printHexByte(rawValue);
        Serial.println();
      }
    } else {
      Serial.println(F("ERR usage: door closed|flopen|raw XX"));
    }
    return;
  }

  if (strcmp(command, "key") == 0) {
    char *state = strtok(NULL, " ,\t");
    if (state != NULL && strcmp(state, "on") == 0) {
      keyStatusEnabled = true;
      lastKeyTx = 0;
      Serial.println(F("key_status=on, ID 0x2F8"));
    } else if (state != NULL && strcmp(state, "off") == 0) {
      keyStatusEnabled = false;
      Serial.println(F("key_status=off"));
    } else {
      Serial.println(F("ERR usage: key on|off"));
    }
    return;
  }

  if (strcmp(command, "rx") == 0) {
    char *state = strtok(NULL, " ,\t");
    if (state != NULL && strcmp(state, "on") == 0) {
      rxLoggingEnabled = true;
      Serial.println(F("rx_logging=on"));
    } else if (state != NULL && strcmp(state, "off") == 0) {
      rxLoggingEnabled = false;
      Serial.println(F("rx_logging=off"));
    } else {
      Serial.println(F("ERR usage: rx on|off"));
    }
    return;
  }

  if (strcmp(command, "profile") == 0) {
    char *name = strtok(NULL, " ,\t");
    if (name == NULL) {
      Serial.println(F("ERR usage: profile simple|acc|ign|crank|off|zero"));
    } else {
      selectUserProfile(name);
    }
    return;
  }

  if (strcmp(command, "sequence") == 0 || strcmp(command, "seq") == 0) {
    char *state = strtok(NULL, " ,\t");
    if (state != NULL && strcmp(state, "start") == 0) {
      startSequence();
    } else if (state != NULL && strcmp(state, "stop") == 0) {
      stopAutomaticTest(false);
      cancelSequence(true);
      Serial.println(F("SEQUENCE_STOP,OFF profile selected"));
    } else {
      Serial.println(F("ERR usage: sequence start|stop"));
    }
    return;
  }

  if (strcmp(command, "target") == 0) {
    char *target = strtok(NULL, " ,\t");
    stopAutomaticTest(false);
    cancelSequence(false);
    keyStatusEnabled = false;

    if (target != NULL && (strcmp(target, "001") == 0 || strcmp(target, "1") == 0)) {
      activeTestId = ID_IGNITION_XMODEL;
      testPeriodMs = 100;
      clearTestPayload();
      lastTestTx = 0;
      Serial.println(F("target=0x001, period_ms=100, payload cleared"));
    } else if (target != NULL && strcmp(target, "245") == 0) {
      activeTestId = ID_IGNITION_W212;
      testPeriodMs = 20;
      clearTestPayload();
      lastTestTx = 0;
      Serial.println(F("target=0x245, period_ms=20, payload cleared"));
    } else {
      Serial.println(F("ERR usage: target 001|245"));
    }
    return;
  }

  if (strcmp(command, "clear") == 0 || strcmp(command, "zero") == 0) {
    stopAutomaticTest(false);
    cancelSequence(false);
    clearTestPayload();
    lastTestTx = 0;
    Serial.print(F("test payload cleared on "));
    printHexId(activeTestId);
    Serial.println();
    return;
  }

  if (strcmp(command, "set") == 0) {
    uint8_t candidate[8];
    for (uint8_t i = 0; i < 8; ++i) {
      char *token = strtok(NULL, " ,\t");
      if (!parseHexByte(token, candidate[i])) {
        Serial.println(F("ERR usage: set XX XX XX XX XX XX XX XX"));
        return;
      }
    }

    stopAutomaticTest(false);
    cancelSequence(false);
    copyPayload(testPayload, candidate);
    lastTestTx = 0;
    Serial.print(F("test id="));
    printHexId(activeTestId);
    Serial.print(F(" payload="));
    printPayload(testPayload);
    Serial.println();
    return;
  }

  if (strcmp(command, "bit") == 0 || strcmp(command, "toggle") == 0) {
    const bool toggle = (strcmp(command, "toggle") == 0);
    uint32_t bitNumber = 0;
    char *token = strtok(NULL, " ,\t");
    if (!parseUnsigned(token, bitNumber) || bitNumber > 63) {
      Serial.println(F("ERR usage: bit N or toggle N, N=0..63"));
      return;
    }

    stopAutomaticTest(false);
    cancelSequence(false);
    const uint8_t byteIndex = static_cast<uint8_t>(bitNumber / 8);
    const uint8_t mask = static_cast<uint8_t>(1U << (bitNumber % 8));

    if (!toggle) {
      clearTestPayload();
      testPayload[byteIndex] = mask;
    } else {
      testPayload[byteIndex] ^= mask;
    }

    lastTestTx = 0;
    Serial.print(F("test id="));
    printHexId(activeTestId);
    Serial.print(F(" payload="));
    printPayload(testPayload);
    Serial.println();
    return;
  }

  if (strcmp(command, "speed") == 0) {
    char *mode = strtok(NULL, " ,\t");

    if (mode == NULL) {
      Serial.println(F("ERR usage: speed on|off|zero|mph N|raw N|period N"));
      return;
    }

    if (strcmp(mode, "on") == 0) {
      wheelSpeedEnabled = true;
      lastWheelSpeedTx = 0;
      Serial.println(F("wheel_speed=on"));
      return;
    }

    if (strcmp(mode, "off") == 0) {
      setAllWheelSpeedsRaw(0);
      sendStandardFrame(ID_WHEEL_SPEEDS, wheelSpeedPayload);
      wheelSpeedEnabled = false;
      Serial.println(F("wheel_speed=off, final zero frame sent"));
      return;
    }

    if (strcmp(mode, "zero") == 0) {
      setAllWheelSpeedsRaw(0);
      wheelSpeedEnabled = true;
      Serial.println(F("wheel_speed=on, raw=0, moving flags cleared"));
      return;
    }

    if (strcmp(mode, "mph") == 0) {
      uint32_t mph = 0;
      char *valueToken = strtok(NULL, " ,\t");

      if (!parseUnsigned(valueToken, mph) ||
          mph > 160UL ||
          !setAllWheelSpeedsMph(mph)) {
        Serial.println(F("ERR speed must be 0..160 mph"));
        return;
      }

      wheelSpeedEnabled = true;
      lastWheelSpeedTx = 0;
      Serial.print(F("wheel_speed=on, mph="));
      Serial.print(mph);
      Serial.print(F(", raw="));
      Serial.print(commandedWheelSpeedRaw);
      Serial.print(F(", payload="));
      printPayload(wheelSpeedPayload);
      Serial.println();
      return;
    }

    if (strcmp(mode, "raw") == 0) {
      uint32_t rawSpeed = 0;
      char *valueToken = strtok(NULL, " ,\t");

      if (!parseUnsigned(valueToken, rawSpeed) || rawSpeed > 0x1FFFUL) {
        Serial.println(F("ERR speed raw must be 0..8191"));
        return;
      }

      setAllWheelSpeedsRaw(static_cast<uint16_t>(rawSpeed));
      wheelSpeedEnabled = true;
      lastWheelSpeedTx = 0;
      Serial.print(F("wheel_speed=on, raw="));
      Serial.print(commandedWheelSpeedRaw);
      Serial.print(F(", payload="));
      printPayload(wheelSpeedPayload);
      Serial.println();
      return;
    }

    if (strcmp(mode, "period") == 0) {
      uint32_t period = 0;
      char *valueToken = strtok(NULL, " ,\t");

      if (!parseUnsigned(valueToken, period) || period < 5 || period > 1000) {
        Serial.println(F("ERR speed period must be 5..1000 ms"));
        return;
      }

      wheelSpeedPeriodMs = period;
      lastWheelSpeedTx = 0;
      Serial.print(F("wheel_speed period_ms="));
      Serial.println(wheelSpeedPeriodMs);
      return;
    }

    Serial.println(F("ERR usage: speed on|off|zero|mph N|raw N|period N"));
    return;
  }

  if (strcmp(command, "period") == 0) {
    uint32_t period = 0;
    char *token = strtok(NULL, " ,\t");
    if (!parseUnsigned(token, period) || period < 5 || period > 1000) {
      Serial.println(F("ERR period must be 5..1000 ms"));
      return;
    }

    testPeriodMs = period;
    lastTestTx = 0;
    Serial.print(F("test period_ms="));
    Serial.println(testPeriodMs);
    return;
  }

  if (strcmp(command, "timing") == 0) {
    uint32_t active = 0;
    uint32_t gap = 0;
    char *activeToken = strtok(NULL, " ,\t");
    char *gapToken = strtok(NULL, " ,\t");
    if (!parseUnsigned(activeToken, active) ||
        !parseUnsigned(gapToken, gap) ||
        active < 250 || active > 30000 ||
        gap < 100 || gap > 30000) {
      Serial.println(F("ERR usage: timing ACTIVE GAP; active 250..30000, gap 100..30000 ms"));
      return;
    }

    autoActiveMs = active;
    autoGapMs = gap;
    Serial.print(F("timing active_ms="));
    Serial.print(autoActiveMs);
    Serial.print(F(" gap_ms="));
    Serial.println(autoGapMs);
    return;
  }

  if (strcmp(command, "auto") == 0) {
    char *mode = strtok(NULL, " ,\t");
    if (mode != NULL && strcmp(mode, "bit") == 0) {
      startAutomaticTest(AUTO_SINGLE_BITS);
    } else if (mode != NULL && strcmp(mode, "byte") == 0) {
      startAutomaticTest(AUTO_BYTE_VALUES);
    } else if (mode != NULL && strcmp(mode, "nibble") == 0) {
      startAutomaticTest(AUTO_NIBBLE_VALUES);
    } else if (mode != NULL && strcmp(mode, "off") == 0) {
      stopAutomaticTest(true);
      Serial.println(F("AUTO_STOP,payload cleared"));
    } else {
      Serial.println(F("ERR usage: auto bit|byte|nibble|off"));
    }
    return;
  }

  Serial.println(F("ERR unknown command; type help"));
}

static void serviceSerialInput() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());

    // Exclamation mark always toggles modes, even without pressing Enter.
    if (c == '!') {
      keyboardModeEnabled = !keyboardModeEnabled;
      serialLineLength = 0;
      Serial.print(F("KEYBOARD_MODE,"));
      Serial.println(keyboardModeEnabled ? F("on") : F("off"));
      continue;
    }

    if (keyboardModeEnabled) {
      // CR/LF is ignored so this works with both raw terminals and the
      // Arduino Serial Monitor's line-ending setting.
      if (c == '\r' || c == '\n') {
        continue;
      }

      switch (c) {
        case 'w':
        case 'W':
          beginButton4Pulse(0xFE);
          Serial.println(F("KEY,Up"));
          break;

        case 'a':
        case 'A':
          beginButton4Pulse(0xF7);
          Serial.println(F("KEY,Left"));
          break;

        case 's':
        case 'S':
          beginButton4Pulse(0xFD);
          Serial.println(F("KEY,Down"));
          break;

        case 'd':
        case 'D':
          beginButton4Pulse(0xFB);
          Serial.println(F("KEY,Right"));
          break;

        case 'b':
        case 'B':
          beginButtonPulse(0xEF);
          Serial.println(F("KEY,Back"));
          break;

        case ' ':
          beginButtonPulse(0x02);
          Serial.println(F("KEY,OK"));
          break;

        default:
          Serial.println(F("KEY_UNKNOWN,use W A S D B Space; !=command mode"));
          break;
      }
      continue;
    }

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      serialLine[serialLineLength] = '\0';
      handleCommand(serialLine);
      serialLineLength = 0;
      continue;
    }

    if (serialLineLength < sizeof(serialLine) - 1) {
      serialLine[serialLineLength++] = c;
    } else {
      serialLineLength = 0;
      Serial.println(F("ERR input line too long"));
    }
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

  Serial.println();
  Serial.println(F("W212 CAN-E unified cluster explorer"));
  Serial.println(F("BENCH USE ONLY. Confirm CAN speed, MCP clock, and CS pin."));

  while (CAN0.begin(MCP_ANY, W212_CAN_SPEED, W212_MCP_CLOCK) != CAN_OK) {
    Serial.println(F("CAN init failed; retrying in 500 ms"));
    delay(500);
  }

  CAN0.setMode(MCP_NORMAL);
  Serial.println(F("CAN controller initialized in normal mode"));
  printHelp();
  printStatus();
}

void loop() {
  serviceSerialInput();
  serviceSequence();
  serviceAutomaticTest();
  servicePeriodicFrames();
  serviceReceiveFrames();
}



