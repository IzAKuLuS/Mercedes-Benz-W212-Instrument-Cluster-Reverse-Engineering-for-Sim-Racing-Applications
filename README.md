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


