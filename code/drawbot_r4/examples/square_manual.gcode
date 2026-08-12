; Draw a 100 mm square from the manually-set home corner (no auto-homing).
; Procedure: manually park the pen at the corner, send "M50 X0 Y0" to mark it
; homed, then run this file.
G21
G90
M50 X0 Y0
G0 X50 Y50
M3
G1 X150 Y50 F800
G1 X150 Y150
G1 X50 Y150
G1 X50 Y50
M5
M2
