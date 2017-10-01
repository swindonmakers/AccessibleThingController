#include "CardReader522.h"

CardReader522::CardReader522() :
    mfrc522(SS_PIN, RST_PIN)
{ }

void CardReader522::init()
{
    SPI.begin();
    mfrc522.PCD_Init();
}

bool CardReader522::check()
{
  bool ret = false;

  // Token debounce
  if (lastToken != "" && millis() > lastTokenTime + TOKEN_DEBOUNCE_TIME_MS) {
    Serial.println(F("Clear last token"));
    lastToken = "";
  }

  // Check card reader
  if (millis() > cardreaderLastCheck + CARDREADER_CHECK_INTERVAL_MS && lastToken=="") {

    // Init the reader on every call to make sure its working correctly.
    // Checking version first doesn't seem a reliable way to test if its working
    // and the call to check card doesn't fail in any detecable way.
    mfrc522.PCD_Init();
    yield();

    if (mfrc522.PICC_IsNewCardPresent()) {
      Serial.print(F("Reader reports new card"));

      if (mfrc522.PICC_ReadCardSerial()) {
        lastTokenTime = millis();

        updateTokenStr(mfrc522.uid.uidByte, mfrc522.uid.size);

        Serial.print(F(" -> with UID: "));
        Serial.println(String(tokenStr));

        if (lastToken != String(tokenStr)) {
          lastToken = String(tokenStr);
          lastLen = mfrc522.uid.size;
          memcpy(lastUID, mfrc522.uid.uidByte, mfrc522.uid.size);
          ret = true;
        }
      } else {
        Serial.println(F(" -> Failed to read card serial"));
      }
    } 
    
    cardreaderLastCheck = millis();
  }

  yield();
  return ret;
}