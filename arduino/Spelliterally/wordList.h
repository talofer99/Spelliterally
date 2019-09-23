String spellWordList[60];
byte spellWordListLength;
byte spellWordListSelectedIndex = 0;



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
