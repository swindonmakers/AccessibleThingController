#ifndef TOKENCACHE_H
#define TOKENCACHE_H

#include <Arduino.h>
#include <AccessSystem.h>
#include <EEPROM.h>

#define TOKEN_CACHE_SIZE 32
#define TOKEN_CACHE_SYNC 144 // resync cache after <value> x 10 minutes
#define EEPROM_MAGIC 3       // update to clear EEPROM on restart

// tokens are 4 or 7-byte values, held in a fixed 7-byte array
typedef uint8_t TOKEN[7];

/*
  * struct for token cache
  * token - 7 bytes
  * length - unsigned byte - length of token
  * flags - 1 byte, encodes permission and trainer status
  * scan count - 2 bytes (unsigned int) - number of scans
  */
struct TOKEN_CACHE_ITEM {
    TOKEN token;    // the token uid
    uint8_t length; // length of token in bytes
    uint8_t flags;  // permission bits
    uint16_t count; // scan count
    uint8_t sync;   // countdown to resync with cache with server
};                  // in memory = 12 bytes, EEPROM size = 9 bytes

class TokenCache
{

  private:
    AccessSystem accessSystem;
    /*
    * cache is fixed sized array
    * if cache size exceeded then the item with the smallest scan count is removed
    */
    TOKEN_CACHE_ITEM cache[TOKEN_CACHE_SIZE];

    // number of items in cache
    uint8_t cacheSize = 0;

    unsigned long lastSyncTime = 0;

    // token as hex string
    char tokenStr[14];

    void updateTokenStr(const uint8_t *data, const uint32_t numBytes) {
        const char *hex = "0123456789abcdef";
        uint8_t b = 0;
        for (uint8_t i = 0; i < numBytes; i++) {
            tokenStr[b] = hex[(data[i] >> 4) & 0xF];
            b++;
            tokenStr[b] = hex[(data[i]) & 0xF];
            b++;
        }

        // null remaining bytes in string
        for (uint8_t i = numBytes; i < 7; i++) {
            tokenStr[b] = 0;
            b++;
            tokenStr[b] = 0;
            b++;
        }
    }

    // update a byte of EEPROM memory, return true if changed
    bool updateEEPROM(int address, uint8_t value) {
        boolean changed = EEPROM.read(address) != value;
        if (changed) {
            EEPROM.write(address, value);
        }
        return changed;
    }

    // update EEPROM to match cache
    void syncEEPROM();

  public:
    TokenCache(AccessSystem accessSystem);
    TOKEN_CACHE_ITEM *fetch(TOKEN *token, uint8_t length, String tokenStr);
    TOKEN_CACHE_ITEM *get(TOKEN *token, uint8_t length);
    TOKEN_CACHE_ITEM *add(TOKEN *token, uint8_t length, uint8_t flags);
    void remove(TOKEN_CACHE_ITEM *item);
    void init();
    void sync();
    void loop();
    void printHex(const uint8_t *data, const uint8_t numbytes);
};

#endif
