void flash(int num_flashes = 1, int flash_speed = 75) {
  for (int i = 0; i < num_flashes; i++) {
    digitalWrite(LED, HIGH);
    delay(flash_speed);
    digitalWrite(LED, LOW);
    delay(flash_speed);
  }
}

void sendMessage(String outgoing) {
  last_sent = outgoing;
  LoRa.beginPacket();                   // start packet
  LoRa.write(destination);              // add destination address
  LoRa.write(localAddress);             // add sender address
  LoRa.write(msgCount);                 // add message ID
  LoRa.write(outgoing.length());        // add payload length
  LoRa.print(outgoing);                 // add payload
  LoRa.endPacket();                     // finish packet and send it
  msgCount++;                           // increment message ID

  // Flash Onboard LED once for outgoing message
  flash();
}

void onReceive(int packetSize) {
  if (packetSize == 0) return;          // if there's no packet, return

  // read packet header bytes:
  int recipient = LoRa.read();          // recipient address
  byte sender = LoRa.read();            // sender address
  byte incomingMsgId = LoRa.read();     // incoming msg ID
  byte incomingLength = LoRa.read();    // incoming msg length

  String incoming = "";

  while (LoRa.available()) {
    incoming += (char)LoRa.read();
  }

  if (incomingLength != incoming.length()) {   // check length for error
    Serial.println("error: message length does not match length");
    last_received = "LEN_ERR";
    flash(3);
    return;                             // skip rest of function
  }

  // if the recipient isn't this device or broadcast,
  if (recipient != localAddress && recipient != 0xFF) {
    Serial.println("This message is not for me.");
    last_received = "WRONG_DEV";
    flash(3);
    return;                             // skip rest of function
  }

  last_received = incoming;

  if (incoming == "I") {
    led_enabled = true;
  } else if (incoming == "O") {
    led_enabled = false;
    FastLED.clear();
    FastLED.show();
  } else if (incoming.toInt()) { // Calculate time_offset for sync
    transmitter = false;
    remote_millis = incoming.toInt();
    time_offset = remote_millis - millis();
  }

  // Flash Onboard LED twice for incoming message
  flash(2);
}
