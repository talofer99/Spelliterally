
String cardJson(byte card_state, char letter) {

  // all json reply will have state
  String jsonResponse = "{\"state\":";
  jsonResponse.concat(String(systemState));

  // add card_state
  jsonResponse.concat(",\"card_state\":");
  jsonResponse.concat(String(card_state));

  // add letter
  jsonResponse.concat(",\"letter\":\"");
  if (letter == 0)
    jsonResponse.concat(" ");
  else
    jsonResponse.concat((char)letter) ;
  jsonResponse.concat("\"");

  // end json
  jsonResponse.concat("}");

  return jsonResponse;
}


String stateJson(boolean addImagePath) {

  // all json reply will have state
  String jsonResponse = "{\"state\":";
  jsonResponse.concat(String(systemState));

  // add Q
  jsonResponse.concat(",\"q\":\"");
  jsonResponse.concat(obj["words"][currentSelectedWordIndex]["w"].as<String>());//spellWordList[currentSelectedWordIndex]
  jsonResponse.concat("\"");
  // add A
  jsonResponse.concat(",\"a\":\"");
  for (int i = 0; i < NR_OF_READERS; i++) {
    if (answer[i] == 0)
      jsonResponse.concat(" ");
    else
      jsonResponse.concat((char)answer[i]) ;
  } //end if
  jsonResponse.concat("\"");

  if (addImagePath) {
    jsonResponse.concat(",\"path\":\"");
    jsonResponse.concat(returnImagePath());
    jsonResponse.concat("\"");
  }


  // end json
  jsonResponse.concat("}");

  return jsonResponse;
}


// ******************************************************************
// send state update to all sockets
// ******************************************************************
void WS_SendSCardpdate(byte card_state, char letter) {

  String jsonObjectString = cardJson(card_state, letter);
  webSocket.textAll(jsonObjectString);

} //end  WS_SendStatsUpdate()




// ******************************************************************
// send state update to all sockets
// ******************************************************************
void WS_SendStatsUpdate(boolean addImagePat) {
  String jsonObjectString = stateJson(addImagePat);
  webSocket.textAll(jsonObjectString);
} //end  WS_SendStatsUpdate()


// ******************************************************************
// Start new game process used more then once in the code
// ******************************************************************
void startNewGameProccess() {
  //select new word
  selectNewWord();
  // set system state
  systemState = SSWaitForAnswer;
  // send update with new image
  WS_SendStatsUpdate(true);
}

// ******************************************************************
// WEB SOCKER EVENTS
// ******************************************************************
void webSocketEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {

  switch (type) {
    case WS_EVT_DISCONNECT:
      Serial.printf("ws[%s][%u] disconnect\n", server->url(), client->id());

      break;
    case WS_EVT_CONNECT: {
        AwsFrameInfo * info = (AwsFrameInfo*)arg;
        Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
        client->ping();
      } //end case
      break;

    case WS_EVT_DATA:
      AwsFrameInfo * info = (AwsFrameInfo*)arg;
      String msg = "";
      if (info->final && info->index == 0 && info->len == len) {
        //the whole message is in a single frame and we got all of it's data
        Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);

        if (info->opcode == WS_TEXT) {
          for (size_t i = 0; i < info->len; i++) {
            msg += (char) data[i];
          }
        }
        Serial.printf("%s\n", msg.c_str());

        if (info->opcode == WS_TEXT) {
          // letters admin
          if (msg[0] == 'a') {
            systemState = SSLettersAdmin;
          }
          // game
          else if (msg[0] == 'g')
          {
            if (systemState == SSLettersAdmin) {
              startNewGameProccess();
            } else  {
              // if the game was never started
              if (systemState == SSWaitForPlayer) {
                startNewGameProccess();
              } else {
                // send update with new image
                WS_SendStatsUpdate(true);
              }
            } //end if
          }
          // commands (#)
          else if (msg[0] == '#')
          {

            switch (msg[1]) {
              case 'n': // select new word
                startNewGameProccess();
                break;
              case 's': // save card value
                Serial.println((char) msg[2]);
                saveCardLetterValue(currentCardId, (char) msg[2]);
                setUpLetterList();
                answer[0] = (char) msg[2];
                card_state = 1;
                WS_SendSCardpdate(card_state, (char) msg[2]);
                break;
            } //end swith
          } //end if
        } //end if
      } //end if 
      break;
  } //end switch

}

void WS_setup() {
  webSocket.onEvent(webSocketEvent);
}
