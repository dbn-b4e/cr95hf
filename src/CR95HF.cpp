/**
 * @file    CR95HF.cpp
 * @brief   CR95HF NFC/RFID Transceiver Driver Implementation
 * @author  B4E SRL - David Baldwin
 * @date    November 2025
 * @version 1.0.0
 *
 * Implementation of the CR95HF driver for ISO14443-A tag reading.
 *
 * @copyright Copyright (c) 2025 B4E SRL. All rights reserved.
 */

#include "CR95HF.h"

// ============================================================================
// Constructor
// ============================================================================

/**
 * @brief Construct CR95HF driver instance
 * @param port HardwareSerial reference
 * @param rxPin RX pin (from CR95HF TXD)
 * @param txPin TX pin (to CR95HF RXD)
 * @param baudRate Baud rate (57600 for CR95HF)
 */
CR95HF::CR95HF(HardwareSerial& port, int rxPin, int txPin, uint32_t baudRate)
    : _port(&port), _rxPin(rxPin), _txPin(txPin), _baud(baudRate), _debug(false)
{
    memset(lastATQA, 0, sizeof(lastATQA));
    memset(deviceName, 0, sizeof(deviceName));
}

// ============================================================================
// Debug Helpers
// ============================================================================

/**
 * @brief Print debug message if debug mode enabled
 * @param msg Message to print
 */
void CR95HF::log(const char* msg) {
    if (_debug) Serial.print(msg);
}

/**
 * @brief Print hex dump if debug mode enabled
 * @param prefix Prefix string
 * @param data Data to dump
 * @param len Data length
 */
void CR95HF::logHex(const char* prefix, const uint8_t* data, uint8_t len) {
    if (!_debug) return;
    Serial.print(prefix);
    for (uint8_t i = 0; i < len; i++) {
        if (data[i] < 0x10) Serial.print('0');
        Serial.print(data[i], HEX);
        Serial.print(' ');
    }
    Serial.println();
}

// ============================================================================
// Low-Level Communication
// ============================================================================

/**
 * @brief Flush serial receive buffer
 */
void CR95HF::flushRx() {
    while (_port->available()) _port->read();
}

/**
 * @brief Send frame to CR95HF
 * @param frame Frame to send
 */
void CR95HF::sendFrame(const CR95HF_Frame& frame) {
    flushRx();
    _port->write(frame.data, frame.len);
    logHex("[TX] ", frame.data, frame.len);
}

/**
 * @brief Read response from CR95HF
 * @param code Output: response code
 * @param buf Output: response data
 * @param len Input: buffer size, Output: data length
 * @param timeoutMs Timeout in milliseconds
 * @return true if response received, false on timeout
 */
bool CR95HF::readResponse(uint8_t& code, uint8_t* buf, uint8_t& len, uint32_t timeoutMs) {
    uint32_t start = millis();

    // Wait for response code
    while (!_port->available()) {
        if (millis() - start > timeoutMs) {
            log("[RX] Timeout waiting for code\n");
            return false;
        }
    }
    code = _port->read();

    // Wait for length byte
    while (!_port->available()) {
        if (millis() - start > timeoutMs) {
            log("[RX] Timeout waiting for length\n");
            return false;
        }
    }
    uint8_t respLen = _port->read();

    // Read payload
    uint8_t n = 0;
    while (n < respLen) {
        if (_port->available()) {
            buf[n++] = _port->read();
        } else if (millis() - start > timeoutMs) {
            log("[RX] Timeout reading payload\n");
            return false;
        }
    }
    len = n;

    if (_debug) {
        Serial.printf("[RX] Code=0x%02X Len=%d ", code, len);
        logHex("Data=", buf, len);
    }

    return true;
}

// ============================================================================
// Echo Test
// ============================================================================

/**
 * @brief Perform echo test to verify CR95HF communication
 * @return true if CR95HF responds with echo
 */
bool CR95HF::echoTest() {
    flushRx();
    _port->write((uint8_t)CR95HF_CMD_ECHO);

    uint32_t start = millis();
    while (millis() - start < 50) {
        if (_port->available()) {
            uint8_t resp = _port->read();
            if (resp == CR95HF_CMD_ECHO) {
                log("[CR95HF] Echo OK\n");
                return true;
            }
        }
    }
    log("[CR95HF] Echo FAILED\n");
    return false;
}

// ============================================================================
// Initialization
// ============================================================================

/**
 * @brief Initialize CR95HF transceiver
 * @param debug Enable debug output
 * @return true if initialization successful
 */
bool CR95HF::begin(bool debug) {
    _debug = debug;

    // CR95HF requires UART 8N2 (2 stop bits!) - critical!
    _port->begin(_baud, SERIAL_8N2, _rxPin, _txPin);
    delay(20);  // Wait for UART stabilization
    flushRx();

    // Step 1: Echo test - verify basic communication
    if (!echoTest()) {
        return false;
    }

    // Step 2: Get IDN - read device identification
    _txFrame.buildIDN();
    sendFrame(_txFrame);

    uint8_t code, buf[32], len = sizeof(buf);
    if (!readResponse(code, buf, len, 100) || code != CR95HF_RSP_SUCCESS || len < 10) {
        log("[CR95HF] IDN failed\n");
        return false;
    }

    // Store device name (null-terminate)
    uint8_t copyLen = (len < sizeof(deviceName) - 1) ? len : sizeof(deviceName) - 1;
    memcpy(deviceName, buf, copyLen);
    deviceName[copyLen] = '\0';

    if (_debug) {
        Serial.printf("[CR95HF] Device: %s\n", deviceName);
    }

    // Step 3: Select ISO14443A protocol
    if (!protocolSelectA()) {
        log("[CR95HF] Protocol select failed\n");
        return false;
    }
    log("[CR95HF] ISO14443A ready\n");

    return true;
}

// ============================================================================
// Protocol Selection
// ============================================================================

/**
 * @brief Select ISO14443-A protocol
 * @return true if protocol selected successfully
 */
bool CR95HF::protocolSelectA() {
    _txFrame.buildProtocolSelect(CR95HF_PROTO_ISO14443A, 0x00);
    sendFrame(_txFrame);

    uint8_t code, buf[8], len = sizeof(buf);
    if (!readResponse(code, buf, len, 50)) return false;
    return code == CR95HF_RSP_SUCCESS;
}

// ============================================================================
// REQA / WUPA - Tag Wake Commands
// ============================================================================

/**
 * @brief Send REQA or WUPA command
 * @param cmd Command (ISO14443A_REQA or ISO14443A_WUPA)
 * @param atqa1 Output: ATQA byte 1
 * @param atqa2 Output: ATQA byte 2
 * @return true if tag responded
 */
bool CR95HF::sendReqWup(uint8_t cmd, uint8_t& atqa1, uint8_t& atqa2) {
    if (cmd == ISO14443A_WUPA) {
        _txFrame.buildWUPA();
    } else {
        _txFrame.buildREQA();
    }
    sendFrame(_txFrame);

    uint8_t code, buf[16], len = sizeof(buf);
    if (!readResponse(code, buf, len, 20)) return false;

    // Check for valid ATQA response
    if (code != CR95HF_RSP_DATA || len < 2) return false;

    atqa1 = buf[0];
    atqa2 = buf[1];
    return true;
}

// ============================================================================
// Anticollision & Select - Cascade Level 1
// ============================================================================

/**
 * @brief Perform anticollision for cascade level 1
 * @param cl1 Output: 5 bytes (UID[0-3] + BCC)
 * @return true if successful
 */
bool CR95HF::anticollCL1(uint8_t* cl1) {
    _txFrame.buildAnticollCL1();
    sendFrame(_txFrame);

    uint8_t code, buf[16], len = sizeof(buf);
    if (!readResponse(code, buf, len, 50)) return false;
    if (code != CR95HF_RSP_DATA || len < 5) return false;

    memcpy(cl1, buf, 5);  // 4 UID bytes + BCC
    return true;
}

/**
 * @brief Select tag with cascade level 1 UID
 * @param cl1 5 bytes: UID[0-3] + BCC
 * @param sak Output: SAK byte
 * @return true if successful
 */
bool CR95HF::selectCL1(const uint8_t* cl1, uint8_t& sak) {
    _txFrame.buildSelectCL1(cl1);
    sendFrame(_txFrame);

    uint8_t code, buf[8], len = sizeof(buf);
    if (!readResponse(code, buf, len, 50)) return false;
    if (code != CR95HF_RSP_DATA || len < 1) return false;

    sak = buf[0];
    return true;
}

// ============================================================================
// Anticollision & Select - Cascade Level 2
// ============================================================================

/**
 * @brief Perform anticollision for cascade level 2
 * @param cl2 Output: 5 bytes (UID[3-6] + BCC)
 * @return true if successful
 */
bool CR95HF::anticollCL2(uint8_t* cl2) {
    _txFrame.buildAnticollCL2();
    sendFrame(_txFrame);

    uint8_t code, buf[16], len = sizeof(buf);
    if (!readResponse(code, buf, len, 50)) return false;
    if (code != CR95HF_RSP_DATA || len < 5) return false;

    memcpy(cl2, buf, 5);
    return true;
}

/**
 * @brief Select tag with cascade level 2 UID
 * @param cl2 5 bytes: UID[3-6] + BCC
 * @param sak Output: SAK byte
 * @return true if successful
 */
bool CR95HF::selectCL2(const uint8_t* cl2, uint8_t& sak) {
    _txFrame.buildSelectCL2(cl2);
    sendFrame(_txFrame);

    uint8_t code, buf[8], len = sizeof(buf);
    if (!readResponse(code, buf, len, 50)) return false;
    if (code != CR95HF_RSP_DATA || len < 1) return false;

    sak = buf[0];
    return true;
}

// ============================================================================
// Get UID (4-byte or 7-byte)
// ============================================================================

/**
 * @brief Read ISO14443-A tag UID
 * @param uid Output buffer (min 10 bytes)
 * @param uidLen Output: UID length (4 or 7)
 * @param sakOut Output: SAK byte
 * @return true if tag detected and UID read
 *
 * Algorithm:
 * 1. Send WUPA (wake all tags), fallback to REQA
 * 2. Perform anticollision CL1
 * 3. Select CL1
 * 4. If cascade tag (0x88), continue to CL2
 * 5. Extract full UID from CL1 and CL2 responses
 */
bool CR95HF::iso14443aGetUID(uint8_t* uid, uint8_t& uidLen, uint8_t& sakOut) {
    uidLen = 0;
    sakOut = 0;

    // Try WUPA first (wakes halted cards), then REQA
    uint8_t atqa1 = 0, atqa2 = 0;
    bool gotAtqa = sendReqWup(ISO14443A_WUPA, atqa1, atqa2);
    if (!gotAtqa) {
        gotAtqa = sendReqWup(ISO14443A_REQA, atqa1, atqa2);
    }
    if (!gotAtqa) {
        return false;  // No tag in field
    }

    // Store ATQA for debugging
    lastATQA[0] = atqa1;
    lastATQA[1] = atqa2;

    // Anticollision CL1
    uint8_t cl1[5];
    if (!anticollCL1(cl1)) {
        return false;
    }

    // Select CL1
    uint8_t sak1 = 0;
    if (!selectCL1(cl1, sak1)) {
        return false;
    }

    // Check for cascade tag (0x88 = more UID bytes follow)
    if (cl1[0] != ISO14443A_CT) {
        // 4-byte UID (single size) - most common for MIFARE Classic
        uid[0] = cl1[0];
        uid[1] = cl1[1];
        uid[2] = cl1[2];
        uid[3] = cl1[3];
        uidLen = 4;
        sakOut = sak1;
        return true;
    }

    // 7-byte UID: first 3 bytes from CL1 (skip cascade tag 0x88)
    uid[0] = cl1[1];
    uid[1] = cl1[2];
    uid[2] = cl1[3];

    // Anticollision CL2
    uint8_t cl2[5];
    if (!anticollCL2(cl2)) {
        return false;
    }

    // Select CL2
    uint8_t sak2 = 0;
    if (!selectCL2(cl2, sak2)) {
        return false;
    }

    // Remaining 4 bytes from CL2
    uid[3] = cl2[0];
    uid[4] = cl2[1];
    uid[5] = cl2[2];
    uid[6] = cl2[3];
    uidLen = 7;
    sakOut = sak2;

    return true;
}

/**
 * @brief Read ISO14443-A tag UID (without SAK)
 * @param uid Output buffer (min 10 bytes)
 * @param uidLen Output: UID length
 * @return true if tag detected
 */
bool CR95HF::iso14443aGetUID(uint8_t* uid, uint8_t& uidLen) {
    uint8_t sak;
    return iso14443aGetUID(uid, uidLen, sak);
}

// ============================================================================
// Card Type from SAK
// ============================================================================

/**
 * @brief Get human-readable card type from SAK byte
 * @param sak SAK byte value
 * @return Card type string
 */
const char* CR95HF::getCardType(uint8_t sak) {
    switch (sak) {
        case SAK_MIFARE_UL:      return "MIFARE Ultralight/NTAG";
        case SAK_MIFARE_1K:      return "MIFARE Classic 1K";
        case SAK_MIFARE_MINI:    return "MIFARE Mini";
        case SAK_MIFARE_4K:      return "MIFARE Classic 4K";
        case SAK_MIFARE_PLUS_2K: return "MIFARE Plus 2K";
        case SAK_MIFARE_PLUS_4K: return "MIFARE Plus 4K";
        case SAK_MIFARE_PLUS:    return "MIFARE Plus/DESFire";
        case SAK_JCOP:           return "JCOP/SmartMX";
        case SAK_MIFARE_4K_EMU:  return "MIFARE Classic 4K (emu)";
        case SAK_MIFARE_1K_INF:  return "MIFARE Classic 1K (Infineon)";
        case SAK_MIFARE_PRO:     return "MIFARE ProX";
        default:                 return "Unknown";
    }
}

// ============================================================================
// Read IDN
// ============================================================================

/**
 * @brief Read device identification string
 * @param out Output buffer
 * @param maxLen Maximum buffer length
 * @return true if successful
 */
bool CR95HF::readIDN(char* out, uint8_t maxLen) {
    if (!out || maxLen == 0) return false;

    _txFrame.buildIDN();
    sendFrame(_txFrame);

    uint8_t code, buf[32], len = sizeof(buf);
    if (!readResponse(code, buf, len, 100)) return false;
    if (code != CR95HF_RSP_SUCCESS) return false;

    uint8_t copyLen = (len < maxLen - 1) ? len : (maxLen - 1);
    memcpy(out, buf, copyLen);
    out[copyLen] = '\0';

    return true;
}

// ============================================================================
// Self Test
// ============================================================================

/**
 * @brief Run comprehensive self-test
 * @note Results printed to Serial
 */
void CR95HF::selfTest() {
    Serial.println("\n=== CR95HF Self-Test ===");

    // Echo test
    bool echoOk = echoTest();
    Serial.printf("  Echo:     %s\n", echoOk ? "OK" : "FAIL");

    // IDN
    char idn[32];
    bool idnOk = readIDN(idn, sizeof(idn));
    Serial.printf("  IDN:      %s\n", idnOk ? idn : "FAIL");

    // Protocol select
    bool protoOk = protocolSelectA();
    Serial.printf("  Protocol: %s\n", protoOk ? "ISO14443A OK" : "FAIL");

    // RF Field test
    uint8_t a1, a2;
    bool fieldOk = sendReqWup(ISO14443A_WUPA, a1, a2) ||
                   sendReqWup(ISO14443A_REQA, a1, a2);

    if (fieldOk) {
        Serial.printf("  RF Field: OK (tag present, ATQA=%02X%02X)\n", a1, a2);
    } else {
        // No tag - re-select protocol to ensure field is on
        protocolSelectA();

        // Send REQA just to check field response
        _txFrame.buildREQA();
        sendFrame(_txFrame);

        uint8_t code, buf[8], len = sizeof(buf);
        if (readResponse(code, buf, len, 50)) {
            if (code == CR95HF_RSP_TIMEOUT) {
                Serial.println("  RF Field: OK (no tag)");
            } else {
                Serial.printf("  RF Field: response 0x%02X\n", code);
            }
        } else {
            Serial.println("  RF Field: FAIL (no response)");
        }
    }

    Serial.println("========================\n");
}

// ============================================================================
// Measure Field Level
// ============================================================================

/**
 * @brief Measure RF field level
 * @param level Output: 0=off, 50=on/no tag, 100=tag present
 * @return true if measurement successful
 */
bool CR95HF::measureFieldLevel(uint8_t& level) {
    // Re-select protocol to ensure field is on
    if (!protocolSelectA()) {
        level = 0;
        return false;
    }

    // Try WUPA/REQA
    uint8_t a1, a2;
    if (sendReqWup(ISO14443A_WUPA, a1, a2) || sendReqWup(ISO14443A_REQA, a1, a2)) {
        level = 100;  // Tag present = strong field
        return true;
    }

    // No tag - check if field is at least active
    _txFrame.buildREQA();
    sendFrame(_txFrame);

    uint8_t code, buf[8], len = sizeof(buf);
    if (readResponse(code, buf, len, 50)) {
        if (code == CR95HF_RSP_TIMEOUT) {
            level = 50;  // Field on, no tag
        } else {
            level = 25;  // Some response
        }
    } else {
        level = 0;  // No response at all
    }

    return true;
}

// ============================================================================
// Antenna Check
// ============================================================================

/**
 * @brief Check if antenna/RF field is operational
 * @return true if antenna OK
 */
bool CR95HF::antennaOK() {
    uint8_t level = 0;
    if (!measureFieldLevel(level)) return false;
    return level > 0;
}
