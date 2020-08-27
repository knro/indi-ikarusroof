## Ikarus Observatory Roll-Off Driver

### Hardware

Parts:
+ 1x 0.5 HP winch motor.
+ 1x Web-enabled [Din Relay](https://dlidirect.com/products/din-relay-iv)
+ 1x Raspberry PI 4
+ 2x Limit Switches

For more information, please check the [Detailed Contruction Information](https://ikarusobs.com/about/construction.html).

## Software

Motor Open and Close commands are sent to a web-enabled relay using Curl. Limit Switches are conntected to mains to cut off the power to the motor once actuated. The limit switches are NC (Normally Closed) meaning that power is ON when they are NOT actutated. We detect if a limit switch is actutated once the power is CUT OFF. Two phone chargers are connected to the limit switches. They act as a poor-man digital sensor. They output 5v when on and using a simple voltage divider, they are connected to Raspberry PI GPIO v3.3 digital inputs.
 
+ Roof Fully Closed: CLOSE Limit Switch is OFF --> CLOSE Charger OFF --> CLOSE GPIO LOW --> Parked
+ Roof Fully Opened: OPEN Limit Switch is OFF --> OPEN Charger OFF --> OPEN GPIO LOW --> Unparked
+ Roof in between #1 and #2: CLOSE and OPEN limit switches are BOTH ON --> Both Charges ON --> Both GPIOs ON --> Unknown State

The INDI driver is a simple implementation that parks and unparks the roll-off roof and report its status.
