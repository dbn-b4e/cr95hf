/**
 * @file    TagReader.ino
 * @brief   CR95HF NFC Tag Reader Example
 * @author  B4E SRL - David Baldwin
 * @date    November 2025
 *
 * Basic example demonstrating how to read ISO14443-A NFC tags using
 * the CR95HF library. Displays tag UID, length, SAK byte, and card type.
 *
 * Hardware Setup (ESP32-C6):
 * - GPIO1 = RX (connect to CR95HF TXD)
 * - GPIO2 = TX (connect to CR95HF RXD)
 * - CR95HF SSI_0 and SSI_1 tied to GND (UART mode)
 * - 57600 baud, 8N2 (2 stop bits required!)
 *
 * Supported Cards:
 * - MIFARE Classic 1K/4K
 * - MIFARE Ultralight
 * - NTAG21x series
 * - Most ISO14443-A compatible tags
 *
 * @note Adjust pin definitions for your hardware
 */

#include <CR95HF.h>

// ============================================================================
// Configuration - Adjust for your hardware
// ============================================================================

#define NFC_RX_PIN  1       // RX pin (from CR95HF TXD)
#define NFC_TX_PIN  2       // TX pin (to CR95HF RXD)
#define NFC_BAUD    57600   // CR95HF baud rate (fixed)

// Create CR95HF instance on Serial1
CR95HF nfc(Serial1, NFC_RX_PIN, NFC_TX_PIN, NFC_BAUD);

// ============================================================================
// Setup
// ============================================================================

void setup() {
    // Initialize debug serial
    Serial.begin(115200);
    delay(1000);  // Wait for serial monitor

    Serial.println();
    Serial.println("=== CR95HF Tag Reader Example ===");
    Serial.println();

    // Initialize CR95HF (set true for debug output)
    Serial.print("Initializing CR95HF... ");
    if (!nfc.begin(false)) {
        Serial.println("FAILED!");
        Serial.println("Check wiring:");
        Serial.println("  - RX pin connected to CR95HF TXD");
        Serial.println("  - TX pin connected to CR95HF RXD");
        Serial.println("  - SSI_0 and SSI_1 tied to GND");
        while (1) delay(500);
    }

    Serial.println("OK");
    Serial.print("Device: ");
    Serial.println(nfc.deviceName);
    Serial.println();
    Serial.println("Place a tag on the antenna...");
    Serial.println();
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
    uint8_t uid[10];
    uint8_t uidLen = 0;
    uint8_t sak = 0;

    // Try up to 3 times for better detection reliability
    bool detected = false;
    for (int retry = 0; retry < 3 && !detected; retry++) {
        detected = nfc.iso14443aGetUID(uid, uidLen, sak);
        if (!detected) delay(5);
    }

    if (detected) {
        // Print UID
        Serial.print("UID (");
        Serial.print(uidLen);
        Serial.print(" bytes): ");
        for (int i = 0; i < uidLen; i++) {
            if (uid[i] < 0x10) Serial.print('0');
            Serial.print(uid[i], HEX);
            if (i < uidLen - 1) Serial.print(':');
        }

        // Print SAK and card type
        Serial.print("  SAK=0x");
        if (sak < 0x10) Serial.print('0');
        Serial.print(sak, HEX);
        Serial.print(" (");
        Serial.print(nfc.getCardType(sak));
        Serial.println(")");
    }

    // Scan interval (~6.7 Hz)
    delay(150);
}
