# Video #262: ESP32 + WiFi + I2C + NeoPixel + nRF24L01 = DISASTER  
### How many things can go wrong with a project? Too many!  

![Thumbnail-00019 (Phone)](https://user-images.githubusercontent.com/20911308/225037315-99311e41-eca8-4c77-8011-d5f9dea47d36.png)  

### Video link: https://youtu.be/11yovWoOgjw  
<br>  

<sub>JLCPCB 1-8 Layer PCB at $2. PCBA from $0 (Free Setup, Free Stencil)</sub>  
[![JLCPCB Any colour including purple](https://user-images.githubusercontent.com/20911308/223475598-b2e00f51-f634-4802-a6c1-336b02c748d6.jpg "JLCPCB 1-8 Layer PCB at $2. PCBA from $0 (Free Setup, Free Stencil)
Sign Up Here to Get $54 New User Coupons")](https://jlcpcb.com/?from=SKL)  
<sup>Sign Up Here to Get $54 New User Coupons at -  https://jlcpcb.com/?from=SKL</sup>  

#### Long story short
I modified my ESP32 NeoPixel project (yes, the three circular LED rings that tell me if I have left the storage container bin lids open) to connect to Wi-Fi to get the time. If it is overnight, blank the LED display to "save power". Simples.

ESP32 + Neopixels works great, especially if you use a task per Neopixel strip. Oh, and you'd be wise to use a level-shifter with the NeoPixels because whilst they *may* work at 3v3 they are not guaranteed to do so. 5v is the preferred voltage which the ESP32 will most certainly not like. So a 10c (UK: 12p) 4-channel level shifter is being used.

#### So what's the FIRST problem?

NeoPixel task + Wi-Fi = ESP32 crash! Always happens on the neopixel "show()" command, but the message may vary as the ESP32 panics and reboots. Sometimes it complains about heap memory corruption, sometimes it's a pointer corruption (I don't use pointers in this sketch)... the list is varied and long and never shows the correct reason for the PANIC on Core 0 (sometimes Core 1).

#### And the SECOND problem?

Using an I2c device on an ESP32 which doesn't use the "default" I2C pins of 21 & 22 is trivial: just start the ```Wire.begin(SDA, CLK);``` with the pins you want to use. Great, works just fine. Except that the nRF24l01, which was running just fine before all these mods now refuses to work, because it is using those "default" I2C pins of 21 & 22 as standard GPIO pins.

#### Did anything work?

Well, if the Wi-Fi was started *first*, retrieved the time from an NTP server and then shut down, all was fine.

Also, if the I2C is put into a task then the nRF24L01 seems to behave itself again (or perhaps, Wire [aka I2C] does).

Watch the video to see more!

### Hardware
► Simple 4-way level shifter so 5v peripherals can be used with a 3v3 μController  
![Level Shifter 4-channel (Custom)](https://user-images.githubusercontent.com/20911308/225067366-badab862-4993-4471-8a10-581b50800e8c.png)  
https://s.click.aliexpress.com/e/_DD0d7wz  

► List of all my videos
(Special thanks to Michael Kurt Vogel for compiling this)  
http://bit.ly/YouTubeVideoList-RalphBacon

► If you like this video please give it a thumbs up, share it and if you're not already subscribed please consider doing so and joining me on my Arduinite (and other μControllers) journey

My channel, GitHub and blog are here:  
\------------------------------------------------------------------  
• https://www.youtube.com/RalphBacon  
• https://ralphbacon.blog  
• https://github.com/RalphBacon  
• https://buymeacoffee.com/ralphbacon  
\------------------------------------------------------------------

My ABOUT page with email address: https://www.youtube.com/c/RalphBacon/about
