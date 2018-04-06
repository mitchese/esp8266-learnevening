autoscale: true


# [fit] ESP8266
Sean Mitchell
April 11 2018

![](img/esp-12.png)

---

# Agenda

* What is it
* Why ESP8266
* How can I use it
* Hello World
* WifiManager
* Distros

---

# WTF

What is it

* Fairly powerful microcontroller
* 3.3V with 5V capable inputs
* "An Arduino with WiFi"
* External flash storage ranging from 512K - 16MB (128Mb)
  * Depends on your module
* Full Datasheet: https://goo.gl/jckoSJ

![right, fit](img/esp12.png)

---

# Why ESP8266

* Good, Cheap, Easy. Choose ~~2~~ 3
* Huge & Vibrant Community
* Many modules
  * Wemos, NodeMCU, bare module, among others
* Integrated in some finished products (Sonoff,etc)

---

# How to use

* Programmers vs built-in
* Arduino "IDE" or Atom.io
* Wiring Diagram

---

# Modules

* Inputs
  * PIR (Motion)
  * DHT11/22 (Temp/Humidity)
  * HC-SR04 (Distance)
  * AS3935 (Lightning)
* Outputs
  * Relays (220V)
  * LEDs
  * Text displays (LCDs)
  * Servos

![right, fit](img/modules.jpg)

---


![autoplay](https://youtu.be/8ISbmQTbjDI?t=206)

---

# Getting our hands dirty


In your bag, you will find one Wemos D1 Mini. Plug it into your computer and let's make something work!

Follow the instructions for blink.

![left, fit](img/blink.png)

---

# Using WiFi

Follow the instructions for the Base code or skip to the rgb led code

This will work either on your Wemos D1 - or if you were to flash it to my MagicHome controller, it would work immediately

![left, fit](img/rgbled.png)

---

# Hooking to HomeAssistant

MQTT we can integrate our light into HomeAssistant

* Allow complex control and automation
* Act / React to all kinds of events
  * HomeAssistant connected to Kodi (XBMC); when TV on, then dim lights
  * Scrape times from Deutsche Bahn & show status of my next train
  * When doorbell rings, notify HomeAssistant; if I'm home, pause TV; otherwise push notification
  * Log temperature / humidity to Prometheus / Grafana

---

# HASS setup

HomeAssistant is running hass.io on the raspberry pi here; Connect your MQTT and we can play around with it

---

# [fit] ENDE

---

footer: © Unsigned Integer UG, 2017
slidenumbers: true

# Deckset Basics

### Everything you need to know to start making presentations

![inline, filtered](https://www.get-digital.dk/web/getdigital/gfx/products/__generated__resized/380x380/4626tasten_sticker_tux_farbe.jpg)

---

Paragraphs **Bold** _italic_ **_BoldItalic_** in the Markdown

* BulletList [Links Like this](http://www.decksetapp.com)

1. This
1. Is
1. OrderedList

Code:

```objectivec
 UIView *someView = [[UIView alloc] init];
 NSString *string = @"some string that is fairly long, with many words";
```

![right](https://kubernetes.io/images/favicon.png)


^speakernotes go here and can't be seen on the presentation

---

# [fit] BIG

^ This slide must be left blank

---
Video

![autoplay](https://www.youtube.com/watch?v=P0_QrKfD8us)


---
^ The : in the --- line shows how to align right/left/default/centre

First Name | Last Name | Born | Birthplace
---:|:---|---|:---:
Ross | **Anderson** | Sep 15, **1956** | :gb:
Whitfield | **Diffie** | Jun 4, **1944** | :us:
Clifford | **Cocks** | 28 Dec, **1950** | :gb:
Ralph | **Merkle** | Feb 2, **1952** | :us:


--
Sean Mitchell
-------------------

Brunnenallee 6
50226 Frechen

Mobile: +49 (0)176 456 357 69
E-mail: sean@srm.im
Homepage: https://srm.im
