# CR95HF Arduino UART Library

Arduino library for the STMicroelectronics CR95HF 13.56 MHz NFC/RFID transceiver.

## Features

- ISO14443-A (NFC-A) protocol support
- 4-byte and 7-byte UID detection
- Automatic anticollision handling
- SAK-based card type identification
- Built-in self-test and diagnostics
- Debug output option

## Hardware

Tested on:

* ESP32C6


## Supported Cards

- MIFARE Classic 1K/4K
- MIFARE Mini
- MIFARE Ultralight
- NTAG21x series (213, 215, 216)
- MIFARE Plus
- MIFARE DESFire
- Other ISO14443-A compatible tags

## Hardware Setup

The CR95HF must be configured for UART mode:

| CR95HF Pin | Connection |
|------------|------------|
| SSI_0 | GND |
| SSI_1 | GND |
| TXD | MCU RX |
| RXD | MCU TX |
| VCC | 3.3V |
| GND | GND |

**UART Configuration:**
- Baud rate: 57600
- Data bits: 8
- Stop bits: **2** (critical!)
- Parity: None

> **Warning:** The CR95HF requires 2 stop bits (8N2). Using 8N1 will cause communication failures.

## Installation

### Arduino IDE Library Manager

1. Open Arduino IDE
2. Go to Sketch → Include Library → Manage Libraries
3. Search for "CR95HF"
4. Click Install

### Manual Installation

1. Download this repository as ZIP
2. In Arduino IDE: Sketch → Include Library → Add .ZIP Library
3. Select the downloaded ZIP file

## Quick Start

```cpp
#include <CR95HF.h>

// ESP32-C6 example pins
#define NFC_RX_PIN  1
#define NFC_TX_PIN  2
#define NFC_BAUD    57600

CR95HF nfc(Serial1, NFC_RX_PIN, NFC_TX_PIN, NFC_BAUD);

void setup() {
    Serial.begin(115200);

    if (!nfc.begin()) {
        Serial.println("CR95HF init failed!");
        while (1);
    }

    Serial.println("CR95HF ready");
}

void loop() {
    uint8_t uid[10];
    uint8_t uidLen, sak;

    if (nfc.iso14443aGetUID(uid, uidLen, sak)) {
        // Print UID
        Serial.print("Tag: ");
        for (int i = 0; i < uidLen; i++) {
            if (uid[i] < 0x10) Serial.print('0');
            Serial.print(uid[i], HEX);
            if (i < uidLen - 1) Serial.print(':');
        }

        // Print card type
        Serial.print(" (");
        Serial.print(nfc.getCardType(sak));
        Serial.println(")");
    }

    delay(150);
}
```

## API Reference

### Constructor

```cpp
CR95HF(HardwareSerial& port, int rxPin, int txPin, uint32_t baudRate);
```

### Methods

| Method | Description |
|--------|-------------|
| `begin(bool debug = false)` | Initialize CR95HF. Returns true on success. |
| `iso14443aGetUID(uid, uidLen, sak)` | Read tag UID and SAK byte. |
| `iso14443aGetUID(uid, uidLen)` | Read tag UID (without SAK). |
| `getCardType(sak)` | Get card type string from SAK byte. |
| `selfTest()` | Run self-test, print results to Serial. |
| `readIDN(out, maxLen)` | Read device identification string. |
| `measureFieldLevel(level)` | Measure RF field strength (0-100). |
| `antennaOK()` | Check if antenna is operational. |

### SAK Card Types

| SAK | Card Type |
|-----|-----------|
| 0x00 | MIFARE Ultralight / NTAG |
| 0x08 | MIFARE Classic 1K |
| 0x09 | MIFARE Mini |
| 0x18 | MIFARE Classic 4K |
| 0x20 | MIFARE Plus / DESFire |

### Public Members

| Member | Type | Description |
|--------|------|-------------|
| `lastATQA[2]` | uint8_t[] | Last received ATQA (for debugging) |
| `deviceName[20]` | char[] | CR95HF identification string |

## Examples

- **TagReader** - Basic tag reading example

## Troubleshooting

### "CR95HF init failed"

1. Check wiring connections
2. Verify SSI_0 and SSI_1 are tied to GND
3. Ensure correct RX/TX pin assignment (TXD→RX, RXD→TX)
4. Verify 3.3V power supply

### No tags detected

1. Run `nfc.selfTest()` to check RF field
2. Verify antenna connection
3. Try different tag types
4. Check for interference (metal surfaces)

### Intermittent detection

1. Increase retry count in your code
2. Adjust scan interval
3. Check antenna coupling distance

## Library dependencies

* None.

## References

- [CR95HF Datasheet](https://www.st.com/resource/en/datasheet/cr95hf.pdf)
- [ISO14443-3A Standard](https://www.iso.org/standard/73596.html)
- [NXP MIFARE Type Identification (AN10833)](https://www.nxp.com/docs/en/application-note/AN10833.pdf)

## License

Copyright (c) 2025 B4E SRL. All rights reserved.

## Author

David Baldwin - B4E SRL
