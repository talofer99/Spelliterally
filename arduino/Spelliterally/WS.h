#include <WebSocketsServer.h>

WebSocketsServer webSocket = WebSocketsServer(81);



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
  jsonResponse.concat(spellWordList[currentSelectedWordIndex]);
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
  int totalSockets = webSocket.connectedClients();
  for (int i = 0; i < totalSockets; i++) {
    // create return json
    String jsonObjectString = cardJson(card_state, letter);
    // send message to client
    webSocket.sendTXT(i, jsonObjectString);
  } //end for
} //end  WS_SendStatsUpdate()




// ******************************************************************
// send state update to all sockets
// ******************************************************************
void WS_SendStatsUpdate(boolean addImagePat) {
  int totalSockets = webSocket.connectedClients();
  for (int i = 0; i < totalSockets; i++) {
    // create return json
    String jsonObjectString = stateJson(addImagePat);
    // send message to client
    webSocket.sendTXT(i, jsonObjectString);
  } //end for
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
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED: {
        //print basic info
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

        //check how many connect there are
        int i = webSocket.connectedClients();
        Serial.print("webSocket.connectedClients - ");
        Serial.println(i);

        // check the page we are coming from


        if (payload[1] == 'a') { // admin
          systemState = SSLettersAdmin;

        } else if (payload[1] == 'g') { //game
          // if we are in admin - restart game
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
        } //end if
      } //end case
      break;

    case WStype_TEXT:
      Serial.printf("[%u] get Text: %s\n", num, payload);
      // we ,ark functions with # on the start
      if (payload[0] == '#') {
        switch (payload[1]) {
          case 'n': // select new word
            startNewGameProccess();
            break;
          case 's': // save card value
            Serial.println((char) payload[2]);
            saveCardLetterValue(currentCardId,(char) payload[2]);
            setUpLetterList();
            answer[0] = (char) payload[2];
            card_state = 1;
            WS_SendSCardpdate(card_state,(char) payload[2]);
            break;
        } //end swith

      } //end if
      break;
  } //end switch

}

void WS_setup() {


  // start webSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

}

void WS_loop() {
  webSocket.loop();
}
