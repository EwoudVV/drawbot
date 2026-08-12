G21        ; mm
G90        ; absolute positioning
M5         ; pen up

; go to safe start
G0 X20 Y20 F3000

; --- square border ---
G1 X380 Y20 F1200
G1 X380 Y300
G1 X20 Y300
G1 X20 Y20

M5

; --- diagonals ---
G0 X20 Y20
G1 X380 Y300 F1200

M5
G0 X380 Y20
G1 X20 Y300 F1200

M5

; --- horizontal lines ---
G0 X20 Y80
G1 X380 Y80 F1200

M5
G0 X20 Y160
G1 X380 Y160 F1200

M5
G0 X20 Y240
G1 X380 Y240 F1200

M5

; --- vertical lines ---
G0 X100 Y20
G1 X100 Y300 F1200

M5
G0 X200 Y20
G1 X200 Y300 F1200

M5
G0 X300 Y20
G1 X300 Y300 F1200

M5

; done
M2