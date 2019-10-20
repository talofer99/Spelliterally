#include <SPI.h>
#include <MFRC522.h>

#define NR_OF_READERS   6
#define RST_PIN         D0
#define SS_1_PIN        D8
#define SS_2_PIN        D4
#define SS_3_PIN        D3
#define SS_4_PIN        D2
#define SS_5_PIN        D1
#define SS_6_PIN        RX

#define SS_7_PIN        47


//
byte flagCounter[NR_OF_READERS];
byte ssPins[] = { SS_1_PIN, SS_2_PIN, SS_3_PIN, SS_4_PIN, SS_5_PIN, SS_6_PIN};

MFRC522 mfrc522[NR_OF_READERS];   // Create MFRC522 instance.


char answer[NR_OF_READERS];
char question[NR_OF_READERS];

// setup rfid pins (only)
void setup_rfid_pins() {
  // set all SS pins and output and pull up
  for (uint8_t i = 0; i < NR_OF_READERS; i++) {
    pinMode(ssPins[i], OUTPUT);
    digitalWrite(ssPins[i], HIGH);
  } //end for
} //end

// setup rfid
void setup_rfid() {
  SPI.begin();      // Init SPI bus
  // set all the resders
  for (uint8_t reader = 0; reader < NR_OF_READERS; reader++) {
    mfrc522[reader].PCD_Init(ssPins[reader], RST_PIN); // Init each MFRC522 card
    delay(100); //short dealy to allow init
    Serial.print(F("Reader "));
    Serial.print(reader);
    Serial.print(F(": "));
    mfrc522[reader].PCD_DumpVersionToSerial();
  } //end for
} //end setup_rfid()


void setNewQuestion(String newWord) {
  Serial.print(F("new word - "));
  Serial.println(newWord);
  
  for (byte i = 0; i < NR_OF_READERS; i++) {
    if (i < newWord.length())
      question[i] = newWord[i];
    else
      question[i] = 0;
  }
  Serial.println(newWord.length());
}


// check for matching letter
char  checkLetter(byte * buffer, byte bufferSize, byte rederID) {
  char returnValue = 0;
  byte match;
  for (byte letterCode = 0; letterCode < sizeof(letters); letterCode++) { //lettersArrayActualSize
    match = 0;
    for (byte i = 0; i < bufferSize; i++) {
      if (letters_id[letterCode][i] == buffer[i])
        match++;
      else
        break; // no need to keep the loop - it already fail
    } //end for

    // if we got a match
    if (match == bufferSize) {
      returnValue = letters[letterCode];
      currentCardIdx = letterCode;
      break;
    } //end if
  } //end for

  return returnValue;
} //end checkLetter



// checkAnswer
boolean is_rfid_cards_clear() {
  byte match = 0;
  boolean isCleared  = false;
  for (int i = 0; i < NR_OF_READERS; i++) {
    if (answer[i] == 0)
      match++;
    else
      break; // no need to keep the loop - it already fail
  } //end for

  // if all cleared
  if (match == NR_OF_READERS)
    isCleared = true;


  return isCleared;
} //end is_rfid_cards_clear


// checkAnswer
boolean is_rfid_solved() {
  byte match = 0;
  boolean solved = false;
  // loop over all letters
  for (int i = 0; i < NR_OF_READERS; i++) {
    if (answer[i] == question[i])
      match++;
    else
      break; // no need to keep the loop - it already fail
  } //end for

  // if all match
  if (match == NR_OF_READERS)
    solved = true;


  return solved;
} //end printSpell



boolean rfid_check_first_reader() {
  boolean isChanged = false;
  byte reader = 0;
  //Serial.println("READER - " + String(reader));
  if (mfrc522[reader].PICC_IsNewCardPresent()) {
    flagCounter[reader] = 0;
    if (mfrc522[reader].PICC_ReadCardSerial()) {
      char checkedLetter = checkLetter(mfrc522[reader].uid.uidByte, mfrc522[reader].uid.size, reader);
      if (memcmp ( currentCardId, mfrc522[reader].uid.uidByte, sizeof(currentCardId) ) != 0 ) {
        memcpy(currentCardId, mfrc522[reader].uid.uidByte, mfrc522[reader].uid.size * sizeof(byte));
        answer[reader] = checkedLetter;
        isChanged = true;
      }
    } //end if
  } else {
    flagCounter[reader]++;
  } //end if

  // hadneling the state
  if (flagCounter[reader] == 10) {
    answer[reader] = 0;
    memset(currentCardId, 0x00, sizeof(currentCardId)); //clear
    
    isChanged = true;
  } else if (flagCounter[reader] > 250) {
    flagCounter[reader] = 21;
  } //end if

  return isChanged;
}






// ********************************
// RFID loop
boolean rfid_change() {
  boolean isChanged = false;
  for (uint8_t reader = 0; reader < NR_OF_READERS; reader++) {
    //Serial.println("READER - " + String(reader));
    if (mfrc522[reader].PICC_IsNewCardPresent()) {
      flagCounter[reader] = 0;
      if (mfrc522[reader].PICC_ReadCardSerial()) {
        char checkedLetter = checkLetter(mfrc522[reader].uid.uidByte, mfrc522[reader].uid.size, reader);
        //Serial.println((char) answer[reader]);
        if (checkedLetter != answer[reader]) {
          answer[reader] = checkedLetter;
          isChanged = true;
        } //end if
      } //end if
    } else {
      flagCounter[reader]++;
    } //end if

    // hadneling the state
    if (flagCounter[reader] == 4) {
      answer[reader] = 0;
      isChanged = true;
    } else if (flagCounter[reader] > 250) {
      flagCounter[reader] = 21;
    } //end if
  } //for(uint8_t reader


  //return
  return isChanged;
} //end RFIDLoop()
