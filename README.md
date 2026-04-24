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


<h3 align="left">Languages and Tools being used so far:</h3>
<p align="left"> <a href="https://www.arduino.cc/" target="_blank" rel="noreferrer"> <img src="https://cdn.worldvectorlogo.com/logos/arduino-1.svg" alt="arduino" width="40" height="40"/> </a> <a href="https://www.cprogramming.com/" target="_blank" rel="noreferrer"> <img src="https://raw.githubusercontent.com/devicons/devicon/master/icons/c/c-original.svg" alt="c" width="40" height="40"/> </a> <a href="https://www.w3schools.com/cpp/" target="_blank" rel="noreferrer"> <img src="https://raw.githubusercontent.com/devicons/devicon/master/icons/cplusplus/cplusplus-original.svg" alt="cplusplus" width="40" height="40"/> </a> <a href="https://www.python.org" target="_blank" rel="noreferrer"> <img src="https://raw.githubusercontent.com/devicons/devicon/master/icons/python/python-original.svg" alt="python" width="40" height="40"/> </a> </p>


