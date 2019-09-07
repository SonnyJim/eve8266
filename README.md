# eve8266
What's it do?

Sets the color of a string of LED lights according to what system security your character is in.

Why?

Because I'd thought it would be fun.  Also I wanted to see if a ESP8266 had enough grunt to talk to the EVE Online ESI API.

Yeah but why though?

Admittedly it's not for everyone.  It's more fun for explorers, who'll jump around various security systems, rather than a station trader.  It's super cool to be in a HS system, all green and nice, then have it slowly fade to red as you jump through into a wormhole.

How does it work?

Short version: A cheap microcontroller with WiFi asks the EVE servers where your character is, then changes the color of the lights accordingly.

Long version:  The ESP8266 connects to a wifi network, authorizes with the ESI API using httpclient and queries where the characters location is.  It then fetches the system security via a JSON request and decodes it to a color, based on the color of the waypoint markers.  It then feeds a string of WS2811 LEDs via a 3.3v -> 5v level shifter using the NeoPixelBus library.  It repeats every half second so it's always updated by the time your character is out of warp.

Does it do anything else?

Not really, I wanted to do some more complicated lighting effects like a larson scanner, but with the ESP8266 there's only one core, so it would mean the update interval would take longer.  I could probably do some effects with an ESP32 as it has two cores, but it seemed like overkill.  Also I was thinking about querying for kills in system, but there's not a whole lot of memory and it would require parsing universe-wide killmail logs from the ESI, as it doesn't provide them on a per system basis.  Could maybe knock up something using the zkillboard.com API though.  If you have any ideas, have a look through https://esi.evetech.net/ui/ and see if the data is available for what you want to do.

Can I buy one?

Just make one, it's not that hard.  The components cost less than $40 if you shop around, wiring it up will take about 20 minutes if you know what you are doing.  You might have to solder a few wires but being an EVE nerd you'll probably know someone who can do that for you.  The only fiddly part is setting up the ESI authorisation, which requires making a developer application with CCP.

Ingredients:

Nodemcu ESP8266 board

A 3.3v -> 5v level shifter, a lot of WS2811 strings won't work properly at 3.3v.

Some WS2811 lights

A power supply, rated to however many lights you want to run.

Manual:

Flash the software to the NodeMCU somehow.  Also connect to the NodeMCU using a terminal emulator as it makes diagnosing problems 3000% easier.

On first boot, it should automatically detect there's no settings and start an access point called 'eve8266'.  
Connect to this and then navigate to http://192.168.4.1 in a browser.  You will then be able to enter in the SSID and password of the WiFi network you want it to connect to.  Be aware than the ESP8266 only supports 2.4GHz wifi.

Connect to the NodeMCUs IP address and follow the instructions there, it should be quite simple to follow from this point.  It's important to note that the NodeMCU isn't really powerful enough to run a webserver at the same time as polling the ESI, so once it's been configured the webpage configuration is disabled.

Jumper settings:

Ground D5 and reset the NodeMCU to factory reset everything and start the configuration webserver
Ground D6 and reset to erase everything EXCEPT the wifi settings and start the configuration webserver.

Troubleshooting:

When it boots, it'll light the first three LEDs in the connected string to let you know how far along the boot process it is.
Also it gives you a chance to see if the LED RGB order is correct:

1st LED Red = Booting, 2nd LED Blue = Connected to WiFi, 3rd LED Green = Authorisation successful.

For further troubleshooting, plug the NodeMCU in via USB and use a terminal emulator set to 115200.
