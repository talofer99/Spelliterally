#include <WebSocketsServer.h>

WebSocketsServer webSocket = WebSocketsServer(81);



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
// WEB SOCKER EVENTS
// ******************************************************************
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED: {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

        //check how many connect there are
        int i = webSocket.connectedClients();
        Serial.print("webSocket.connectedClients - ");
        Serial.println(i);
        // if the game was never started
        if (systemState == SSWaitForPlayer) {
          //select new word
          selectNewWord();
          // set system state
          systemState = SSWaitForAnswer;
        }

        // create return json
        String jsonObjectString = stateJson(true);
        // send message to client
        webSocket.sendTXT(num, jsonObjectString);
      }
      break;
    case WStype_TEXT:
      Serial.printf("[%u] get Text: %s\n", num, payload);
      // we ,ark functions with # on the start
      if (payload[0] == '#') {
        switch (payload[1]) {
          case 'n':
            //select new word
            selectNewWord();
            // set system state
            systemState = SSWaitForAnswer;
            // send update with new image 
            WS_SendStatsUpdate(true);
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
