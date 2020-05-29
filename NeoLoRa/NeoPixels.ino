void LEDBlink(void * parameter) { // Run FastLED commands here
  while (true) {
    byte hue = millis() * .05;
    if (led_enabled) {
      for (int i = 0; i < LED_COUNT; i++) {
        leds[i] = CHSV((hue + i*30)%255, 255, 255);
      }
      FastLED.show();
    }
    delay(5);
  }
}
