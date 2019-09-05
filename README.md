# eve8266
What's it do?

Set's the color of a string of LED lights according to what system security your character is in.

Why?

Because I'd thought it would be fun.  Also I wanted to see if a ESP8266 had enough grunt to talk to the ESI API.

Yeah but why though?

Admittedly it's not for everyone.  It's more fun for explorers, who'll jump around various security systems, rather than a station trader.  It's super cool to be in a HS system, all green and nice, then have it slowly fade to red as you jump through into a wormhole.

How does it work?

Short version: A cheap microcontroller with WiFi asks the EVE servers where your character is, then changes the color of the lights accordingly.

Long version:  The ESP8266 connects to a wifi network, authorizes with the ESI API using httpclient and queries where the characters location is.  It then fetches the system security via a JSON request and decodes it to a color, based on the color of the waypoint markers.  It then feeds a string of WS2811 LEDs via a 3.3v -> 5v level shifter using the NeoPixelBus library.  It repeats every half second so it's always updated by the time your character is out of warp.

Does it do anything else?

Not really, I wanted to do some more complicated lighting effects like a larson scanner, but with the ESP8266 there's only one core, so it would mean the update interval would take longer.  I could probably do some effects with an ESP32 as it has two cores, but it seemed like overkill.  Also I was thinking about querying for kills in system, but there's not a whole lot of memory and it would require parsing universe-wide killmail logs from the ESI, as it doesn't provide them on a per system basis.  Could maybe knock up something using the zkillboard.com API though.

Can I buy one?

Just make one, it's not that hard.  The components cost less than $40 if you shop around, wiring it up will take about 20 minutes if you know what you are doing.  You might have to solder a few wires but being an EVE nerd you'll probably know someone who can do that for you.  The only fiddly part is setting up the ESI authorisation, which requires making a developer application with CCP.

Ingredients

ESP8266

A 3.3v -> 5v level shifter, a lot of WS2811 strings won't work properly at 3.3v.

Some lights

A power supply, rated to however many lights you want to run.
