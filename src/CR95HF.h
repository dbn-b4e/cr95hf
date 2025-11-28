/**
 * @file    CR95HF.h
 * @brief   CR95HF NFC/RFID Transceiver Driver for Arduino
 * @author  B4E SRL - David Baldwin
 * @date    November 2025
 * @version 1.0.0
 *
 * Driver for STMicroelectronics CR95HF 13.56 MHz multi-protocol contactless
 * transceiver IC. Supports ISO14443-A (NFC-A) protocol for reading MIFARE,
 * NTAG, and other ISO14443-A compatible cards.
 *
 * Key features:
 * - UART communication (57600 baud, 8N2)
 * - ISO14443-A anticollision and selection
 * - 4-byte and 7-byte UID support
 * - SAK-based card type identification
 * - Frame builder for protocol commands
 *
 * Hardware connection (UART mode):
 * - CR95HF SSI_0 and SSI_1 must be tied to GND
 * - TXD (CR95HF) -> RX (MCU)
 * - RXD (CR95HF) <- TX (MCU)
 * - UART: 57600 baud, 8N2 (2 stop bits required!)
 *
 * @note    CR95HF requires UART 8N2 (2 stop bits) - this is critical!
 * @warning Do not use 8N1 - communication will fail
 *
 * @code
 * #include <CR95HF.h>
 *
 * CR95HF nfc(Serial1, 1, 2, 57600);
 *
 * void setup() {
 *     Serial.begin(115200);
 *     if (!nfc.begin()) {
 *         Serial.println("CR95HF init failed!");
 *         while (1);
 *     }
 * }
 *
 * void loop() {
 *     uint8_t uid[10];
 *     uint8_t uidLen, sak;
 *     if (nfc.iso14443aGetUID(uid, uidLen, sak)) {
 *         Serial.print("Tag detected: ");
 *         Serial.println(nfc.getCardType(sak));
 *     }
 *     delay(150);
 * }
 * @endcode
 *
 * @copyright Copyright (c) 2025 B4E SRL. All rights reserved.
 */

#pragma once

#include <Arduino.h>

// ============================================================================
// CR95HF Command Codes (Host -> CR95HF)
// Reference: CR95HF Datasheet Section 5.2
// ============================================================================

#define CR95HF_CMD_IDN          0x01    ///< Get device identification string
#define CR95HF_CMD_PROTOCOL     0x02    ///< Select RF protocol
#define CR95HF_CMD_SENDRECV     0x04    ///< Send data and receive response from tag
#define CR95HF_CMD_IDLE         0x07    ///< Enter low-power idle mode
#define CR95HF_CMD_RDREG        0x08    ///< Read analog configuration register
#define CR95HF_CMD_WRREG        0x09    ///< Write analog configuration register
#define CR95HF_CMD_ECHO         0x55    ///< Echo test command (returns 0x55)

// ============================================================================
// CR95HF Response Codes (CR95HF -> Host)
// Reference: CR95HF Datasheet Section 5.4
// ============================================================================

#define CR95HF_RSP_SUCCESS      0x00    ///< Command executed successfully
#define CR95HF_RSP_DATA         0x80    ///< Data received from tag
#define CR95HF_RSP_INVALID_LEN  0x82    ///< Invalid command length
#define CR95HF_RSP_INVALID_CMD  0x83    ///< Invalid/unsupported command code
#define CR95HF_RSP_TIMEOUT      0x87    ///< Frame wait timeout (no tag in field)
#define CR95HF_RSP_COLLISION    0x88    ///< Collision detected during anticollision
#define CR95HF_RSP_FRAMEERR     0x8F    ///< Framing error in received data

// ============================================================================
// CR95HF Protocol Codes
// Reference: CR95HF Datasheet Section 5.3
// ============================================================================

#define CR95HF_PROTO_OFF        0x00    ///< RF field off
#define CR95HF_PROTO_ISO15693   0x01    ///< ISO15693 (vicinity cards)
#define CR95HF_PROTO_ISO14443A  0x02    ///< ISO14443-A (NFC-A, MIFARE, NTAG)
#define CR95HF_PROTO_ISO14443B  0x03    ///< ISO14443-B (NFC-B)
#define CR95HF_PROTO_FELICA     0x04    ///< FeliCa (NFC-F)

// ============================================================================
// ISO14443-A RF Commands
// Reference: ISO/IEC 14443-3A
// ============================================================================

#define ISO14443A_REQA          0x26    ///< Request Type A (wake idle tags only)
#define ISO14443A_WUPA          0x52    ///< Wake-Up Type A (wake all tags including halted)
#define ISO14443A_HLTA_B1       0x50    ///< HALT command byte 1
#define ISO14443A_HLTA_B2       0x00    ///< HALT command byte 2
#define ISO14443A_CT            0x88    ///< Cascade Tag (indicates more UID bytes follow)

// Anticollision/Select commands
#define ISO14443A_SEL_CL1       0x93    ///< Select cascade level 1 (UID bytes 0-3)
#define ISO14443A_SEL_CL2       0x95    ///< Select cascade level 2 (UID bytes 3-6)
#define ISO14443A_SEL_CL3       0x97    ///< Select cascade level 3 (UID bytes 6-9, rare)
#define ISO14443A_NVB_ANTICOLL  0x20    ///< NVB for anticollision (0 UID bits known)
#define ISO14443A_NVB_SELECT    0x70    ///< NVB for select (all 40 UID bits known)

// ============================================================================
// CR95HF SendRecv Flags
// Reference: CR95HF Datasheet Section 5.6
// ============================================================================

#define CR95HF_TXFLAG_7BIT      0x07    ///< Transmit 7-bit short frame (for REQA/WUPA)
#define CR95HF_TXFLAG_PARITY    0x08    ///< Append odd parity bit (ISO14443A)
#define CR95HF_TXFLAG_CRC       0x20    ///< Append CRC-A
#define CR95HF_TXFLAG_TOPAZ     0x80    ///< Topaz format (NFC-T1T)

/// Combined flags: 7-bit short frame, no CRC (for REQA/WUPA)
#define CR95HF_FLAG_SHORTFRAME  0x07
/// Combined flags: parity only, no CRC (for anticollision)
#define CR95HF_FLAG_STD         0x08
/// Combined flags: parity + CRC (for select, read, write commands)
#define CR95HF_FLAG_STD_CRC     0x28

// ============================================================================
// SAK (Select Acknowledge) Card Type Values
// Reference: NXP AN10833 - MIFARE Type Identification Procedure
// ============================================================================

#define SAK_MIFARE_UL           0x00    ///< MIFARE Ultralight / NTAG21x
#define SAK_MIFARE_1K           0x08    ///< MIFARE Classic 1K
#define SAK_MIFARE_MINI         0x09    ///< MIFARE Mini (320 bytes)
#define SAK_MIFARE_4K           0x18    ///< MIFARE Classic 4K
#define SAK_MIFARE_PLUS_2K      0x10    ///< MIFARE Plus 2K (Security Level 1)
#define SAK_MIFARE_PLUS_4K      0x11    ///< MIFARE Plus 4K (Security Level 1)
#define SAK_MIFARE_PLUS         0x20    ///< MIFARE Plus (SL2/SL3) / DESFire
#define SAK_JCOP                0x28    ///< JCOP / SmartMX
#define SAK_MIFARE_4K_EMU       0x38    ///< MIFARE Classic 4K (emulated)
#define SAK_MIFARE_1K_INF       0x88    ///< MIFARE Classic 1K (Infineon)
#define SAK_MIFARE_PRO          0x98    ///< MIFARE ProX

// ============================================================================
// CR95HF_Frame - Frame Builder/Decoder Class
// ============================================================================

/**
 * @class   CR95HF_Frame
 * @brief   Frame builder for CR95HF UART protocol commands
 *
 * Helper class to construct CR95HF command frames. Each command consists of:
 * - Command code (1 byte)
 * - Length (1 byte)
 * - Payload (variable)
 *
 * @code
 * CR95HF_Frame frame;
 * frame.buildREQA();
 * serial.write(frame.data, frame.len);
 * @endcode
 */
class CR95HF_Frame {
public:
    uint8_t data[32];   ///< Frame buffer (max 32 bytes)
    uint8_t len;        ///< Current frame length

    /**
     * @brief Default constructor
     */
    CR95HF_Frame() : len(0) {}

    /**
     * @brief Clear frame buffer and reset length
     */
    void clear() { len = 0; }

    /**
     * @brief Add a byte to the frame
     * @param b Byte to add
     */
    void add(uint8_t b) { if (len < sizeof(data)) data[len++] = b; }

    /**
     * @brief Build IDN (identification) command
     * @note Response contains device name string (e.g., "NFC FS2JAST4")
     */
    void buildIDN() {
        clear();
        add(CR95HF_CMD_IDN);
        add(0x00);  // No payload
    }

    /**
     * @brief Build Protocol Select command
     * @param proto Protocol code (CR95HF_PROTO_*)
     * @param param Protocol parameter (usually 0x00)
     */
    void buildProtocolSelect(uint8_t proto, uint8_t param = 0x00) {
        clear();
        add(CR95HF_CMD_PROTOCOL);
        add(0x02);  // Payload length
        add(proto);
        add(param);
    }

    /**
     * @brief Build SendRecv command with custom RF data
     * @param rfData RF data to transmit
     * @param rfLen Length of RF data
     * @param flags Transmit flags (CR95HF_FLAG_*)
     */
    void buildSendRecv(const uint8_t* rfData, uint8_t rfLen, uint8_t flags) {
        clear();
        add(CR95HF_CMD_SENDRECV);
        add(rfLen + 1);  // Payload = RF data + flags byte
        for (uint8_t i = 0; i < rfLen; i++) {
            add(rfData[i]);
        }
        add(flags);
    }

    /**
     * @brief Build REQA (Request Type A) command
     * @note Wakes only idle tags (not halted ones)
     */
    void buildREQA() {
        clear();
        add(CR95HF_CMD_SENDRECV);
        add(0x02);
        add(ISO14443A_REQA);
        add(CR95HF_FLAG_SHORTFRAME);
    }

    /**
     * @brief Build WUPA (Wake-Up Type A) command
     * @note Wakes all tags including previously halted ones
     */
    void buildWUPA() {
        clear();
        add(CR95HF_CMD_SENDRECV);
        add(0x02);
        add(ISO14443A_WUPA);
        add(CR95HF_FLAG_SHORTFRAME);
    }

    /**
     * @brief Build Anticollision command for Cascade Level 1
     * @note Returns 4 UID bytes + BCC
     */
    void buildAnticollCL1() {
        clear();
        add(CR95HF_CMD_SENDRECV);
        add(0x03);
        add(ISO14443A_SEL_CL1);
        add(ISO14443A_NVB_ANTICOLL);
        add(CR95HF_FLAG_STD);
    }

    /**
     * @brief Build Anticollision command for Cascade Level 2
     * @note Used for 7-byte and 10-byte UIDs
     */
    void buildAnticollCL2() {
        clear();
        add(CR95HF_CMD_SENDRECV);
        add(0x03);
        add(ISO14443A_SEL_CL2);
        add(ISO14443A_NVB_ANTICOLL);
        add(CR95HF_FLAG_STD);
    }

    /**
     * @brief Build Select command for Cascade Level 1
     * @param uid4bcc Pointer to 5 bytes: UID[0-3] + BCC
     */
    void buildSelectCL1(const uint8_t* uid4bcc) {
        clear();
        add(CR95HF_CMD_SENDRECV);
        add(0x08);
        add(ISO14443A_SEL_CL1);
        add(ISO14443A_NVB_SELECT);
        for (uint8_t i = 0; i < 5; i++) {
            add(uid4bcc[i]);
        }
        add(CR95HF_FLAG_STD_CRC);
    }

    /**
     * @brief Build Select command for Cascade Level 2
     * @param uid4bcc Pointer to 5 bytes: UID[3-6] + BCC
     */
    void buildSelectCL2(const uint8_t* uid4bcc) {
        clear();
        add(CR95HF_CMD_SENDRECV);
        add(0x08);
        add(ISO14443A_SEL_CL2);
        add(ISO14443A_NVB_SELECT);
        for (uint8_t i = 0; i < 5; i++) {
            add(uid4bcc[i]);
        }
        add(CR95HF_FLAG_STD_CRC);
    }
};

// ============================================================================
// CR95HF - Main Driver Class
// ============================================================================

/**
 * @class   CR95HF
 * @brief   Driver for CR95HF NFC/RFID transceiver
 *
 * Provides high-level interface for reading ISO14443-A tags using the
 * CR95HF transceiver over UART.
 *
 * @note    UART must be configured as 8N2 (2 stop bits)
 *
 * @code
 * CR95HF nfc(Serial1, RX_PIN, TX_PIN, 57600);
 *
 * if (nfc.begin()) {
 *     uint8_t uid[10];
 *     uint8_t uidLen, sak;
 *     if (nfc.iso14443aGetUID(uid, uidLen, sak)) {
 *         // Tag detected
 *     }
 * }
 * @endcode
 */
class CR95HF {
public:
    /**
     * @brief Constructor
     * @param port HardwareSerial reference (e.g., Serial1)
     * @param rxPin RX pin number (receives from CR95HF TXD)
     * @param txPin TX pin number (transmits to CR95HF RXD)
     * @param baudRate Baud rate (must be 57600 for CR95HF)
     */
    CR95HF(HardwareSerial& port, int rxPin, int txPin, uint32_t baudRate);

    /**
     * @brief Initialize the CR95HF
     * @param debug Enable debug output to Serial (default: false)
     * @return true if initialization successful, false otherwise
     *
     * Performs:
     * 1. UART initialization (8N2)
     * 2. Echo test
     * 3. IDN query
     * 4. ISO14443-A protocol selection
     */
    bool begin(bool debug = false);

    /**
     * @brief Read tag UID using ISO14443-A anticollision
     * @param uid Output buffer for UID (min 10 bytes)
     * @param uidLen Output: UID length (4, 7, or 10 bytes)
     * @param sak Output: SAK byte (card type indicator)
     * @return true if tag detected and UID read, false otherwise
     *
     * Automatically handles:
     * - WUPA/REQA for tag detection
     * - Multi-level anticollision for 7-byte UIDs
     * - Cascade tag (0x88) detection
     */
    bool iso14443aGetUID(uint8_t* uid, uint8_t& uidLen, uint8_t& sak);

    /**
     * @brief Read tag UID (without SAK)
     * @param uid Output buffer for UID (min 10 bytes)
     * @param uidLen Output: UID length (4, 7, or 10 bytes)
     * @return true if tag detected and UID read, false otherwise
     */
    bool iso14443aGetUID(uint8_t* uid, uint8_t& uidLen);

    /**
     * @brief Get human-readable card type from SAK byte
     * @param sak SAK byte value
     * @return Card type string (e.g., "MIFARE Classic 1K")
     */
    const char* getCardType(uint8_t sak);

    /**
     * @brief Run self-test and print results to Serial
     */
    void selfTest();

    /**
     * @brief Read device identification string
     * @param out Output buffer for IDN string
     * @param maxLen Maximum buffer length
     * @return true if successful
     */
    bool readIDN(char* out, uint8_t maxLen);

    /**
     * @brief Measure RF field level
     * @param level Output: field level (0-100)
     * @return true if measurement successful
     */
    bool measureFieldLevel(uint8_t& level);

    /**
     * @brief Check if antenna/RF field is operational
     * @return true if antenna OK
     */
    bool antennaOK();

    uint8_t lastATQA[2];    ///< Last received ATQA (for debugging)
    char deviceName[20];    ///< Device identification string

private:
    HardwareSerial* _port;  ///< Serial port reference
    int _rxPin;             ///< RX pin number
    int _txPin;             ///< TX pin number
    uint32_t _baud;         ///< Baud rate
    bool _debug;            ///< Debug output enabled

    CR95HF_Frame _txFrame;  ///< Reusable frame buffer

    // Debug helpers
    void log(const char* msg);
    void logHex(const char* prefix, const uint8_t* data, uint8_t len);

    // Low-level communication
    void flushRx();
    void sendFrame(const CR95HF_Frame& frame);
    bool readResponse(uint8_t& code, uint8_t* buf, uint8_t& len, uint32_t timeoutMs);

    // Protocol operations
    bool echoTest();
    bool protocolSelectA();
    bool sendReqWup(uint8_t cmd, uint8_t& atqa1, uint8_t& atqa2);
    bool anticollCL1(uint8_t* cl1);
    bool selectCL1(const uint8_t* cl1, uint8_t& sak);
    bool anticollCL2(uint8_t* cl2);
    bool selectCL2(const uint8_t* cl2, uint8_t& sak);
};
