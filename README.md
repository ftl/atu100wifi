# atu100wifi

This is a little extension device for David Fainitski's [ATU-100](https://github.com/Dfinitski/N7DDC-ATU-100-mini-and-extended-boards) antenna tuner. It adds connectivity though wifi to use the tuner in a remote setup. The software publishes all measurement data using MQTT and accepts remote commands through another MQTT topic.

The device replaces the tuner's display. It communicates with the ATU-100 through it's I2C bus to grasp all the measurement data. An Arduino Nano acts as I2C slave, listening with the display's address, and interpreting the rendering commands coming from the ATU-100. The Nano sends the display content to an ESP8266 [WeMos D1 mini](https://www.wemos.cc/en/latest/d1/d1_mini.html), where the data is parsed, packed into a JSON object and published to a MQTT topic.

This project uses PlatformIO to build the software for the MCUs.

## Disclaimer
I develop this device for myself and just for fun in my free time. If you find it useful, I'm happy to hear about that. If you have trouble using it, you have all the source code to fix the problem yourself (although pull requests are welcome).

I am not liable for whatever happens to you by building or using this device. Do this on your own risk!

## License
This tool is published under the [MIT License](https://www.tldrlegal.com/l/mit).

Copyright [Florian Thienel](http://thecodingflow.com/)