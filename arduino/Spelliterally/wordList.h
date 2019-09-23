String spellWordList[60];
byte spellWordListLength;
byte currentSelectedWordIndex = 0;



// ********************************************************************************************
// LIST OF WORDS ARE STORED ON THE SPIFF AS [WORD].txt and contain a valid full url to an image
// ********************************************************************************************
void setUpWordList() {
  // reset the length of the list
  spellWordListLength = 0;
  
  Dir dir = SPIFFS.openDir("/spell");
  
  while (dir.next()) {
    String wordSpell = dir.fileName();
    wordSpell.replace("/spell/","");
    wordSpell.replace(".txt","");
    Serial.println(wordSpell);
    spellWordList[spellWordListLength++] = wordSpell;
    
  }//end while 
} //end setUpWordList  
// ********************************************************************************************
// ********************************************************************************************

//==============================================================
//  RETURN IMAGE DATA JSON
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
