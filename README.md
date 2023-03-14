# Video #262: ESP32 + WiFi + I2C + NeoPixel + nRF24L01 = DISASTER
How many things can go wrong with a project? Too many!

Long story short: I modified my ESP32 NeoPixel project (yes, the three circular LED rings that tell me if I have left the storage container bin lids open) to connect to Wi-Fi to get the time. If it is overnight, blank the LED display to "save power". Simples.

ESP32 + Neopixels works great, especially if you use a task per Neopixel strip. Oh, and you'd be wise to use a level-shifter with the NeoPixels because whilst they *may* work at 3v3 they are not guaranteed to do so. 5v is the preferred voltage which the ESP32 will most certainly not like. So a 10c (UK: 12p) 4-channel level shifter is being used.

So what's the FIRST problem?

NeoPixel task + Wi-Fi = ESP32 crash! Always happens on the neopixel "show()" command, but the message may vary as the ESP32 panics and reboots. Sometimes it complains about heap memory corruption, sometimes it's a pointer corruption (I don't use pointers in this sketch)... the list is varied and long and never shows the correct reason for the PANIC on Core 0 (sometimes Core 1).

And the SECOND problem?

Using an I2c device on an ESP32 which doesn't use the "default" I2C pins of 21 & 22 is trivial: just start the ```Wire.begin(SDA, CLK);``` with the pins you want to use. Great, works just fine. Except that the nRF24l01, which was running just fine before all these mods now refuses to work, because it is using those "default" I2C pins of 21 & 22 as standard GPIO pins.

Did anything work?

Well, if the Wi-Fi was started *first*, retrieved the time from an NTP server and then shut down, all was fine.

Also, if the 
