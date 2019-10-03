String spellWordList[60];
byte spellWordListLength;
byte currentSelectedWordIndex = 0;

void btox(char *xp, byte *bb, int n)
{
  const char xx[] = "0123456789ABCDEF";
  while (--n >= 0) xp[n] = xx[(bb[n >> 1] >> ((1 - (n & 1)) << 2)) & 0xF];
}


// **********************************************************************************************************
// save Card Letter Value
// **********************************************************************************************************
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
    letterFile.print(",");
    letterFile.print(letter);
    letterFile.print("\n");
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

// **********************************************************************************************************
// set up letters
// **********************************************************************************************************
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
      Serial.println("Missing comma in letter file");
      return;
    } //end if
    // read the letter
    letters[lettersArrayActualSize] = (char)cardsFile.read();
    // to make sure the integrity for the file is correct we are looking for a \n now
    if ((char)cardsFile.read() != '\n') {
      Serial.println("Missing \\n in letter file");
      return;
    } //end if
    // move to next letter in the array
    lettersArrayActualSize++;
  } //end while
  cardsFile.close();
}


// ********************************************************************************************
// LIST OF WORDS ARE STORED ON THE SPIFF AS [WORD].txt and contain a valid full url to an image
// ********************************************************************************************
void setUpWordList() {
  // reset the length of the list
  spellWordListLength = 0;

  Dir dir = SPIFFS.openDir("/spell");

  while (dir.next()) {
    String wordSpell = dir.fileName();
    wordSpell.replace("/spell/", "");
    wordSpell.replace(".txt", "");
    Serial.println(wordSpell);
    spellWordList[spellWordListLength++] = wordSpell;

  }//end while
} //end setUpWordList
// ********************************************************************************************
// ********************************************************************************************

//==============================================================
//  RETURN IMAGE PATH FROM FILE
//==============================================================

String returnImagePath() {
  String imagePath = "";
  // now lets get the path
  File wordFile = SPIFFS.open("/spell/" + spellWordList[currentSelectedWordIndex] + ".txt", "r");
  while (wordFile.available()) {
    imagePath.concat((char)wordFile.read());
  } // end while
  wordFile.close();

  return imagePath;
}


//==============================================================
//   SELECT NEW WORD
//==============================================================
void selectNewWord() {
  byte newRandomWordIndex = currentSelectedWordIndex;
  // ***********************************
  while (newRandomWordIndex == currentSelectedWordIndex) {
    newRandomWordIndex = random(0, spellWordListLength);
  } // while they are the same
  // set new one
  currentSelectedWordIndex = newRandomWordIndex;
  setNewQuestion(spellWordList[currentSelectedWordIndex]); //set new word
}
