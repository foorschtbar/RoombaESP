EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L MCU_Module:WeMos_D1_mini U1
U 1 1 5FE90555
P 1550 1750
F 0 "U1" H 1750 900 50  0000 C CNN
F 1 "WeMos_D1_mini" H 2000 1000 50  0000 C CNN
F 2 "Module:WEMOS_D1_mini_light" H 1550 600 50  0001 C CNN
F 3 "https://wiki.wemos.cc/products:d1:d1_mini#documentation" H -300 600 50  0001 C CNN
	1    1550 1750
	1    0    0    -1  
$EndComp
$Comp
L Connector:Conn_01x04_Male J3
U 1 1 5FE99303
P 5950 1900
F 0 "J3" H 5922 1874 50  0000 R CNN
F 1 "LCD" H 5922 1783 50  0000 R CNN
F 2 "Connector_JST:JST_PH_S4B-PH-K_1x04_P2.00mm_Horizontal" H 5950 1900 50  0001 C CNN
F 3 "~" H 5950 1900 50  0001 C CNN
	1    5950 1900
	-1   0    0    1   
$EndComp
$Comp
L Connector:Conn_01x03_Male J2
U 1 1 5FE9A213
P 5950 2300
F 0 "J2" H 5922 2324 50  0000 R CNN
F 1 "SW/LED" H 5922 2233 50  0000 R CNN
F 2 "Connector_JST:JST_PH_S3B-PH-K_1x03_P2.00mm_Horizontal" H 5950 2300 50  0001 C CNN
F 3 "~" H 5950 2300 50  0001 C CNN
	1    5950 2300
	-1   0    0    1   
$EndComp
$Comp
L power:GND #PWR0102
U 1 1 5FEA25EC
P 1550 2550
F 0 "#PWR0102" H 1550 2300 50  0001 C CNN
F 1 "GND" H 1555 2377 50  0000 C CNN
F 2 "" H 1550 2550 50  0001 C CNN
F 3 "" H 1550 2550 50  0001 C CNN
	1    1550 2550
	1    0    0    -1  
$EndComp
$Comp
L Connector:Conn_01x05_Male J4
U 1 1 5FEA7263
P 6000 2850
F 0 "J4" H 5972 2874 50  0000 R CNN
F 1 "ROOMBA SER" H 5972 2783 50  0000 R CNN
F 2 "Connector_JST:JST_PH_S5B-PH-K_1x05_P2.00mm_Horizontal" H 6000 2850 50  0001 C CNN
F 3 "~" H 6000 2850 50  0001 C CNN
	1    6000 2850
	-1   0    0    1   
$EndComp
Text GLabel 5800 2650 0    50   Input ~ 0
RMB_BRC
Text GLabel 5800 2850 0    50   Input ~ 0
RMB_RXD
Text GLabel 5800 2950 0    50   Input ~ 0
RMB_TXD
Text GLabel 5800 3050 0    50   Input ~ 0
RMB_PWR
$Comp
L power:GND #PWR0106
U 1 1 5FEAC797
P 5750 2400
F 0 "#PWR0106" H 5750 2150 50  0001 C CNN
F 1 "GND" V 5755 2272 50  0000 R CNN
F 2 "" H 5750 2400 50  0001 C CNN
F 3 "" H 5750 2400 50  0001 C CNN
	1    5750 2400
	0    1    1    0   
$EndComp
Text GLabel 5750 2300 0    50   Input ~ 0
SW_BTN
Text GLabel 5750 2200 0    50   Input ~ 0
SW_LED
$Comp
L power:GND #PWR0107
U 1 1 5FEB5A92
P 5750 1700
F 0 "#PWR0107" H 5750 1450 50  0001 C CNN
F 1 "GND" V 5755 1572 50  0000 R CNN
F 2 "" H 5750 1700 50  0001 C CNN
F 3 "" H 5750 1700 50  0001 C CNN
	1    5750 1700
	0    1    1    0   
$EndComp
$Comp
L power:+3.3V #PWR0108
U 1 1 5FEB6635
P 5750 1800
F 0 "#PWR0108" H 5750 1650 50  0001 C CNN
F 1 "+3.3V" V 5765 1928 50  0000 L CNN
F 2 "" H 5750 1800 50  0001 C CNN
F 3 "" H 5750 1800 50  0001 C CNN
	1    5750 1800
	0    -1   -1   0   
$EndComp
Text GLabel 5750 1900 0    50   Input ~ 0
LCD_SCL
Text GLabel 5750 2000 0    50   Input ~ 0
LCD_SDA
Text GLabel 1950 2150 2    50   Input ~ 0
RMB_RXD
Text GLabel 4250 3350 2    50   Input ~ 0
RMB_TXD
$Comp
L Device:R R1
U 1 1 5FEB8268
P 3800 3350
F 0 "R1" V 3593 3350 50  0000 C CNN
F 1 "10K" V 3684 3350 50  0000 C CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder" V 3730 3350 50  0001 C CNN
F 3 "~" H 3800 3350 50  0001 C CNN
	1    3800 3350
	0    1    1    0   
$EndComp
$Comp
L Device:R R2
U 1 1 5FEB98B6
P 4100 3350
F 0 "R2" V 3893 3350 50  0000 C CNN
F 1 "6.8K" V 3984 3350 50  0000 C CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder" V 4030 3350 50  0001 C CNN
F 3 "~" H 4100 3350 50  0001 C CNN
	1    4100 3350
	0    1    1    0   
$EndComp
$Comp
L Device:R R5
U 1 1 5FEBED2D
P 3850 2300
F 0 "R5" V 3643 2300 50  0000 C CNN
F 1 "330" V 3734 2300 50  0000 C CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder" V 3780 2300 50  0001 C CNN
F 3 "~" H 3850 2300 50  0001 C CNN
	1    3850 2300
	0    1    1    0   
$EndComp
Text GLabel 4300 2300 2    50   Input ~ 0
SW_LED
$Comp
L Device:R R3
U 1 1 5FEC3837
P 3850 2800
F 0 "R3" V 3643 2800 50  0000 C CNN
F 1 "4,7K" V 3734 2800 50  0000 C CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder" V 3780 2800 50  0001 C CNN
F 3 "~" H 3850 2800 50  0001 C CNN
	1    3850 2800
	0    1    1    0   
$EndComp
Text GLabel 4300 2800 2    50   Input ~ 0
SW_BTN
NoConn ~ 1450 950 
NoConn ~ 1150 1350
NoConn ~ 1150 1650
NoConn ~ 1150 1750
Wire Wire Line
	4000 2300 4300 2300
Text GLabel 1950 1250 2    50   Input ~ 0
MCU_A0
Text GLabel 5450 1400 0    50   Input ~ 0
MCU_A0
$Comp
L power:+3.3V #PWR0111
U 1 1 5FED4F4A
P 5450 1500
F 0 "#PWR0111" H 5450 1350 50  0001 C CNN
F 1 "+3.3V" V 5450 1650 50  0000 L CNN
F 2 "" H 5450 1500 50  0001 C CNN
F 3 "" H 5450 1500 50  0001 C CNN
	1    5450 1500
	0    -1   -1   0   
$EndComp
$Comp
L power:GND #PWR0112
U 1 1 5FED60E7
P 5400 1250
F 0 "#PWR0112" H 5400 1000 50  0001 C CNN
F 1 "GND" V 5400 1100 50  0000 R CNN
F 2 "" H 5400 1250 50  0001 C CNN
F 3 "" H 5400 1250 50  0001 C CNN
	1    5400 1250
	0    1    1    0   
$EndComp
$Comp
L Connector:Conn_01x02_Male J1
U 1 1 5FED6D2E
P 5950 1400
F 0 "J1" H 5900 1400 50  0000 R CNN
F 1 "LDR" H 5900 1300 50  0000 R CNN
F 2 "Connector_JST:JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal" H 5950 1400 50  0001 C CNN
F 3 "~" H 5950 1400 50  0001 C CNN
	1    5950 1400
	-1   0    0    -1  
$EndComp
$Comp
L Device:R R4
U 1 1 5FED9AB3
P 5550 1250
F 0 "R4" V 5757 1250 50  0000 C CNN
F 1 "1K" V 5666 1250 50  0000 C CNN
F 2 "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder" V 5480 1250 50  0001 C CNN
F 3 "~" H 5550 1250 50  0001 C CNN
	1    5550 1250
	0    -1   -1   0   
$EndComp
Wire Wire Line
	5750 1400 5700 1400
Wire Wire Line
	5750 1500 5450 1500
Text GLabel 1950 2050 2    50   Input ~ 0
MCU_D7
Text GLabel 1950 1650 2    50   Input ~ 0
MCU_D3
Text GLabel 1950 1550 2    50   Input ~ 0
MCU_D2
Text GLabel 3900 3500 0    50   Input ~ 0
MCU_D7
Wire Wire Line
	3350 3350 3650 3350
Text GLabel 3350 2300 0    50   Input ~ 0
MCU_D2
Text GLabel 3900 2950 0    50   Input ~ 0
MCU_D3
Wire Wire Line
	3700 2300 3350 2300
Text GLabel 1950 1450 2    50   Input ~ 0
RMB_BRC
Text GLabel 1950 1850 2    50   Input ~ 0
LCD_SDA
Text GLabel 1950 1950 2    50   Input ~ 0
LCD_SCL
NoConn ~ 1950 1350
NoConn ~ 1950 1750
$Comp
L power:+3.3V #PWR0103
U 1 1 5FF24686
P 1650 950
F 0 "#PWR0103" H 1650 800 50  0001 C CNN
F 1 "+3.3V" H 1665 1123 50  0000 C CNN
F 2 "" H 1650 950 50  0001 C CNN
F 3 "" H 1650 950 50  0001 C CNN
	1    1650 950 
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0109
U 1 1 5FF262A9
P 3350 3350
F 0 "#PWR0109" H 3350 3100 50  0001 C CNN
F 1 "GND" V 3355 3222 50  0000 R CNN
F 2 "" H 3350 3350 50  0001 C CNN
F 3 "" H 3350 3350 50  0001 C CNN
	1    3350 3350
	0    1    1    0   
$EndComp
Wire Wire Line
	4000 2800 4100 2800
$Comp
L power:+3.3V #PWR0110
U 1 1 5FF27227
P 3300 2800
F 0 "#PWR0110" H 3300 2650 50  0001 C CNN
F 1 "+3.3V" V 3315 2928 50  0000 L CNN
F 2 "" H 3300 2800 50  0001 C CNN
F 3 "" H 3300 2800 50  0001 C CNN
	1    3300 2800
	0    -1   -1   0   
$EndComp
Wire Wire Line
	3300 2800 3700 2800
Wire Wire Line
	3900 3500 3950 3500
Wire Wire Line
	3950 3500 3950 3350
Connection ~ 3950 3350
Wire Wire Line
	3900 2950 4100 2950
Wire Wire Line
	4100 2950 4100 2800
Connection ~ 4100 2800
Wire Wire Line
	4100 2800 4300 2800
Wire Wire Line
	5700 1250 5700 1400
Connection ~ 5700 1400
Wire Wire Line
	5700 1400 5450 1400
$Comp
L power:GND #PWR0105
U 1 1 5FEBC77D
P 5800 2750
F 0 "#PWR0105" H 5800 2500 50  0001 C CNN
F 1 "GND" V 5805 2622 50  0000 R CNN
F 2 "" H 5800 2750 50  0001 C CNN
F 3 "" H 5800 2750 50  0001 C CNN
	1    5800 2750
	0    1    1    0   
$EndComp
Text GLabel 3900 1300 2    50   Input ~ 0
RMB_PWR
$Comp
L power:GND #PWR0101
U 1 1 600B3CE3
P 3900 1400
F 0 "#PWR0101" H 3900 1150 50  0001 C CNN
F 1 "GND" V 3905 1272 50  0000 R CNN
F 2 "" H 3900 1400 50  0001 C CNN
F 3 "" H 3900 1400 50  0001 C CNN
	1    3900 1400
	0    -1   -1   0   
$EndComp
$Comp
L power:+3.3V #PWR0104
U 1 1 600B4C35
P 3900 1500
F 0 "#PWR0104" H 3900 1350 50  0001 C CNN
F 1 "+3.3V" V 3915 1628 50  0000 L CNN
F 2 "" H 3900 1500 50  0001 C CNN
F 3 "" H 3900 1500 50  0001 C CNN
	1    3900 1500
	0    1    1    0   
$EndComp
$Comp
L Converter_DCDC_Mod:DD4012SA U2
U 1 1 600BABF8
P 3650 1350
F 0 "U2" H 3708 1715 50  0000 C CNN
F 1 "DD4012SA" H 3708 1624 50  0000 C CNN
F 2 "Converter_DCDC_Mod:DD4012SA" H 3708 1533 50  0000 C CNN
F 3 "" H 3650 1500 50  0001 C CNN
	1    3650 1350
	1    0    0    -1  
$EndComp
$EndSCHEMATC
