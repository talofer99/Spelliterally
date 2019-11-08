#define HISTORYBUFFERSIZE 15
byte historyBuffer[HISTORYBUFFERSIZE];
byte historyBufferIDX;

// Use arduinojson.org/assistant to compute the capacity.
StaticJsonDocument<4024> doc;
JsonObject obj;

byte spellWordListLength;
byte currentSelectedWordIndex = 0;
String spellListJsonPath = "/settings/spell.json";


void btox(char *xp, byte *bb, int n)
{
  const char xx[] = "0123456789ABCDEF";
  while (--n >= 0) xp[n] = xx[(bb[n >> 1] >> ((1 - (n & 1)) << 2)) & 0xF];
}


//==============================================================
// save Card Letter Value
//==============================================================
void saveCardLetterValue(byte * currentCardId, char letter) {
  //convert  id to 8 carecters hex value
  int n = sizeof currentCardId << 1;
  char hexstr[n + 1];
  btox(hexstr, currentCardId, n);
  hexstr[n] = 0; /* Terminate! */

  // make sure fileexists
  if (!SPIFFS.exists("/settings/cards.txt")) {
    Serial.println("NO CARDS FILE");
    return;
  } //end if


  // if its new - we append to end
  if (card_state == 2) {
    File letterFile = SPIFFS.open("/settings/cards.txt", "a");
    letterFile.print(hexstr);
    letterFile.print(F(","));
    letterFile.print(letter);
    letterFile.print(F("\n"));
    letterFile.close();
  }
  //if not new we need to seek
  else
  {
    // lets calculate the position
    int offset = currentCardIdx * 11 + 9; // each line is 11 and to get to the letter we need 9 more steps
    File letterFile = SPIFFS.open("/settings/cards.txt", "r+");
    letterFile.seek(offset, SeekSet);
    letterFile.write(letter);
    letterFile.close();
  } //end if
}

//==============================================================
// set up letters
//==============================================================
void setUpLetterList() {
  //reset the length
  lettersArrayActualSize = 0;
  // try to open the cards list file
  File cardsFile = SPIFFS.open("/settings/cards.txt", "r");
  if (!cardsFile) {
    Serial.println("Failed to open the cards list file!");
    return;
  } //end if
  while (cardsFile.available()) {
    for (byte pos = 0; pos < 4; pos++) {
      char hexVal[2];
      cardsFile.readBytes(hexVal, 2);
      byte num = (byte)strtol(hexVal, NULL, 16);
      letters_id[lettersArrayActualSize][pos] = num;
      Serial.println(letters_id[lettersArrayActualSize][pos], HEX);
    } //end for
    // to make sure the integrity for the file is correct we are looking for a comma now
    if ((char)cardsFile.read() != ',') {
      Serial.println(F("Missing comma in letter file"));
      return;
    } //end if
    // read the letter
    letters[lettersArrayActualSize] = (char)cardsFile.read();
    // to make sure the integrity for the file is correct we are looking for a \n now
    if ((char)cardsFile.read() != '\n') {
      Serial.println(F("Missing \\n in letter file"));
      return;
    } //end if
    // move to next letter in the array
    lettersArrayActualSize++;
  } //end while
  cardsFile.close();
}


//==============================================================
// LIST OF WORDS STORED ON spell.json
//==============================================================
void setUpWordList() {
  // reset history
  historyBufferIDX = 0;

  // reset the length of the list
  spellWordListLength = 0;


  // Open file for reading
  File file = SPIFFS.open(spellListJsonPath, "r");

  // Parse the root object
  auto error = deserializeJson(doc, file);
  if (error) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(error.c_str());
    return;
  }
  // close file
  file.close();
  // create the json obj
  obj = doc.as<JsonObject>();

  for (int i = 0; i < obj["words"].size(); i++) {
    //Serial.println(obj["words"][i].as<String>());
    Serial.println(obj["words"][i]["w"].as<String>());
    Serial.println(obj["words"][i]["p"].as<String>());
  }

  spellWordListLength = obj["words"].size();


} //end setUpWordList




//==============================================================
//  RETURN IMAGE PATH FROM FILE
//==============================================================
String returnImagePath() {
  return obj["words"][currentSelectedWordIndex]["p"].as<String>();// imagePath;
}

//==============================================================
// is word in history
//==============================================================

boolean wordInHistory(byte wordIDX) {
  boolean returnValue = false;
  for (byte i = 0; i < HISTORYBUFFERSIZE; i++) {
    if (historyBuffer[i] == wordIDX) {
      returnValue = true;
      break;
    } //end if
  } //end for
  return returnValue;
}


//==============================================================
// add word to history
//==============================================================

void addWordToHistory(byte wordIDX) {
  historyBuffer[historyBufferIDX] = wordIDX;
  historyBufferIDX += 1;
  if (historyBufferIDX == HISTORYBUFFERSIZE) {
    historyBufferIDX = 0;
  }
}

//==============================================================
//   SELECT NEW WORD
//==============================================================
void selectNewWord() {
  byte newRandomWordIndex = random(0, spellWordListLength);
  // ***********************************
  while (wordInHistory(newRandomWordIndex)) {
    newRandomWordIndex = random(0, spellWordListLength);
  } // while they are the same
  // set new one
  currentSelectedWordIndex = newRandomWordIndex;
  setNewQuestion(obj["words"][currentSelectedWordIndex]["w"].as<String>()); //set new word //spellWordList[currentSelectedWordIndex]
  addWordToHistory(currentSelectedWordIndex);
}


//==============================================================
//   GET SPELL JSON LIST
//==============================================================
String getSpellJsonList() {
  Dir dir = SPIFFS.openDir("/settings/");

  String output = "[";
  while (dir.next()) {
    File entry = dir.openFile("r");


    Serial.println(String(entry.name()).substring(String(entry.name()).length() - 4));
    if (String(entry.name()).substring(String(entry.name()).length() - 4) == "json") {
      if (output != "[") {
        output += ',';
      } //end if 
      output += "\"";
      output += String(entry.name());
      output += "\"";
    } //end if
    entry.close();
  } //end while 

  output += "]";

  output = "{\"current\":\"" + spellListJsonPath + "\"," + "\"list\":" + output + "}";
  return output;
}
