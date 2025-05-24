---
title: "drawbot"
author: "Ewoud Van Vooren"
description: "A drawing machine that is as fast and as cheap as possible"
created_at: "2025-05-25"
---

## Journal
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
 - <img width="622" alt="image" src="https://github.com/user-attachments/assets/43933af5-9d0d-4029-b5b1-4ff4d70892e5" />

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
