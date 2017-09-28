#ifndef CARD_READER_522_H
#define CARD_READER_522_H

#include <Arduino.h>
#include <MFRC522.h>
#include <TokenCache.h>

// RST-PIN for RC522 - RFID - SPI - Modul GPIO5 
#define RST_PIN 16 
// SDA-PIN for RC522 - RFID - SPI - Modul GPIO4 
#define SS_PIN 2 
#define CARDREADER_CHECK_INTERVAL_MS 100
#define TOKEN_DEBOUNCE_TIME_MS 5000


class CardReader522
{
public:
    CardReader522();
    void init();
    bool check();
    String lastToken; // last token as string
    uint8_t lastLen; // last token length
    TOKEN lastUID; // lasttokenUID

private:
    MFRC522 mfrc522; 
    unsigned long cardreaderLastCheck; // last time we polled the card reader
    unsigned long lastTokenTime; // millis when last token was detected
    char tokenStr[14]; // token as hex string

    void updateTokenStr(const uint8_t *data, const uint32_t numBytes) {
        const char * hex = "0123456789abcdef";
        uint8_t b = 0;
        for (uint8_t i = 0; i < numBytes; i++) {
                tokenStr[b] = hex[(data[i]>>4)&0xF];
                b++;
                tokenStr[b] = hex[(data[i])&0xF];
                b++;
        }

        // null remaining bytes in string
        for (uint8_t i=numBytes; i < 7; i++) {
                tokenStr[b] = 0;
                b++;
                tokenStr[b] = 0;
                b++;
        }
    }
};

#endif