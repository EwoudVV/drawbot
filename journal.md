---
title: "drawbot"
author: "Ewoud Van Vooren"
description: "A drawing machine that is as fast and as cheap as possible"
created_at: "2025-05-25"
---

## Journal

### Total time spent: 25hrs and 35 mins

**18/5/25** *time spent: 1 hr*
 - repo created, organized it
 - made pr to highway repo
 - made bom
    - found all the parts
        - Arduino Uno REV3
        - 12V 2A Power Supply
        - 2020 V Slot Aluminum Extrusion
        - Linear Motion Rod
        - Linear Ball Bearings
        - Stepper Motor Nema 17
        - 5M GT2 Timing Belt 6mm Width
        - Stepper Motor Driver Module

**19/5/25** *time spent: 30 mins*
 - updated journal [[commit](https://github.com/EwoudVV/drawbot/commit/50e35cfa68b8b6b28bf3ccec420033e38243821c)] from yesterday
 - update bom [[commit 1](https://github.com/EwoudVV/drawbot/commit/d7c03af33f3740dc8db9d91a1a45316af86a2cfb)][[commit 2](https://github.com/EwoudVV/drawbot/commit/1cbc16131d8af86ff61878edf334b16d864162b7)]
    - more misc items like screws, servo, bearings etc that i will need, found good links for those
    - made some small changes
 - made a nice readme, can update more [[commit](https://github.com/EwoudVV/drawbot/commit/9d6c27ab3a7f5787f335faa75507cd3d0add2d44)]

**21/5/25** *time spent: 15 mins*
 - found some nice limit switches for homing on digikey, updated bom

**23/5/25** *time spent: 20 mins*
 - found stl files for some parts
 - started designing
 - <img width="600" alt="image" src="https://github.com/user-attachments/assets/43933af5-9d0d-4029-b5b1-4ff4d70892e5" />

**24/5/25** *time spent: 1 hr*
 - researched how to do stuff with aluminum extrusions
 - looked at my printer for inspiration on how to connect those
 - most of the connectors i found required tapping, which i dont have tools for
 - updated bom with good parts i found [[commit](https://github.com/EwoudVV/drawbot/commit/4ae2a4a4eb12693b451b2eabf4d3502016529dd1)]
     - parts include connection parts to connect the extrusions securely
         - they slide in to the v part, and then i tighten them with grub screws so that no tapping threads is required
     - also wheel rollers, 2 bases for them (4 rollers each) and adjusting nuts for the wheel. the rollers roll on the aluminum extrusions, a base on each side. The linear bearings are connected to those for the other axis.
 - update: i found much cheaper parts, specifically for the roller. amazon had a pack of 2 of the whole thing (rollers, adjusters and plate included) for much cheaper than what i first found
 - i also found cheaper aluminum extrusions, that even came with connecter things and screws, so more stuff for less price
 - also got rid of the 12v 2a power supply, it wasnt neccessary bc i have a variable power supply that i can use

**14/6/25** *time spent: 1hr*
 - designed more:
 - <img width="600" alt="image" src="https://github.com/user-attachments/assets/774a2857-dc24-4615-b987-1174145fadbc" />
 - i designed a rough thing of what the roller plates would look like on the extrusions with the right dimensions, and put the linear motion rods in the right place
 - i looked at the reviews for some of the parts listings, and i found that the roller plate had bad reviews (almost everyone said it was assembled wrong, bad quality) so i found a better brand that had much better reviews and that was even cheaper.

**27/6/25** *time spent: 30 mins*
 - made a better replica of the sliding gantry plate on the aluminum extrusions so i can better model a mount for the linear bearings: <img width="600" alt="image" src="https://github.com/user-attachments/assets/4a0390bf-32ba-41c9-b8ae-36078373de46" />

**29/6/25** *time spent: 1 hr*
 - i designed a plate for in between the aluminum gantry plate and the linear bearing holders (which i found the stl for), and it has screw holes with countersunk things for the screws. I also aligned everything perfectly.<img width="600" alt="image" src="https://github.com/user-attachments/assets/5605d525-b185-4258-bc7f-7cbaacfb7c3b" />

**1/7/25** *time spent: 3 hrs*
 - i made the part for the pen lifting system
 - <img width="600" alt="image" src="https://github.com/user-attachments/assets/dae794d0-8a83-4818-abe7-e4049825630b" />
 - first i made the base, which screws on to the bottom of the linear bearings. i made a hole for the pen with a bit bigger diameter for the pen (which i chose to be a Bic Round Stic because it is simple and cylindrical) and i put an extrusion on the bottom of the plate for the pen so it would wobble less.
 - i put a mounting place for the servo
 - for moving the pen up and down, i used a mechanism called a [scotch yoke](https://www.youtube.com/watch?v=4iP_ZPBduSo). it converts rotational movement from the servo into linear movement for the pen.
 - i made the clamp for the screw, which has tabs on the end of the piece that wraps around the pen and is attached to part of the scotch yoke mechanism, and to tighten it, i will put a bolt and nut into the tabs and tighten it around the pen so that it stays in place well.
 - i also tried to remove unnessecary sections of the plate, because i want the draw bot to move fast and have high acceleration, so it needs less mass on the moving parts.
 - <img width="600" alt="image" src="https://github.com/user-attachments/assets/dac6b701-ace8-42c6-bad9-9bec1c4f182e" />
 - <img width="600" alt="image" src="https://github.com/user-attachments/assets/707ed2c8-0a09-47f8-b3e5-42600efe9fb4" />

**23/7/25** *time spent: 15 hrs*
 - kind of a while since my last journal entry but I finished it!
 - I finished the belt system
    - I put idler and drive wheels in the places that they needed to go
    - <img width="896" height="262" alt="image" src="https://github.com/user-attachments/assets/939d7a3b-de5a-48a5-a106-25587f4845be" />
    - I made spots to clamp the belt and the toolhead. The timing belt comes with the clamps.
    - <img width="659" height="305" alt="image" src="https://github.com/user-attachments/assets/251056c5-2044-490f-b1c0-49576f930c40" />
    - I made attatchments at the opposite corners of the motors where idler wheels will go. They attatch to the aluminum extrusions
    - <img width="565" height="488" alt="image" src="https://github.com/user-attachments/assets/6c6d8e55-384c-4f3b-abd0-07393abc1cc7" />
    - I reasearched the CoreXY/cartesian belt design and layed out where the belts would go on the drawing machine, and color coded both of them. I also calculated the lengths of the belts to total to about 4.5 meters, which is good because the belt i will buy is 5 meters.
    - <img width="1109" height="653" alt="image" src="https://github.com/user-attachments/assets/7297f1ad-89dc-468e-821e-04ccabdaa496" />
    - I made attatchements on the other 2 corners for the stepper motor base. I copied the attatchment design for the other side and used it, and made a base.
    - <img width="497" height="414" alt="image" src="https://github.com/user-attachments/assets/345fb99f-b63f-4ef2-ae5b-58deeeaf0577" />

**25/7/25** *time spent: 1 hr*
 - (Im combining stuff I did recently)
 - Im doing final checks from the website before submitting
 - I uploaded some images of the design in the new folder
 - I designed the wiring diagram for the stepper motor drivers and the arduino (which i already have, so i dont need to add it to the bom)
 - <img width="866" height="854" alt="schematic" src="https://github.com/user-attachments/assets/17e3ca47-1548-45f0-8598-33b0a1f5de7f" />

**25/7/25** *time spent: 1hr*
 - I coded some code for the drawing machine. It homes it, then goes to coordinates specified in the serial monitor. [[commit](https://github.com/EwoudVV/drawbot/commit/557fd0dd407ad2a281d6dc4d6faa46b979bcdcc2)]
