void LEDBlink(void * parameter) { // Run FastLED commands here
  while (true) {
    byte hue = millis() * .05;
    if (led_enabled) {
      FastLED.showColor(CHSV(hue, 255, 255));
    }
    delay(5);
  }
}
