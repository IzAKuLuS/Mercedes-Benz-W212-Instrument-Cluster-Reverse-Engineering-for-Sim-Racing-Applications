<h1 align="center">Mercedes-Benz W212 Instrument Cluster Reverse Engineering Project</h1>
<h3 align="center">An attempt to turn proprietary hardware into open equipment</h3>

<h2 align="left">Background</h2>
<p align="left">

This project serves as an attempt to turn an instrument cluster from a 2010 Mercedes-Benz E350 Sedan into functional gauges for use in a sim-racing application. For 99% of cars, an instrument cluster is built either for (1) a specific model of a car or (2) a specific generation of car. Once the car becomes obselete, there is oftentimes no clear and obvious way to use the instrument cluster - and by extension other parts of the car - for alternative uses. While some talented individuals eventually find ways to repurpose the instrument clusters of popular vehicles, this is not the case for *every* model of car - like those from the W212 Mercedes-Benz class of vehicles. Hence, this repository serves to document my progress in making the instrument cluster from a W212 Mercedes work with racing simulator software. 
</p>

<h2 align="left">The Instrument Cluster itself</h2>
<p align="left">
<img width="3072" height="4080" alt="Image" src="https://github.com/user-attachments/assets/3a68be12-74af-4a58-9186-0589a0d91018" />
<img width="3072" height="4080" alt="Image" src="https://github.com/user-attachments/assets/f9b517b2-3236-46cb-ac5d-b206275ca03b" />

</p>
<h2 align="left">Starting point</h2>
<p align="left">

To begin, there is not much to the instrument cluster itself. It is basically just a hollow shell with some electronics that control lights and some spinning elements. Since Mercedes-Benz during this era wanted to be particularly *fancy*, they designed the gauges in such a way to inovate from the status quo with the iconic rotating speedometer. This [video](https://youtu.be/4ZdIniJRVmc?t=181) showcases what I mean. This combined with the addition of an analog clock as well as the general design aesthetic made me fall in love with these gauges from the moment I first saw them as a child.

Now as far as trying to power them, there isn't much that one can do when just looking at them head on. As seen in the pictures, there is only 1 connector at the back of the unit and that is it. And of course, **nothing is labeled**. It's almost as if the wonderful people at Daimler were trying their best to prevent people from repurposing those beautiful gauges for other usecases because only a machine like the W212 could ever, ever have them. 

However, in the age of the internet, there is hope in the form of [charm.li](charm.li), which serves as a website that contains within it a massive repository of information across several brands of vehicle manufacturers. Navigating the website to the [page](https://charm.li/Mercedes%20Benz/2010/E%20350%20Sedan%20%28212.056%29%20V6-3.5L%20%28272.980%29/Repair%20and%20Diagnosis/Instrument%20Panel%2C%20Gauges%20and%20Warning%20Indicators/Diagrams/Electrical%20Diagrams/PE54.30-P-2101DAA%20Instrument%20Cluster%20%28IC%29%20Control%20Unit/) that we want, we discover a massive page filled with technical information relating to wiring diagrams and connectors. The one we are interested in is the connector labeled "A1" as it most resembles the connector at the back of the cluster. A quick read of the diagram shows us that, for the 20 pins on the back of the unit, there are only 7 that we care about. <br/>

<img width="832" height="1292" alt="Image" src="https://github.com/user-attachments/assets/8d0ea901-dda2-4d33-b9ca-ad88c922804e" />

As can be seen, there are only 7 pins of interest. [Another look at the internet] tells us that pins 1, 4, and 6 are supplying power and a trigger to the unit - presumably from something like a key ignition being activated or something. That leaves the remaining 4 pins for something. Looking back to the [charm.li](charm.li) [page](https://charm.li/Mercedes%20Benz/2010/E%20350%20Sedan%20%28212.056%29%20V6-3.5L%20%28272.980%29/Repair%20and%20Diagnosis/Instrument%20Panel%2C%20Gauges%20and%20Warning%20Indicators/Diagrams/Electrical%20Diagrams/PE54.30-P-2101DAA%20Instrument%20Cluster%20%28IC%29%20Control%20Unit/) reveals that those remaining four pins are responsible for the CANBUS communications. This unit takes two CANBUS signals. 

</p>

<h2 align="left">Theoretical Wiring Diagram</h2>
<p align="left">

After reading up [a very informative article on how the CANBUS protocol works](https://www.ic-online.com/blog/post/understanding-the-need-for-120-ohm-termination-in-can-networks), I came up with the following diagram. <br/><br/>
<img width="656" height="511" alt="Image" src="https://github.com/user-attachments/assets/12bb4ef5-da5c-4ce7-b1cd-0f4fe629d709" />

As can be seen, there is a 12 volt power supply to provide the +12V, +12V trigger, and GND connections for the instrument cluster. There is also two separate Arduino microcontrollers providing the CAN_H and CAN_L for both of the instrument cluster's CANBUS networks. Each CAN_H and CAN_L is connected to a BUS bar in which all the CANBUS signals head to in order to reach their intended destinations - that being the instrument cluster pins 12,13, 17, and 18. As far as what the purpose of the CANBUS networks are for, they are to provide the signals for enabling certain functionality on the instrument cluster as a result of other signals sent by the car's ECU or other CANBUS nodes within the car's communication system. This [page](https://charm.li/Mercedes%20Benz/2010/E%20350%20Sedan%20%28212.056%29%20V6-3.5L%20%28272.980%29/Repair%20and%20Diagnosis/Instrument%20Panel%2C%20Gauges%20and%20Warning%20Indicators/Instrument%20Cluster%20%2F%20Carrier/Locations/Instrument%20Cluster/) details what each of these individual functions can be. Now in an ideal world, the original ECU of the particular 2010 Mercedes-Benz E350 sedan my gauges are from would be used to provide these signals alongside the other CANBUS node computers within the car; however, since I am trying to emulate those computers as well as the fact that the original car is not in my posession, this leaves me to discover what the specific CANBUS frames I need in order to determine how to activate each individual signal. This is a challenge to be addressed at a later time. 

In regards to the rest of the wiring, the BUS bars can take the place of a row on a breadboard or some other contraption. As this is still in the planning phase, there is some room to make changes before fully committing to hardware. 120 ohm resistors are placed on the ends of the BUS bar in question in order to prevent signals from reflecting back and forth within the BUS, which can lead to communication errors that ultimately cripple the CANBUS network from functioning. By having this resistor here, this problem is omitted and the integrity of the signals are maintained. 

</p>

<h2 align="left">Cracking the code(s)</h2>
<p align="left">

As of this point, I have a wiring diagram for how the system should work on a high level. The problem is that I have no clue as to *what* the exact CANBUS frames are to activate the various functions of the instrument cluster. Now, if I was lucky, I could go to the original car the instrument cluster came from and scan the car's ECU to obtain the codes I need; however, since I do not have said car, this leaves me in a difficult situation. Fortunately, [this video](https://youtu.be/QOX_SNWhKeo?t=957) gives me a clue as to how to proceed from here. Specifically, uncovering the specific CANBUS signals can be done through a trial and error as demonstrated by the video I previously mentioned. Thus, my current focus is in regards to emulating this setup on a test bench in order to understand how the instrument cluster interacts with the CANBUS frames. Once I determine how to activate certain functions, all other functions should follow in due time. 

**To be updated when new progress is made**

</p>

<h3 align="left">Languages and Tools being used so far:</h3>
<p align="left"> <a href="https://www.arduino.cc/" target="_blank" rel="noreferrer"> <img src="https://cdn.worldvectorlogo.com/logos/arduino-1.svg" alt="arduino" width="40" height="40"/> </a> <a href="https://www.cprogramming.com/" target="_blank" rel="noreferrer"> <img src="https://raw.githubusercontent.com/devicons/devicon/master/icons/c/c-original.svg" alt="c" width="40" height="40"/> </a> <a href="https://www.w3schools.com/cpp/" target="_blank" rel="noreferrer"> <img src="https://raw.githubusercontent.com/devicons/devicon/master/icons/cplusplus/cplusplus-original.svg" alt="cplusplus" width="40" height="40"/> </a> <a href="https://www.python.org" target="_blank" rel="noreferrer"> <img src="https://raw.githubusercontent.com/devicons/devicon/master/icons/python/python-original.svg" alt="python" width="40" height="40"/> </a> </p>



---

<h2 align="left">CAN E Progress: From the DBC File to a Working Ignition State</h2>

After the early wiring and coding work was completed, the next useful starting point was the supplied CAN database file:

```text
mercedes_benz_e350_2010(1).dbc
```

A DBC file gives names to CAN messages and describes where known signals are stored inside each payload. It does not automatically guarantee that every message is complete or correct. In this project, it was mainly used as a map for choosing CAN IDs to test.

One important detail is that message IDs inside the DBC are written in decimal. They must be converted to hexadecimal before they are entered into the Arduino program.

| DBC message | Decimal ID | Hexadecimal CAN ID | Current result |
|---|---:|---:|---|
| `DRIVER_CONTROLS` | 69 | `0x045` | Display wake and OK-button control are working. Other steering-wheel buttons are still unknown. |
| `IGNITION` | 581 | `0x245` | The DBC gives no byte or signal definitions. Static payload searches produced no visible response. |
| `WHEEL_SPEEDS` | 515 | `0x203` | Not tested yet. This is the main candidate for future speedometer control. |
| `DOOR_SENSORS` | 643 | `0x283` | Door open and closed states are working. |
| `SEATBELT_SENSORS` | 885 | `0x375` | Not tested yet. |
| `GAS_PEDAL` | 261 | `0x105` | Not validated. The DBC contains an engine-RPM field, but the definition appears incomplete and should not yet be trusted. |
| `GEAR_LEVER` | 109 | `0x06D` | Not tested yet. |
| `GEAR_PACKET` | 115 | `0x073` | Not tested yet. |

The DBC helped confirm the known `0x045` and `0x283` messages and identified `0x245` as an ignition-related candidate. However, the working ignition control was ultimately found on CAN ID `0x001`, which is not defined in the supplied DBC.


<h3 align="left">Experimental limits and current bench setup</h3>

The original vehicle is not available, and there is no access to another W212 with a complete, known-good CAN E and CAN B network. Because of this, the project cannot record authentic traffic from a running car and copy it directly. Progress must come from documentation, candidate messages, controlled experiments, and limited brute-force testing.

The cluster has two separate CAN networks:

| Instrument-cluster pin | Connection |
|---:|---|
| 1 | Ground |
| 4 | +12 V |
| 6 | +12 V |
| 12 | CAN E Low |
| 13 | CAN E High |
| 17 | CAN B Low |
| 18 | CAN B High |

The current hardware only allows one CAN network to be tested at a time. All of the results in this section were obtained on **CAN E only**, using pins 12 and 13. CAN B was not connected during these tests.

The current bench setup is:

- Arduino Mega 2560.
- Seeed Studio CAN-BUS Shield V2.0 with an MCP2515 controller.
- CAN speed of `500 kbit/s`.
- MCP2515 clock setting of `16 MHz`.
- CAN shield chip-select pin `9`.
- Serial Monitor set to `115200 baud` with a newline ending.
- One 120-ohm termination resistor on the CAN shield.
- One 120-ohm termination resistor at the instrument-cluster end.
- About 60 ohms measured across CAN High and CAN Low when the network is powered down.

With power connected but no useful CAN traffic, the analog clock resets to 12:00. The screen, backlighting, warning lamps, and gauge needles otherwise remain inactive.

Because only CAN E is connected, the cluster does not receive information from every system in the vehicle. This is why many system warnings appear after the cluster is placed in a full ignition-on state.

> **Safety note:** The explorer and its automatic search modes are intended for a standalone bench setup only. They should not be used on a complete vehicle.


<h3 align="left">The main Arduino explorer sketch</h3>

A separate Arduino `.ino` sketch was created to make CAN E testing repeatable without editing and re-uploading the program for every payload:

```text
w212_can_e_ignition_explorer(1).ino
```

The sketch continuously services several independent message types:

| Function | CAN ID | Default period | Purpose |
|---|---:|---:|---|
| Display wake and OK button | `0x045` | 50 ms | Wakes the display and can send a short OK-button pulse. |
| Door state | `0x283` | 100 ms | Reports all doors closed or the left-front door open. |
| Active test frame | `0x001` or `0x245` | 100 ms for `0x001`; 20 ms for `0x245` | Sends the ignition candidate or a manually entered payload. |
| Optional key/status frame | `0x2F8` | 100 ms | Sends a candidate companion frame when enabled. |

The sketch is controlled through the Serial Monitor. Important commands include:

```text
help
status
wake on | wake off
ok
door closed | door flopen | door raw XX
key on | key off
rx on | rx off
profile simple | acc | ign | crank | off | zero
sequence start | sequence stop
target 001 | target 245
clear
set XX XX XX XX XX XX XX XX
bit N
toggle N
period N
timing ACTIVE GAP
auto bit | auto byte | auto nibble | auto off
```

The named profiles initially used the following candidate payloads:

| Profile | CAN ID `0x001` payload | Key/status frame |
|---|---|---|
| `simple` | `04 00 00 00 00 00 00 00` | Off |
| `zero` | `00 00 00 00 00 00 00 00` | Off |
| `acc` | `C2 80 CF AD AA 07 10 55` | On |
| `ign` | `CC 80 CF AD AA 07 10 55` | On |
| `crank` | `07 80 CF AD AA 07 10 55` | On |
| `off` | `CF 80 CF AD AA 07 10 55` | On |

The automatic sequence runs:

```text
OFF   for 1.5 seconds
ACC   for 2.0 seconds
IGN   for 3.5 seconds
CRANK for 0.45 seconds
IGN   for 3.5 seconds
OFF   for 1.5 seconds
```

The explorer can also print every frame transmitted by the cluster using `rx on`. This became important because a visual reaction alone does not show exactly when the cluster changes its internal operating state.

The `auto bit`, `auto byte`, and `auto nibble` modes were added to test unknown static payloads on `0x245`. Each candidate is applied for a fixed time, followed by an all-zero gap. These modes make broad searches easier, but they cannot discover messages that require counters, checksums, or several changing bytes.


<h3 align="left">Reconfirming the first working CAN E controls</h3>

Before ignition testing, the earlier display and door results were added to the explorer and confirmed again.

#### Display wake

```text
CAN ID: 0x045
Payload: 00 00 00 00 FF 00 00 00
```

This wakes the multifunction display and shows the `PRE-SAFE Functions Limited` message. It does **not** place the cluster in a full vehicle-on state.

#### Steering-wheel OK button

```text
CAN ID: 0x045
Pressed:  00 00 00 00 FF 02 00 00
Released: 00 00 00 00 FF 00 00 00
```

The explorer sends the `02` value as a short pulse and then releases it. This clears the PRE-SAFE message in the same way as pressing the OK button on the steering wheel.

#### Door states

The DBC describes alternating closed and open bits in byte 0 of `0x283`:

```text
bit 0 = left-front closed
bit 1 = left-front open
bit 2 = right-front closed
bit 3 = right-front open
bit 4 = left-rear closed
bit 5 = left-rear open
bit 6 = right-rear closed
bit 7 = right-rear open
```

The complete states used in later testing are:

```text
All doors closed:
0x283 : 55 00 00 00 00 00 00 00

Left-front door open; other doors closed:
0x283 : 56 00 00 00 00 00 00 00
```

An earlier payload of `02 00 00 00 00 00 00 00` only asserted that the left-front door was open. It did not explicitly report the other doors as closed.


<h3 align="left">Discovering a working ignition state</h3>

The first major test used the simple profile:

```text
0x001 : 04 00 00 00 00 00 00 00
Period: 100 ms
```

This produced the first complete vehicle-on-like response from the cluster:

- Gauge backlighting turned on.
- Display backlighting turned on.
- The warning lamps turned on.
- The display began cycling through warnings for missing vehicle systems.
- Two chimes were heard during the first test.
- The speedometer made a very small movement.

The small speedometer movement was not a valid speed indication because no wheel-speed data was being sent. It was most likely a startup, zeroing, or stepper-motor initialization movement.

The automatic long-profile sequence was then tested. The cluster followed the expected ON and OFF transitions during the sequence. This showed that `0x001` could control the operating state in a repeatable way.

The individual states were then tested separately:

- The ACC candidate produced a limited response.
- The IGN candidate produced the full warning-lamp, backlighting, warning-message, and chime response.
- Similar ignition results were obtained with the driver door open and closed.
- The ignition candidate still woke the cluster when the separate `0x045` wake message was disabled.

This last test was important. It proved that the working `0x001` ignition message can wake the cluster by itself. The `0x045` message is useful for screen and menu interaction, but it is not required for the full ignition-on response.


<h3 align="left">Reducing the ignition messages to one changing byte</h3>

The original ACC, IGN, CRANK, and OFF candidates all shared the same final seven bytes:

```text
80 CF AD AA 07 10 55
```

To determine whether those bytes were needed, each state was tested again with bytes 1 through 7 set to zero.

#### Limited or ACC-like state

```text
0x001 : C2 00 00 00 00 00 00 00
```

Observed result:

- Seat-belt warning lamp turned on.
- The display showed `SRS Malfunction Service Required`.
- Full gauge backlighting did not turn on.
- The complete warning-lamp sequence did not occur.

This is called **limited** or **ACC-like** because it activates only part of the cluster. Its exact Mercedes production meaning is not yet proven.

#### Alternate full-ON state

```text
0x001 : CC 00 00 00 00 00 00 00
```

Observed result:

- All warning lamps turned on.
- Gauge backlighting turned on.
- Display backlighting turned on.
- The display cycled through the missing-system warnings.
- Chimes were heard.

This produced the same main visible response as the simple `04` profile.

#### Delayed OFF state

```text
0x001 : CF 00 00 00 00 00 00 00
```

When this was sent from a confirmed full-ON state, the warning lamps and gauge illumination turned off and the display returned to the pre-start state. The shutdown occurred after about 1.5 seconds.

#### Zero OFF state

```text
0x001 : 00 00 00 00 00 00 00 00
```

This also returned the cluster to the pre-start state. The display remained on long enough to show the PRE-SAFE message and then switched off after roughly one minute.

These tests proved that the shared seven-byte suffix is not needed for the visible `C2`, `CC`, `CF`, or simple `04` behavior. Byte 0 alone is enough for the confirmed bench functions.


<h3 align="left">Testing the optional 0x2F8 key/status frame</h3>

The candidate companion frame was:

```text
0x2F8 : 00 FF 00 00 00 00 00 00
Period: 100 ms
```

The tests showed:

- `0x2F8` by itself caused no visible change.
- `C2` worked without `0x2F8`.
- Adding `0x2F8` while `C2` was active added an `SOS Tele Aid Inoperative` warning.
- `CC` worked without `0x2F8`.
- Adding `0x2F8` after `CC` caused no immediate visible change.

The frame is therefore not required for basic ignition control. It is being interpreted by the cluster in at least one operating state, but its exact meaning is still unknown. The cleanest bench setup leaves it disabled unless its effect is being studied.


<h3 align="left">Confirmed CAN E state map</h3>

The following table records the behavior that has been physically confirmed on this instrument cluster. All frames use CAN ID `0x001`, DLC 8, and a 100 ms period.

| Byte 0 | Bytes 1-7 | Confirmed bench behavior | Current label |
|---:|---|---|---|
| `00` | All zero | Returns the cluster to the pre-start state; display later sleeps | OFF |
| `04` | All zero | Self-wakes the cluster and produces the full startup response | Minimal full ON |
| `C2` | All zero | Seat-belt lamp and SRS warning only | Limited / ACC-like |
| `CC` | All zero | Full warning-lamp, backlighting, display-warning, and chime response | Alternate full ON |
| `CF` | All zero | Returns a full-ON cluster to pre-start after about 1.5 seconds | Delayed OFF |
| `07` | All zero | Returns to pre-start after about 1.5 seconds if held; a 450 ms pulse can be followed by `CC` without losing the ON state | Transitional candidate; exact meaning unknown |

The simplest reliable ON/OFF pair found so far is:

```text
CAN ID: 0x001
DLC: 8
Period: 100 ms

ON:  04 00 00 00 00 00 00 00
OFF: 00 00 00 00 00 00 00 00
```

A staged alternative is:

```text
Limited / ACC-like: C2 00 00 00 00 00 00 00
Full ON:           CC 00 00 00 00 00 00 00
Delayed OFF:       CF 00 00 00 00 00 00 00
```

These labels describe the behavior seen on the bench. They do not prove that the values are the exact production encodings used by every W212.


<h3 align="left">Testing the DBC ignition message at 0x245</h3>

The DBC contains this message:

```text
BO_ 581 IGNITION: 8 XXX
```

The number `581` is decimal, which converts to hexadecimal CAN ID `0x245`. The DBC gives no signal positions, counters, checksum, or valid payload examples for this message.

The explorer tested `0x245` using:

1. Every individual payload bit, for a total of 64 one-bit tests.
2. The values below in each of the eight byte positions:

```text
01 02 03 04 07 08 0F 10 20 40 80 FF
```

3. Values `00` through `0F` in each byte position.

None of the single-bit, selected-byte, or nibble sweeps produced visible or audible feedback from the cluster.

This does not prove that `0x245` is invalid. A real message may require multiple nonzero bytes, an alive counter, a checksum, a changing sequence, a companion frame, or another prerequisite state. Since `0x001` already provides reliable bench control, further blind searches on `0x245` are a lower priority.

Also, an occasional cluster-transmitted hexadecimal ID `0x581` was seen in some logs. That frame is not the same as decimal DBC ID 581. Decimal 581 is `0x245`.


<h3 align="left">Validating the physical results with RX logging</h3>

The cluster transmits its own CAN messages while it is operating. RX logging showed that many IDs are present in both the pre-start and full-ON states, so the presence of a message alone is not useful. The important evidence is a repeatable payload change.

Two cluster-transmitted messages became reliable state indicators:

| Cluster state | CAN ID `0x015` | CAN ID `0x39D` |
|---|---|---|
| Full ON | `3D FE 00 00` | `80 02 FF FF 00 8E 70 XX` |
| OFF / pre-start | `00 FE 00 00` | `FF 02 FF FF 00 8E 70 XX` |

For `0x39D`, byte 0 is the useful state byte:

```text
80 = full-ON state observed
FF = OFF/pre-start state observed
```

The final byte of `0x39D` follows this repeating sequence:

```text
07, 0F, 17, 1F, 27, 2F, 37, 3F,
47, 4F, 57, 5F, 67, 6F, 77, 7F
```

This strongly resembles an alive counter. It should not be treated as a fixed value when checking the cluster state.

The logs also showed several secondary changes:

| CAN ID | Observation | Current interpretation |
|---:|---|---|
| `0x1C1` | Temporarily changed from `00 00 00 00` to `08 00 00 00` during part of the startup sequence, then returned to zero while the cluster stayed on | Startup or warning-lamp prove-out marker; not a steady ON indicator |
| `0x19F` | Changed among forms containing `F0 00`, `FF FF`, `F0 01`, `F0 02`, and other values | Internal display, warning, or operating state; not stable enough for the main ON/OFF check |
| `0x3E1` | Byte 0 briefly changed `11 -> 10 -> 11` near delayed shutdown | Short transition event or acknowledgment; not decoded |
| `0x39F` | Several fields continued changing before, during, and after ignition tests | Likely contains counters or timers rather than one simple ignition flag |

These frames are outputs from the cluster. They are useful for monitoring, but they are not ignition commands that should be replayed.


<h3 align="left">Detailed CF OFF validation</h3>

The `CF` test began with the cluster fully on and reporting:

```text
0x015 : 3D FE 00 00
0x39D : 80 02 FF FF 00 8E 70 XX
```

After changing the command to:

```text
0x001 : CF 00 00 00 00 00 00 00
```

The log showed:

- `0x015` byte 0 changed from `3D` to `00` after about 1.45 seconds.
- `0x39D` byte 0 changed from `80` to `FF` after about 1.50 seconds.
- `0x3E1` briefly changed from `11` to `10` and then returned to `11` near the same transition.
- `0x19F` changed later, so it is a slower secondary state indicator.

This confirmed that byte-zero-only `CF` is a valid delayed OFF command for the current bench setup.


<h3 align="left">Testing 07 as a possible crank transition</h3>

When this value was held continuously after the cluster had reached full ON:

```text
0x001 : 07 00 00 00 00 00 00 00
```

The warning lamps and gauge illumination turned off, and the display returned to the pre-start state after about 1.5 seconds. The RX indicators also changed from the ON values to the OFF values.

This result did not answer whether `07` could be a short crank-like state, because a real crank state would normally be applied briefly and then replaced by ignition ON.

A separate standalone Arduino sketch was therefore created so this timing test could be performed without changing the main explorer:

```text
w212_final_07_pulse_test.ino
```

The automated test performed:

```text
1. CF for 3 seconds.
2. CC until 0x015 = 3D and 0x39D byte 0 = 80 were each confirmed three times.
3. CC for another 500 ms.
4. 07 for a requested 450,000 microseconds.
5. CC for an 8-second observation period.
6. CF for 3 seconds as a final control.
```

The measured `07` pulse was:

```text
Requested: 450,000 us
Measured:  450,432 us
```

During the pulse:

- Every captured `0x015` frame remained `3D FE 00 00`.
- Every captured `0x39D` frame kept byte 0 at `80`.
- The captured `0x1C1` frame remained `00 00 00 00`.
- No unique visual or audible response was observed.

After `CC` was restored:

- `0x015` remained in the ON state.
- `0x39D` remained in the ON state.
- No delayed OFF transition occurred during the eight-second observation.
- No delayed change was found in `0x1C1`, `0x19F`, or `0x3E1` that uniquely identified the pulse.

The final `CF` control still turned the cluster off after the normal delay. This confirmed that the cluster and RX monitoring were functioning correctly during the experiment.

The proven behavior is therefore:

```text
CC -> 07 held longer than about 1.5 seconds:
cluster returns to OFF/pre-start

CC -> 07 for about 450 ms -> CC:
cluster remains fully ON
```

`07` is now a confirmed short transitional candidate, but it is **not yet proven to mean engine cranking**. It may be a true crank state, or it may simply be a non-ON value that does not remain active long enough to pass the cluster's shutdown delay.


<h3 align="left">Why the warning messages and chimes appear</h3>

Once full ignition is simulated, the cluster begins checking for information from the rest of the vehicle. The bench does not currently provide messages from systems such as:

- SRS and restraint control.
- ESP and ABS.
- Engine control.
- Transmission control.
- PRE-SAFE.
- Tele Aid and SOS.
- Tire-pressure and driver-assistance systems.
- Modules that communicate on CAN B.

The cluster therefore cycles through warnings for missing or unavailable systems. Some higher-priority warnings also produce chimes.

These warnings are useful evidence that the cluster entered its full operating state. They do not mean that the missing systems have been simulated or decoded.

Factory documentation also states that the cluster receives information over both CAN E and CAN B, uses Circuit 15 status for normal operating displays, calculates vehicle speed from wheel-speed inputs, and requires engine-running information plus engine speed for the tachometer. This explains why the ignition state can be activated while the gauges still do not show meaningful values.


<h3 align="left">What has now been solved on CAN E</h3>

| Feature | Status |
|---|---|
| CAN E electrical connection | Confirmed |
| Display wake | Confirmed with `0x045` |
| Steering-wheel OK button | Confirmed with `0x045` |
| Other steering-wheel buttons | Not decoded |
| All doors closed | Confirmed with `0x283 : 55 ...` |
| Left-front door open | Confirmed with `0x283 : 56 ...` |
| Minimal full ON | Confirmed with `0x001 : 04 00 00 00 00 00 00 00` |
| Minimal OFF | Confirmed with an all-zero `0x001` payload |
| Limited / ACC-like state | Confirmed with byte 0 `C2` |
| Alternate full ON | Confirmed with byte 0 `CC` |
| Delayed OFF | Confirmed with byte 0 `CF` |
| Sustained `07` | Confirmed to return the cluster to pre-start after about 1.5 seconds |
| 450 ms `07` pulse | Confirmed to preserve the ON state when followed by `CC` |
| Need for separate `0x045` wake during ignition | Not required |
| Need for `0x2F8` during ignition | Not required |
| DBC `0x245` static payload search | No response found |
| Machine-readable ON/OFF validation | Confirmed using cluster outputs `0x015` and `0x39D` |
| Speedometer control | Not yet decoded |
| Tachometer control | Not yet decoded |
| Fuel and temperature gauge control | Not yet decoded |
| CAN B network | Not yet tested |

The most important milestone is that the cluster can now be moved between a repeatable pre-start state and a full vehicle-on-like state using one CAN ID and one changing byte.


<h3 align="left">What remains uncertain</h3>

The current results are **functionally confirmed on this bench cluster**, but several production details remain unknown:

- It has not been proven that `0x001` is the exact message normally sent by the W212 electronic ignition switch.
- The exact original meanings of `04`, `C2`, `CC`, `CF`, and `07` have not been confirmed from an authentic vehicle recording.
- `07` has not been proven to be a real crank state.
- `0x2F8` is interpreted by the cluster in some conditions, but its exact signal meaning is unknown.
- The DBC entry for `0x245` may require a checksum, counter, companion frame, or changing payload that was not covered by the static searches.
- The secondary cluster outputs `0x1C1`, `0x19F`, `0x3E1`, and `0x39F` are not fully decoded.
- Testing one CAN network at a time prevents the bench from reproducing every interaction that would occur in the complete car.

This distinction is important: a message can be useful for a simulator even when its original factory meaning is not fully known.


<h3 align="left">Recommended next steps</h3>

The next work should build on the stable ignition baseline rather than continue broad ignition searches.

1. **Run a matching 450 ms CF control pulse.** Test `CC -> CF for 450 ms -> CC`. If CF behaves exactly like `07`, then the 07 result may be explained by the general 1.5-second shutdown delay. If CF behaves differently, that would strengthen the case that `07` is a distinct transitional state.
2. **Begin wheel-speed testing on `0x203`.** The DBC describes four individual wheel-speed fields and moving-state bits. A first test should use the same low speed on all four wheels and increase it slowly while monitoring the speedometer.
3. **Test seat-belt inputs on `0x375`.** This may change the seat-belt reminder, but it may not remove the separate SRS warning because the restraint-control module is still absent.
4. **Find engine-running and engine-speed messages.** The tachometer is expected to require both an engine-running or Circuit 61 state and a valid RPM value.
5. **Test gear-display candidates `0x06D` and `0x073`.** These may be needed to show Park, Reverse, Neutral, or Drive correctly.
6. **Decode the remaining `0x045` steering-wheel controls.** Up, down, left, right, and back are still needed for complete menu navigation.
7. **Add a second CAN interface and begin CAN B testing.** This is required to simulate functions that do not arrive over CAN E.
8. **Keep recording RX logs.** The `0x015` and `0x39D` markers should be used to confirm that future experiments do not accidentally remove the full-ON state.


<h3 align="left">Files that document this CAN E work</h3>

```text
mercedes_benz_e350_2010(1).dbc
w212_can_e_ignition_explorer(1).ino
w212_final_07_pulse_test.ino
sequence_start_serial_monitor_output.txt
clean_simple_profile_rx_recording.txt
cf_alone_off_command_testing_output.txt
07_only_as_transition_test_output.txt
07_final_test.txt
Comfort_displays_function.pdf
Trip_Computer_function.pdf
Display_operating_conditions_function.pdf
Instrument_cluster_control_unit.pdf
Control_Warning_Messages_Function_Part_1.pdf
Control_Warning_Messages_Function_Part_2.pdf
```

These files provide the DBC definitions, test programs, raw serial logs, wiring information, and factory descriptions used to reach the conclusions above.


<h3 align="left">Current practical CAN E conclusion</h3>

For normal bench operation, the smallest confirmed ignition control is:

```text
CAN ID: 0x001
DLC: 8
Period: 100 ms

FULL ON:
04 00 00 00 00 00 00 00

OFF / PRE-START:
00 00 00 00 00 00 00 00
```

No separate display-wake message and no `0x2F8` key/status message are required for this pair.

This completes the first major CAN E goal: the instrument cluster can now wake itself, run its warning-lamp and illumination sequence, show its operating warnings, produce chimes, and return to its pre-start state on command. The next major goal is to supply valid wheel-speed information so the speedometer can be controlled intentionally.
