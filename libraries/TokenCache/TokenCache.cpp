#include "TokenCache.h"
#include <EEPROM.h>

TokenCache::TokenCache(AccessSystem accessSystem) :
  accessSystem(accessSystem)
{ }

// Get cache item, or query server if not in cache
TOKEN_CACHE_ITEM* TokenCache::fetch(TOKEN* uid, uint8_t uidLength, String tokenStr) {

  TOKEN_CACHE_ITEM* item = NULL;
  
  // check cache
  item = get(uid, uidLength);
  
  if (item != NULL) {
    // check it's valid
    if (item->flags == 0) {
        remove(item);
        item = NULL;
    }
  }

  // not found, query server and add to cache if has permission
  if (item == NULL) {
    uint8_t flags = accessSystem.getAccess(tokenStr);

    if ((flags > 0) && (flags != TOKEN_ERROR)) {
        item = add(uid, uidLength, flags);
    }
  }

  return item;
}

// Get cache item by token, returns null if not in cache
TOKEN_CACHE_ITEM* TokenCache::get(TOKEN* token, uint8_t length) {
    // search through items to find a match to token
    // TODO: binary search?
  
    TOKEN_CACHE_ITEM* item = NULL;
  
    uint8_t i;
    for (i=0; i<cacheSize; i++) {
      if ((length == cache[i].length) && (memcmp(token, &cache[i], length)==0)) {
        item = &cache[i];
        break;
      }
    }
  
    return item;
  }
  
  
  // add a token to the cache, returns pointer to new cache item
  TOKEN_CACHE_ITEM* TokenCache::add(TOKEN* token, uint8_t length, uint8_t flags) {
    // if cache not full, then add a new item to end of array
    uint8_t pos = cacheSize;
    uint8_t i;
  
    // check for existing
    TOKEN_CACHE_ITEM* t = get(token, length);
    if (t != NULL) {
      // update flags
      t->flags = flags;
      return t;
    }
  
    // else, find item with least scans
    if (cacheSize == TOKEN_CACHE_SIZE) {
      cacheSize = 0;  // start at the beginning
  
      for (i=0; i < cacheSize; i++) {
        if (cache[i].count < cache[pos].count) {
          pos = i;
        }
      }
    } else {
      cacheSize++;
    }
  
    // write new token into the cache
    memcpy(&cache[pos].token, token, length);
    cache[pos].length = length;
    cache[pos].flags = flags;
    cache[pos].count = 1;
    cache[pos].sync = TOKEN_CACHE_SYNC;
  
    // write new info to EEPROM
    syncEEPROM();
  
    // print number of cache slots used
    Serial.print(F("Cache used: "));
    Serial.print(cacheSize);
    Serial.print('/');
    Serial.println(TOKEN_CACHE_SIZE);
  
    // return new item
    return &cache[pos];
  }
  
  // pass item to remove
  void TokenCache::remove(TOKEN_CACHE_ITEM* item) {
    item->length = 0;
    item->flags = 0;
    item->count = 0;
    item->sync = TOKEN_CACHE_SYNC;
  }
  
  void TokenCache::init() {
    Serial.println(F("Loading cache from EEPROM..."));
  
    EEPROM.begin(512);
  
    // read magic
    if (EEPROM.read(0) != EEPROM_MAGIC) {
      Serial.println(F("Magic changed, resetting cache"));
      // reset stored cache size
      EEPROM.write(0, EEPROM_MAGIC);
      EEPROM.write(1, 0);
  
      cacheSize = 0;
    } else {
      cacheSize = EEPROM.read(1);
      Serial.print(cacheSize);
      Serial.println(F(" items"));
    }
  
    // read token items from cache
    uint8_t i = 0, j = 0;
    uint16_t addr = 2;
    for (i=0; i<cacheSize; i++) {
      // token
      for (j=0; j<7; j++) {
        cache[i].token[j] = EEPROM.read(addr + j);
      }
  
      // length
      cache[i].length = EEPROM.read(addr + 7);
  
      // flags
      cache[i].flags = EEPROM.read(addr + 8);
  
      updateTokenStr(cache[i].token, cache[i].length);
  
      Serial.print(' ');
      Serial.print(tokenStr);
      Serial.print(':');
      Serial.println(cache[i].flags);
  
      cache[i].count = 0;
      cache[i].sync = 1 + i;  // resync everything soon-ish
  
      addr += 9;
    }
  }
  
  // task to update permission flags in cache, e.g. if someones access has changed
  // called every 10min ish
  void TokenCache::sync() {
    // for each item in cache
    uint8_t i;
    for (i=0; i<cacheSize; i++) {
      // dec sync counter
      cache[i].sync--;
  
      // if reached zero
      if (cache[i].sync == 0 && cache[i].length > 0) {
        updateTokenStr(cache[i].token, cache[i].length);
        Serial.print(F("Syncing cached flags for: "));
        Serial.println(tokenStr);
  
        // query permission flags from server
        uint8_t flags = accessSystem.getAccess(tokenStr);
  
        if (flags != TOKEN_ERROR) {
          if (flags > 0) {
           // if successful, update flags and reset sync counter
           cache[i].flags = flags;
           cache[i].sync = TOKEN_CACHE_SYNC;
          } else {
            // else remove token
            remove(&cache[i]);
          }
        } else {
          // else try again next cycle
          cache[i].sync = 1;
        }
  
      }
    }
  
    // sync changes to EEPROM
    syncEEPROM();
  }

// update EEPROM to match cache
void TokenCache::syncEEPROM() {
  boolean changed = false;

  changed |= updateEEPROM(1, cacheSize);

  // update items from cache
  uint8_t i = 0, j = 0;
  uint16_t addr = 2;
  for (i=0; i<cacheSize; i++) {
    // token
    for (j=0; j<7; j++) {
      changed |= updateEEPROM(addr + j, cache[i].token[j]);
    }

    // length
    changed |= updateEEPROM(addr + 7, cache[i].length);

    // flags
    changed |= updateEEPROM(addr + 8, cache[i].flags);

    addr += 9;
  }

  if (changed) {
    Serial.println(F("Updated EEPROM"));
  }
}