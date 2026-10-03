# BLE Media Remote

A pocket-sized Bluetooth Low Energy remote that sends media transport commands (next, previous) directly to a phone, built to solve a specific annoyance: controlling music in a car that has no native wireless media support.

## Why this exists

My car is a 2007 Lexus ES350. It has Bluetooth calling but no Bluetooth audio. I run a Tunai Fly adapter into the aux and USB ports to get wireless audio from my phone, which solves playback but not track control. Changing songs meant either unlocking my phone while driving or asking Siri, which routes through speech recognition and regularly takes 8 to 10 seconds to respond. Neither is something I want to rely on at highway speed.

A dedicated hardware button removes the speech recognition step entirely. Press it, the phone gets a standard Bluetooth HID media command, the song changes. No unlocking the phone, no waiting on a voice assistant.

## What it does

- Two physical buttons send Previous Track and Next Track commands over Bluetooth Low Energy, using the same HID profile a pair of wireless earbuds uses for its media buttons.
- An RGB LED reports battery charge level at a glance (green above 60%, yellow 20 to 59%, red below 20%), and gives a short red blink periodically once the battery is low.
- A hardware slide switch fully disconnects the battery when off, so there's no standby drain to think about between drives.
- Runs on a single rechargeable LiPo cell, charged through the same USB-C port used for flashing firmware.

## Hardware

| Part | Role |
|---|---|
| Seeed XIAO nRF52840 | Main controller, handles BLE radio and firmware |
| 2x tactile push buttons (4-pin) | Previous / Next input |
| 1x RGB LED (4-pin, common cathode) | Battery status indicator |
| 3x 220 ohm resistors | Current limiting for LED legs |
| SPDT slide switch (3-pin) | Hardware power cutoff |
| LiPo battery, JST-PH connector | Power source |
| Female JST-PH pigtail | Removable battery connection |
| Small perfboard | Mounting substrate |

### Why the nRF52840 over other boards

Early on I compared this against the XIAO ESP32-C6. The C6 is five dollars cheaper and nominally more capable on paper (WiFi 6, Thread, Zigbee), but Bluetooth HID support on Espressif's chips is much rougher around the edges. You end up hand-rolling the HID report descriptor and GATT service yourself with less documentation to lean on. Nordic's nRF52840 has mature BLE HID libraries (Adafruit's Bluefruit library, which Seeed's board package is built on) with consumer control media keys supported as a single function call. For a project where the BLE HID stack is the entire point, picking the chip with the better BLE library mattered more than five dollars or the extra radios I wasn't going to use.

## Circuit design

### Power path

The battery's positive lead runs through the slide switch before reaching the board's BAT+ pad. This was a deliberate choice over putting the switch on a GPIO pin and handling power state in software. A switch wired inline with the battery gives a genuine zero-current state when off, no firmware involved, no risk of a sleep mode that doesn't actually sleep. The tradeoff is that the firmware never knows the switch position; it only runs when there's power to run on. For a simple on/off remote, that's the right tradeoff.

Wiring:
- Battery (+) to switch common pin
- Switch outer pin (NO) to the female JST connector's + pin, which in turn connects to the board's BAT+ pad
- Battery (-) to the board's BAT- pad directly, bypassing the switch entirely
- The switch's third pin is left unconnected

BAT+ and BAT- are solder pads on the underside of the XIAO board, not part of the top header. They're easy to miss if you're only looking at the pinout diagram, since they're unlabeled silkscreen-wise on most XIAO nRF52840 units.

### Signal wiring

| Function | Pin | Notes |
|---|---|---|
| Next button | D0 | Internal pull-up, active low |
| Previous button | D6 | Internal pull-up, active low |
| LED Red | D1 | Through 220 ohm resistor |
| LED Green | D3 | Through 220 ohm resistor |
| LED Blue | D5 | Through 220 ohm resistor |

Both buttons use the chip's internal pull-up resistors rather than external ones. One leg goes to the signal pin, the diagonal leg goes to ground, and the firmware reads the pin as LOW when pressed. This is the standard low-component-count way to wire a momentary switch, and it kept the parts list and the perfboard layout simpler.

### Battery monitoring

This went through two approaches. The first plan was an external voltage divider (two resistors) feeding an ADC pin, which is the standard way to read a LiPo's voltage safely, since a full cell can hit 4.2V and the chip's ADC tops out around 3.6V. Before building that, I found that the XIAO nRF52840 already has this divider built in, wired to pin P0.31, with P0.14 acting as an enable line for the read path. Using the board's existing circuit instead of duplicating it in a divider saved two resistors and two solder joints, and it's the kind of detail you only catch by reading the board's documentation closely rather than assuming a generic reference design applies.

The firmware converts the raw ADC reading to a voltage, then maps that voltage to an estimated percentage using a rough resting-voltage curve for LiPo cells (4.2V as 100%, 3.5V as roughly 3%, and so on). This is an approximation. LiPo voltage sags under load and varies by cell, so the percentage is a reasonable indicator rather than a precise fuel gauge. Good enough for "should I charge this before my next drive," which is the actual use case.

## Firmware

Built in the Arduino IDE against Seeed's nRF52 board package, which bundles the Adafruit Bluefruit library.

### BLE HID

The remote advertises as a standard BLE HID peripheral and uses the Bluefruit library's consumer control key functions to send Previous Track and Next Track commands. This is the same mechanism any Bluetooth headset's media buttons use, which is why it works across music apps without needing per-app integration. The library handles the HID report descriptor and GATT service setup; the application code only has to call `consumerKeyPress()` and `consumerKeyRelease()` for the right usage code. I didn't write a custom HID report descriptor for this version, since the library's built-in consumer control profile already covers next/previous/play-pause. A custom descriptor would only be worth the extra complexity if I needed usage codes the library doesn't expose.

### Button handling

Buttons are read by polling in the main loop, not by interrupt. Each button has its own debounce state tracked with a timestamp: a reading has to hold steady for 30ms before it's treated as a real press. This is simpler to reason about than an interrupt service routine and more than fast enough for a human pressing a button, since the loop runs orders of magnitude faster than any button bounce window. The honest tradeoff against an interrupt-driven design is that polling ties input latency to how long the rest of the loop takes to execute; with battery reads only happening every 10 seconds and LED timing handled with non-blocking millis() checks, the loop stays short enough that this isn't noticeable in practice, but it's worth being direct about the choice rather than calling it interrupt-driven when it isn't.

### LED behavior

The LED is a common cathode RGB part, driven directly by digitalWrite on each color's GPIO pin through a current-limiting resistor (no PWM color mixing in this version, just the three primaries and combinations of them). On every button press, the LED flashes briefly in the color matching current battery status. It does the same once at startup, so picking up the remote gives an immediate battery read without needing to press a button first. If the battery drops below 20%, the LED also gives an unprompted short red blink every five seconds, so a draining battery doesn't go unnoticed between presses.

### Battery service

The firmware also runs the standard BLE Battery Service alongside the HID service, so the paired phone can show the remote's battery percentage in its Bluetooth device list the same way it would for headphones, separate from the LED indicator.

## Build and debug notes

A few things that went wrong during assembly are worth recording, since they were more instructive than anything that went right on the first try.

**Ground wire on one button failed silently.** One of the three original buttons (play/pause, later removed when I simplified to two buttons) worked when tested by touching multimeter probes directly to its wires, but did nothing when physically pressed. The switch mechanism itself tested fine in isolation. The fault turned out to be a cold solder joint on that button's ground wire, invisible by eye but detectable by wiggling the wire while watching the multimeter in continuity mode. Lesson: a component testing good on the bench and a component testing good as soldered into the circuit are two different tests, and a connection that reads fine under a multimeter's light touch isn't the same as one that survives normal handling.

**A wire-to-wire short, not a joint failure, briefly bricked a board.** While chasing an unrelated ground issue, a continuity check turned up a short between BAT+ and GND with the battery unplugged. Testing each component of the power path in isolation (switch alone, JST connector alone) showed no fault anywhere, which meant the short only existed when everything was reassembled in its original physical position. That's the signature of two wires touching each other along their insulation rather than a bad solder joint at an endpoint, most likely at a nick where insulation had been shaved thin. Shortly after, the board stopped enumerating over USB entirely. The working theory: the XIAO's onboard battery charge management chip actively monitors BAT+ while USB is powering the board, since it's designed to charge a battery through that pin, and a transient short on that line is a plausible way to trip its protection circuitry into a fault state, or damage it outright. The board never recovered. Replacement cost was about $14, and the second build used the same firmware with a corrected circuit.

**JST connector color convention isn't guaranteed.** The battery's JST plug had red and black wires in the opposite order from the pigtail I'd soldered to the board, by color. Pin position is what the connector keys to, not wire color, and in this case the mismatch was cosmetic rather than a wiring error; I confirmed this by measuring voltage across the connection with the switch off before ever powering it on, which read correctly positive. Don't trust wire color on a connector you didn't wire yourself; verify polarity with a meter before the first power-up.

## What I'd change next

- Move button handling from polling to interrupt-driven GPIO, which would make the "interrupt driven" claim accurate and also reduce average current draw slightly, since the chip wouldn't need to stay in an active polling loop between presses.
- Add deep sleep between actions with a GPIO wake-on-interrupt, instead of running the main loop continuously. This is the single biggest lever on battery life and the natural next step once button handling is interrupt-driven.
- Measure actual sleep current with a current profiler (or a multimeter in series on the low end of its range) rather than relying on datasheet estimates, and use that to report a real battery life figure instead of an assumption.
- Calibrate the LiPo voltage-to-percentage curve against a few real discharge cycles instead of a generic resting curve, since actual cell behavior varies.
- Design and print a proper enclosure. It currently lives as a bare board with exposed solder joints, which is fine for bench testing and not fine for permanent mounting in a car.

## Repository structure

```
ble_media_remote.ino   - firmware (Arduino, targets Seeed XIAO nRF52840)
circuit_diagram.svg    - full wiring schematic
wiring_reference.md    - pin assignment table and build notes
README.md              - this file
```

## Flashing

1. Install the Arduino IDE.
2. Add `https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json` under Preferences > Additional Board Manager URLs.
3. In Boards Manager, install "Seeed nRF52 Boards."
4. Select Tools > Board > Seeed nRF52 Boards > Seeed XIAO nRF52840, and pick the correct port.
5. Open the sketch and upload. If the board doesn't appear as a port, double-tap its reset button to force bootloader mode.
