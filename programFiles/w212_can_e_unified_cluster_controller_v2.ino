/*
  W212 CAN-E unified instrument-cluster controller

  Hardware:
    Arduino Mega 2560 + Seeed Studio CAN-BUS Shield V2.0 (MCP2515)
    CAN E at 500 kbit/s, MCP2515 clock 16 MHz, CS pin 9
    Serial Monitor at 115200 baud with a newline ending

  BENCH USE ONLY.

  Retained commands:
    help
    moreHelp
    status
    rx on | rx off
    wake on | wake off
    !
    speed on | speed off | speed zero
    speed mph N
    speed raw N
    speed period N
    door closed | door flopen | door raw XX
    target HHH
    clear
    set XX XX XX XX XX XX XX XX
    bit N
    toggle N
    period N
    timing ACTIVE GAP

  Keyboard-navigation mode:
    Press ! at any time to toggle keyboard mode.
    W = Up, A = Left, S = Down, D = Right, B = Back, Space = OK

  Target IDs:
    HHH must be exactly three hexadecimal digits from 001 through 999.
    IDs 001 through 7FF are sent as standard 11-bit CAN frames.
    IDs 800 through 999 are sent as extended CAN frames because they do not
    fit in an 11-bit standard identifier.

  Byte numbering is zero-based: byte 0 through byte 7.
*/

#include <SPI.h>
#include <mcp_can.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t CAN_CS_PIN = 9;
#define W212_CAN_SPEED CAN_500KBPS
#define W212_MCP_CLOCK MCP_16MHZ

MCP_CAN CAN0(CAN_CS_PIN);

static const uint16_t ID_DRIVER_CONTROLS = 0x045;
static const uint16_t ID_WHEEL_SPEEDS = 0x203;
static const uint16_t ID_DOOR_SENSORS = 0x283;

// 0x045 display-wake / steering-control carrier.
static uint8_t wakePayload[8] = {
  0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00
};

// 0x283 byte-0 door map from the project DBC:
// bit 0 FL closed, bit 1 FL open, bit 2 FR closed, bit 3 FR open,
// bit 4 RL closed, bit 5 RL open, bit 6 RR closed, bit 7 RR open.
static uint8_t doorPayload[8] = {
  0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Generic payload transmitted on the selected target ID.
static uint8_t testPayload[8] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Four equal wheel-speed fields for CAN ID 0x203.
static uint8_t wheelSpeedPayload[8] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static bool wakeEnabled = true;
static bool doorEnabled = true;
static bool testFrameEnabled = true;
static bool wheelSpeedEnabled = false;
static bool rxLoggingEnabled = false;
static bool keyboardModeEnabled = false;
static bool navigationPulseActive = false;

static uint16_t activeTestId = 0x001;
static bool activeTestExtended = false;
static uint16_t commandedWheelSpeedRaw = 0;

static uint32_t wakePeriodMs = 50;
static uint32_t doorPeriodMs = 100;
static uint32_t testPeriodMs = 100;
static uint32_t wheelSpeedPeriodMs = 20;
static uint32_t navigationPulseMs = 140;

// Retained timing values from the explorer's timed-test configuration.
// The trimmed controller stores and reports them but does not include an
// automatic sweep command.
static uint32_t timedTestActiveMs = 2500;
static uint32_t timedTestGapMs = 1000;

static uint32_t lastWakeTx = 0;
static uint32_t lastDoorTx = 0;
static uint32_t lastTestTx = 0;
static uint32_t lastWheelSpeedTx = 0;
static uint32_t navigationPulseStart = 0;

static uint32_t txOkCount = 0;
static uint32_t txErrorCount = 0;
static uint32_t rxOkCount = 0;
static uint32_t rxErrorCount = 0;

static char serialLine[160];
static uint8_t serialLineLength = 0;

static void printHexByte(uint8_t value) {
  if (value < 0x10U) {
    Serial.print('0');
  }
  Serial.print(value, HEX);
}

static void printHexId(uint32_t value) {
  Serial.print(F("0x"));
  if (value < 0x100U) {
    Serial.print('0');
  }
  if (value < 0x10U) {
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

static void clearTestPayload() {
  memset(testPayload, 0, sizeof(testPayload));
}

static bool sendFrame(uint32_t id, bool extended,
                      const uint8_t payload[8]) {
  const byte result = CAN0.sendMsgBuf(
    id,
    extended ? 1 : 0,
    8,
    const_cast<uint8_t *>(payload)
  );

  if (result == CAN_OK) {
    ++txOkCount;
    return true;
  }

  ++txErrorCount;
  return false;
}

static bool sendStandardFrame(uint16_t id, const uint8_t payload[8]) {
  return sendFrame(id, false, payload);
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

static bool parseThreeDigitHexId(const char *token, uint16_t &value) {
  if (token == NULL || *token == '\0') {
    return false;
  }

  const char *digits = token;
  if (digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) {
    digits += 2;
  }

  if (strlen(digits) != 3U) {
    return false;
  }

  for (uint8_t i = 0; i < 3; ++i) {
    if (!isxdigit(static_cast<unsigned char>(digits[i]))) {
      return false;
    }
  }

  char *end = NULL;
  const unsigned long parsed = strtoul(digits, &end, 16);
  if (end == digits || *end != '\0' ||
      parsed < 0x001UL || parsed > 0x999UL) {
    return false;
  }

  value = static_cast<uint16_t>(parsed);
  return true;
}

// Pack the experimentally validated 13-bit wheel-speed value into each
// two-byte field. The moving flag is set for nonzero speed.
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
  // Project scale: mph = raw * 0.0375, so raw = mph * 80 / 3.
  const uint32_t rawSpeed = (mph * 80UL + 1UL) / 3UL;
  if (rawSpeed > 0x1FFFUL) {
    return false;
  }

  setAllWheelSpeedsRaw(static_cast<uint16_t>(rawSpeed));
  return true;
}

static void releaseNavigationPulse(bool printMessage) {
  wakePayload[4] = 0xFF;
  wakePayload[5] = 0x00;
  navigationPulseActive = false;
  lastWakeTx = 0;

  if (printMessage) {
    Serial.println(F("KEY_RELEASE,id=0x045,byte4=0xFF,byte5=0x00"));
  }
}

static void beginNavigationPulse(uint8_t byte4Value, uint8_t byte5Value,
                                 const __FlashStringHelper *name) {
  wakePayload[4] = byte4Value;
  wakePayload[5] = byte5Value;
  navigationPulseActive = true;
  navigationPulseStart = millis();
  lastWakeTx = 0;

  Serial.print(F("KEY,"));
  Serial.print(name);
  Serial.print(F(",id=0x045,byte4=0x"));
  printHexByte(byte4Value);
  Serial.print(F(",byte5=0x"));
  printHexByte(byte5Value);
  Serial.print(F(",hold_ms="));
  Serial.println(navigationPulseMs);
}

static void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  help"));
  Serial.println(F("  moreHelp"));
  Serial.println(F("  status"));
  Serial.println(F("  rx on | rx off"));
  Serial.println(F("  wake on | wake off"));
  Serial.println(F("  !"));
  Serial.println(F("  speed on | speed off | speed zero"));
  Serial.println(F("  speed mph N | speed raw N | speed period N"));
  Serial.println(F("  door closed | door flopen | door raw XX"));
  Serial.println(F("  target HHH"));
  Serial.println(F("  clear"));
  Serial.println(F("  set XX XX XX XX XX XX XX XX"));
  Serial.println(F("  bit N | toggle N"));
  Serial.println(F("  period N"));
  Serial.println(F("  timing ACTIVE GAP"));
}

static void printMoreHelp() {
  Serial.println(F("--- COMMAND GUIDE ---"));
  Serial.println(F("help: Print the compact command list."));
  Serial.println(F("moreHelp: Explain what each command does."));
  Serial.println(F("status: Show active IDs, payloads, periods, modes, and counters."));
  Serial.println(F("rx on/off: Start or stop printing frames sent by the cluster."));
  Serial.println(F("wake on/off: Start or stop the 0x045 display-wake frame."));
  Serial.println(F("!: Toggle keyboard navigation immediately; no Enter is needed."));
  Serial.println(F("  Keyboard mode: W=Up A=Left S=Down D=Right B=Back Space=OK."));
  Serial.println(F("speed on: Resume the current 0x203 wheel-speed payload."));
  Serial.println(F("speed off: Send one zero-speed frame, then stop 0x203."));
  Serial.println(F("speed zero: Keep sending a stationary 0x203 frame."));
  Serial.println(F("speed mph N: Send 0..160 mph to all four wheel fields."));
  Serial.println(F("speed raw N: Send raw wheel speed 0..8191."));
  Serial.println(F("speed period N: Set the 0x203 period to 5..1000 ms."));
  Serial.println(F("door closed: Report all four doors closed on 0x283."));
  Serial.println(F("door flopen: Report the front-left door open on 0x283."));
  Serial.println(F("door raw XX: Put hex byte XX in byte 0 of 0x283."));
  Serial.println(F("target HHH: Select and clear a generic test ID, 001..999."));
  Serial.println(F("  Targets above 7FF are transmitted as extended CAN frames."));
  Serial.println(F("clear: Zero all eight bytes of the generic target payload."));
  Serial.println(F("set: Replace all eight bytes of the generic target payload."));
  Serial.println(F("bit N: Clear the payload, then set only bit N, where N=0..63."));
  Serial.println(F("toggle N: Flip bit N without clearing the other payload bits."));
  Serial.println(F("period N: Set the generic target period to 5..1000 ms."));
  Serial.println(F("timing A G: Store active/gap test times in milliseconds."));
  Serial.println(F("  This trimmed build does not start an automatic sweep."));
  Serial.println(F("---------------------"));
}

static void printStatus() {
  Serial.println(F("--- STATUS ---"));

  Serial.print(F("wake="));
  Serial.print(wakeEnabled ? F("on") : F("off"));
  Serial.print(F(" id="));
  printHexId(ID_DRIVER_CONTROLS);
  Serial.print(F(" period_ms="));
  Serial.print(wakePeriodMs);
  Serial.print(F(" payload="));
  printPayload(wakePayload);
  Serial.println();

  Serial.print(F("keyboard_mode="));
  Serial.print(keyboardModeEnabled ? F("on") : F("off"));
  Serial.print(F(" pulse="));
  Serial.print(navigationPulseActive ? F("active") : F("released"));
  Serial.print(F(" hold_ms="));
  Serial.println(navigationPulseMs);

  Serial.print(F("door="));
  Serial.print(doorEnabled ? F("on") : F("off"));
  Serial.print(F(" id="));
  printHexId(ID_DOOR_SENSORS);
  Serial.print(F(" period_ms="));
  Serial.print(doorPeriodMs);
  Serial.print(F(" payload="));
  printPayload(doorPayload);
  Serial.println();

  Serial.print(F("test_frame="));
  Serial.print(testFrameEnabled ? F("on") : F("off"));
  Serial.print(F(" id="));
  printHexId(activeTestId);
  Serial.print(F(" format="));
  Serial.print(activeTestExtended ? F("extended") : F("standard"));
  Serial.print(F(" period_ms="));
  Serial.print(testPeriodMs);
  Serial.print(F(" payload="));
  printPayload(testPayload);
  Serial.println();

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

  Serial.print(F("timing active_ms="));
  Serial.print(timedTestActiveMs);
  Serial.print(F(" gap_ms="));
  Serial.println(timedTestGapMs);

  Serial.print(F("rx_logging="));
  Serial.println(rxLoggingEnabled ? F("on") : F("off"));

  Serial.print(F("tx_ok="));
  Serial.print(txOkCount);
  Serial.print(F(" tx_error="));
  Serial.print(txErrorCount);
  Serial.print(F(" rx_ok="));
  Serial.print(rxOkCount);
  Serial.print(F(" rx_error="));
  Serial.println(rxErrorCount);

  Serial.println(F("--------------"));
}

static void serviceReceiveFrames() {
  if (!rxLoggingEnabled) {
    return;
  }

  // Limit each batch so RX printing cannot starve periodic transmissions.
  for (uint8_t frameCount = 0; frameCount < 12; ++frameCount) {
    if (CAN0.checkReceive() != CAN_MSGAVAIL) {
      break;
    }

    unsigned long rawId = 0;
    uint8_t rxLength = 0;
    uint8_t rxPayload[8] = {0};

    if (CAN0.readMsgBuf(&rawId, &rxLength, rxPayload) != CAN_OK) {
      ++rxErrorCount;
      Serial.println(F("RX_ERROR,readMsgBuf"));
      break;
    }

    ++rxOkCount;

    const bool extended = (rawId & 0x80000000UL) != 0;
    const bool remote = (rawId & 0x40000000UL) != 0;
    const uint32_t id = rawId & 0x1FFFFFFFUL;

    Serial.print(F("RX,"));
    Serial.print(millis());
    Serial.print(F(",id="));
    printHexId(id);
    Serial.print(F(",format="));
    Serial.print(extended ? F("extended") : F("standard"));
    Serial.print(F(",rtr="));
    Serial.print(remote ? F("yes") : F("no"));
    Serial.print(F(",dlc="));
    Serial.print(rxLength);
    Serial.print(F(",payload="));

    const uint8_t shownLength = rxLength > 8 ? 8 : rxLength;
    for (uint8_t i = 0; i < shownLength; ++i) {
      printHexByte(rxPayload[i]);
      if (i + 1 < shownLength) {
        Serial.print(' ');
      }
    }
    Serial.println();
  }
}

static void servicePeriodicFrames() {
  const uint32_t now = millis();

  if (navigationPulseActive &&
      now - navigationPulseStart >= navigationPulseMs) {
    releaseNavigationPulse(true);
  }

  if (wakeEnabled && now - lastWakeTx >= wakePeriodMs) {
    sendStandardFrame(ID_DRIVER_CONTROLS, wakePayload);
    lastWakeTx = now;
  }

  if (doorEnabled && now - lastDoorTx >= doorPeriodMs) {
    sendStandardFrame(ID_DOOR_SENSORS, doorPayload);
    lastDoorTx = now;
  }

  if (testFrameEnabled && now - lastTestTx >= testPeriodMs) {
    sendFrame(activeTestId, activeTestExtended, testPayload);
    lastTestTx = now;
  }

  if (wheelSpeedEnabled &&
      now - lastWheelSpeedTx >= wheelSpeedPeriodMs) {
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

  if (strcmp(command, "moreHelp") == 0) {
    printMoreHelp();
    return;
  }

  if (strcmp(command, "status") == 0) {
    printStatus();
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

  if (strcmp(command, "wake") == 0) {
    char *state = strtok(NULL, " ,\t");
    if (state != NULL && strcmp(state, "on") == 0) {
      wakeEnabled = true;
      lastWakeTx = 0;
      Serial.println(F("wake=on"));
    } else if (state != NULL && strcmp(state, "off") == 0) {
      releaseNavigationPulse(false);
      wakeEnabled = false;
      Serial.println(F("wake=off"));
    } else {
      Serial.println(F("ERR usage: wake on|off"));
    }
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

      if (!parseUnsigned(valueToken, period) ||
          period < 5UL || period > 1000UL) {
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

  if (strcmp(command, "target") == 0) {
    uint16_t requestedId = 0;
    char *targetToken = strtok(NULL, " ,\t");

    if (!parseThreeDigitHexId(targetToken, requestedId)) {
      Serial.println(F("ERR usage: target HHH; HHH must be 001..999 hex"));
      return;
    }

    activeTestId = requestedId;
    activeTestExtended = requestedId > 0x7FFU;
    testFrameEnabled = true;
    clearTestPayload();

    // Preserve the historical explorer defaults for its two original IDs.
    if (requestedId == 0x001U) {
      testPeriodMs = 100;
    } else if (requestedId == 0x245U) {
      testPeriodMs = 20;
    }

    lastTestTx = 0;

    Serial.print(F("target="));
    printHexId(activeTestId);
    Serial.print(F(", format="));
    Serial.print(activeTestExtended ? F("extended") : F("standard"));
    Serial.print(F(", period_ms="));
    Serial.print(testPeriodMs);
    Serial.println(F(", payload cleared"));

    if (activeTestExtended) {
      Serial.println(F("NOTE: target is above 0x7FF, so it uses an extended frame."));
    }
    return;
  }

  if (strcmp(command, "clear") == 0) {
    testFrameEnabled = true;
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

    if (strtok(NULL, " ,\t") != NULL) {
      Serial.println(F("ERR set accepts exactly eight bytes"));
      return;
    }

    memcpy(testPayload, candidate, sizeof(testPayload));
    testFrameEnabled = true;
    lastTestTx = 0;

    Serial.print(F("test id="));
    printHexId(activeTestId);
    Serial.print(F(" payload="));
    printPayload(testPayload);
    Serial.println();
    return;
  }

  if (strcmp(command, "bit") == 0 || strcmp(command, "toggle") == 0) {
    const bool toggleBit = strcmp(command, "toggle") == 0;
    uint32_t bitNumber = 0;
    char *token = strtok(NULL, " ,\t");

    if (!parseUnsigned(token, bitNumber) || bitNumber > 63UL) {
      Serial.println(F("ERR usage: bit N or toggle N, N=0..63"));
      return;
    }

    const uint8_t byteIndex = static_cast<uint8_t>(bitNumber / 8UL);
    const uint8_t mask =
      static_cast<uint8_t>(1U << static_cast<uint8_t>(bitNumber % 8UL));

    if (!toggleBit) {
      clearTestPayload();
      testPayload[byteIndex] = mask;
    } else {
      testPayload[byteIndex] ^= mask;
    }

    testFrameEnabled = true;
    lastTestTx = 0;

    Serial.print(F("test id="));
    printHexId(activeTestId);
    Serial.print(F(" payload="));
    printPayload(testPayload);
    Serial.println();
    return;
  }

  if (strcmp(command, "period") == 0) {
    uint32_t period = 0;
    char *token = strtok(NULL, " ,\t");

    if (!parseUnsigned(token, period) ||
        period < 5UL || period > 1000UL) {
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
        active < 250UL || active > 30000UL ||
        gap < 100UL || gap > 30000UL) {
      Serial.println(F("ERR usage: timing ACTIVE GAP; active 250..30000, gap 100..30000 ms"));
      return;
    }

    timedTestActiveMs = active;
    timedTestGapMs = gap;

    Serial.print(F("timing active_ms="));
    Serial.print(timedTestActiveMs);
    Serial.print(F(" gap_ms="));
    Serial.println(timedTestGapMs);
    return;
  }

  Serial.println(F("ERR unknown command; type help"));
}

static void serviceSerialInput() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());

    // The exclamation mark always toggles modes without requiring Enter.
    if (c == '!') {
      keyboardModeEnabled = !keyboardModeEnabled;
      serialLineLength = 0;

      if (!keyboardModeEnabled && navigationPulseActive) {
        releaseNavigationPulse(false);
      }

      Serial.print(F("KEYBOARD_MODE,"));
      Serial.println(keyboardModeEnabled ? F("on") : F("off"));
      continue;
    }

    if (keyboardModeEnabled) {
      // Ignore line endings so raw terminals and Serial Monitor both work.
      if (c == '\r' || c == '\n') {
        continue;
      }

      switch (c) {
        case 'w':
        case 'W':
          beginNavigationPulse(0xFE, 0x00, F("Up"));
          break;

        case 'a':
        case 'A':
          beginNavigationPulse(0xF7, 0x00, F("Left"));
          break;

        case 's':
        case 'S':
          beginNavigationPulse(0xFD, 0x00, F("Down"));
          break;

        case 'd':
        case 'D':
          beginNavigationPulse(0xFB, 0x00, F("Right"));
          break;

        case 'b':
        case 'B':
          beginNavigationPulse(0xFF, 0xEF, F("Back"));
          break;

        case ' ':
          beginNavigationPulse(0xFF, 0x02, F("OK"));
          break;

        default:
          Serial.println(F("KEY_UNKNOWN,use W A S D B Space; press ! for commands"));
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

    if (serialLineLength < sizeof(serialLine) - 1U) {
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
  Serial.println(F("W212 CAN-E unified cluster controller v2"));
  Serial.println(F("BENCH USE ONLY. CAN 500 kbit/s, MCP2515 16 MHz, CS pin 9."));

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
  servicePeriodicFrames();
  serviceReceiveFrames();
}
