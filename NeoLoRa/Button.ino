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
            sendMessage("LED_ON");
          } else {
            sendMessage("LED_OFF");
            FastLED.clear();
            FastLED.show();
          }
        }
      }
    }
    
    delay(5); // For stability
  }
}
