// Button

bool buttonState = false;
bool prevButtonState = false;

void ButtonPoll(void * parameter) { // Run FastLED commands here
  while (true) {
    buttonState = !digitalRead(PRG); // Button reads HIGH when released

    if (prevButtonState != buttonState) {
      prevButtonState = buttonState;

      if (buttonState) {
        if (!booted) { // Code to run before setup() is complete
          transmitter = false;
        } else {
          led_enabled = !led_enabled;
          if (led_enabled) {
            sendMessage("ON");
          } else {
            sendMessage("OFF");
            FastLED.clear();
            FastLED.show();
          }
        }
      }
    }
    
    delay(5); // For stability
  }
}

// OLED

/*bool txt(String input = "----", int custom_line = -1) { // Draw a string with automatic line arrangement
  int txty = line*lnspace+YOFFSET;

  if (txty+txtheight > SCRHEIGHT) { return false; }
  
  display.drawString(0, txty, input);
  display.display();
  
  if (custom_line == -1) {
    line++; // Increment line
  }
  
  return true;
}

void cldsp() { // Clear display and reset line
  display.clear();
  line = 0;
}*/
