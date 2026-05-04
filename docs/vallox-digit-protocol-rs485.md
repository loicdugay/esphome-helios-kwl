# VALLOX DIGIT bus protocol

Translated April 11 2021 from the Finish document written by Vallox/Petteri Kähärä
27.06.2011

Also valid for Helios KWL, with additional comments in Annex B

---

## 1 Common

DIGIT's communication between the modules is based on a bus implemented with a secure twisted pair, where the communication takes place as RS 485 serial communication. The RS 485 design allows a maximum of 32 modules, each containing both a transmitter and a receiver.

---

## 2. Data transmission

- DIGIT communication is based on two-way RS 485 serial communication.
- 9600 bps, no parity (N), 8 data bits, 1 STOP bit.

---

## 3. Protocol

### 3.1 Request / response service principle

The structure of the request / response principle is shown in Figure 1. The module requesting information (requester) determines from which module the information is desired by setting the content of the RECEIVER variable to the module address of the destination. The requester sets the contents of the REQUEST variable to 0 for the information request. What information the requester wants is indicated by setting the content of the VARIABLE variable to the address of the register from which the information is requested.

All modules (except the requester) listen on the bus and the module whose module address corresponds to the contents of the RECEIVER variable forms a response packet in which the DATA variable contains the response to the sent request. This response packet is sent back to the requester.

```
  SYSTEM          SYSTEM
  SENDER          SENDER
  RECIPIENT       RECIPIENT
  REQUEST         VARIABLE
  VARIABLE        DATA
  CHECKSUM        CHECKSUM

  Request         Answer
```

*Figure 1. Updating a network variable on a request / response basis*

The requester waits for a response for a maximum of 10 ms. If the answer is no. time come, send the requester the request packet again and wait again for the max. 10 ms. If 10 responses are not received during the request / wait period, the requesting module enters fault mode.

The request / response principle is used when transferring information from the recipient to the requester, for example when the remote control requests one of the setpoints from the host motherboard.

The request / reply principle is used when transferring information from the recipient to the requester, for example, when the remote control asks the host motherboard for one of the setpoints.

---

### 3.2 End-to-end acknowledged service

The structure of the send / acknowledge principle is shown in Figure 2. The module sending the information determines to which module the information is to be sent by setting the content of the RECEIVER variable to the module address of the destination. What information the sender sends is indicated by setting the contents of the VARIABLE variable to the address of the register whose information is sent. The actual information is set in the DATA variable.

All modules (except the sender) listen to the bus and the module whose module address corresponds to the contents of the RECEIVER variable acknowledges the checksum of the received packet.

```
  SYSTEM          CHECKSUM
  SENDER
  RECIPIENT
  VARIABLE
  DATA
  CHECKSUM

  Sending         Acknowledgment
```

*Figure 2. Updating the network variable with the send / acknowledge principle*

The sender waits for an acknowledgment for a maximum of 10 ms. If the acknowledgment does not time to come, resend the packet and wait again for max. 10 ms. If 10 acknowledgments are not received during the transmission / wait period, the sending module enters fault mode.

The send / acknowledge principle is used when transferring information from the sender to the receiver, for example when the remote control sends a new setpoint to the master-motherboard.

---

### 3.3 Unacknowledged service principle

The structure of the unacknowledged transmission principle is shown in Figure 3. The module sending the information determines to which module group the information is to be sent by setting the content of the RECIPIENT variable is the address of the target group. What information the sender sends is indicated by setting the contents of the VARIABLE variable to the address of the register whose information is sent. The actual information is set in the DATA variable.

All modules (except the sender) listen to the bus and those modules whose group address matches the content of the RECEIVER variable receive the packet without acknowledging it in any way.

```
  SYSTEM
  SENDER
  RECIPIENT
  VARIABLE
  DATA
  CHECKSUM

  Transmission
```

*Figure 3. Updating the network variable with the unacknowledged transmission principle*

The problem with the unacknowledged transmission principle is that the sender does not know if the information went to all the modules for which the information was intended. The unacknowledged transmission principle is used when transmitting information from a sender to multiple recipients, for example, when a remote control sends a new setpoint to other remote controls and a slave motherboard.

---

**SYSTEM:** This variable can be used to isolate separate systems connected to the same bus. Available in 1H-FFH (255 pcs.). Currently implemented only 1, so forced to always set to 1.

**SENDER:** the variable indicates from which module the information in question comes.
- `11H-1FH` = motherboard 1-15.
- `21H-2FH` = remote control 1-15.
- `31H-FFH` = reserved.

**RECIPIENT:** the variable indicates to which module this information comes.
- `10H` = all motherboards.
- `11H-1FH` = motherboard 1-15.
- `20H` = all remote controls.
- `21H-2FH` = remote control 1-15.
- `30H-FFH` = reserved.

**REQUEST:** This variable allows the sender to request the recipient to respond to the request. must always be set to 0.

**VARIABLES:** The variable indicates what information is being processed.

**DATA:** The value of the VARIABLES variable. Possible values are described above in connection with the corresponding variable.

**CHECKSUM:** The sum of the previous bytes in 8-bit. The recipient of the packet himself adds the bytes before the checksum and compares it with the contents of the CHECKSUM variable. If the results do not match, the recipient rejects the received packet.

---

## Register Map

### `06H` — I/O port *(read only)*

> Read only! WRONG! converting more than one bit to one burns the transformer!
> Speed is safely defined in variable 29H.

Fan speed relays:

| Bit | Description | Values |
|-----|-------------|--------|
| bit 0 | speed 1 | 0 = off, 1 = on — read only |
| bit 1 | speed 2 | 0 = off, 1 = on — read only |
| bit 2 | speed 3 | 0 = off, 1 = on — read only |
| bit 3 | speed 4 | 0 = off, 1 = on — read only |
| bit 4 | speed 5 | 0 = off, 1 = on — read only |
| bit 5 | speed 6 | 0 = off, 1 = on — read only |
| bit 6 | speed 7 | 0 = off, 1 = on — read only |
| bit 7 | speed 8 | 0 = off, 1 = on — read only |

---

### `07H` — I/O port

| Bit | Description | Values |
|-----|-------------|--------|
| bit 5 | reheating | 0 = off, 1 = on — read only |

---

### `08H` — I/O port

| Bit | Description | Values |
|-----|-------------|--------|
| bit 1 | damper motor position | 0 = winter, 1 = summer — read only |
| bit 2 | fault information relay | 0 = open, 1 = closed — read only |
| bit 3 | input fan | 0 = on, 1 = off |
| bit 4 | preheating | 0 = off, 1 = on — read only |
| bit 5 | exhaust fan | 0 = on, 1 = off |
| bit 6 | fireplace / boost switch | 0 = open, 1 = closed — read only |

---

### `29H` — CURRENT FAN SPEED

Allowed values:

| Hex | Speed |
|-----|-------|
| `01H` | speed 1 |
| `03H` | speed 2 |
| `07H` | speed 3 |
| `0FH` | speed 4 |
| `1FH` | speed 5 |
| `3FH` | speed 6 |
| `7FH` | speed 7 |
| `FFH` | speed 8 |

---

### `2AH` — MAXIMUM CURRENT MEASURED MOISTURE CONTENT *(read only)*

`33H` = 0% RH, `FFH` = 100% RH — calculation formula: `(x - 51) / 2.04`

---

### `2BH` — CURRENT CURRENT MEASURED CO2 CONCENTRATION upper byte *(read only)*

The CO2 concentration in 16-bit upper byte directly indicates the concentration in PPM.

---

### `2CH` — MAXIMUM CURRENT MEASURED CO2 CONCENTRATION lower byte *(read only)*

The CO2 concentration in 16-bit subbyte directly indicates the concentration in PPM.

---

### `2DH` — CO2 SENSORS INSTALLED ON THE MACHINE *(read only)*

| Bit | Description | Values |
|-----|-------------|--------|
| bit 1 | sensor 1 | 0 = not installed, 1 = installed |
| bit 2 | sensor 2 | 0 = not installed, 1 = installed |
| bit 3 | sensor 3 | 0 = not installed, 1 = installed |
| bit 4 | sensor 4 | 0 = not installed, 1 = installed |
| bit 5 | sensor 5 | 0 = not installed, 1 = installed |

---

### `2EH` — MILLAMPER / VOLTAGE MESSAGE *(read only)*

Current mA / voltage message to the machine on a scale of `00H` to `FFH`.

---

### `2FH` — MEASURED % RH CONCENTRATION FROM SENSOR 1 *(read only)*

`33H` = 0% RH, `FFH` = 100% RH — calculation formula: `(x - 51) / 2.04`

---

### `30H` — MEASURED % RH CONCENTRATION FROM SENSOR 2 *(read only)*

`33H` = 0% RH, `FFH` = 100% RH — calculation formula: `(x - 51) / 2.04`

---

### `32H` — OUTDOOR TEMPERATURE *(read only)*

Outdoor temperature on the NTC sensor scale.

---

### `33H` — EXHAUST TEMPERATURE *(read only)*

Exhaust air temperature on the NTC sensor scale.

---

### `34H` — EXHAUST TEMPERATURE *(read only)*

Exhaust air temperature on the NTC sensor scale.

---

### `35H` — SUPPLY AIR TEMPERATURE *(read only)*

Supply air temperature on the NTC sensor scale.

---

### `36H` — FAULT CONDITION ERROR NUMBER *(read only)*

The number of the last fault:

| Code | Description |
|------|-------------|
| `05H` | Supply air sensor fault |
| `06H` | Carbon dioxide alarm |
| `07H` | Outdoor sensor fault |
| `08H` | Exhaust air sensor fault |
| `09H` | Danger of the water coil freezing |
| `0AH` | Exhaust air sensor fault |

---

### `55H` — POST HEATING ON COUNTER

Post-heating on time in seconds, counter.
As a percentage: `X / 2.5`

---

### `56H` — POST HEATING OFF TIME

Post-heating off-time in seconds, counter.
As a percentage: `X / 2.5`

---

### `57H` — POST HEATING TARGET VALUE *(read only)*

Target temperature of the air blown into the ventilation zone on the NTC sensor scale.

---

### `6DH` — FLAGS 2 flag variables *(read only)*

| Bit | Description | Values |
|-----|-------------|--------|
| bit 0 | CO2 higher speed request | 0 = no, 1 = speed up |
| bit 1 | CO2 lower speed request | 0 = no, 1 = speed down |
| bit 2 | % RH lower speed request | 0 = no, 1 = speed down |
| bit 3 | switch lower speed request | 0 = no, 1 = speed down |
| bit 6 | CO2 alarm | 0 = no, 1 = CO2 alarm |
| bit 7 | cell freeze alarm | 0 = no, 1 = risk of freezing |

---

### `6FH` — FLAGS 4 flag variables *(read only)*

| Bit | Description | Values |
|-----|-------------|--------|
| bit 4 | risk of water coil freezing | 0 = no, 1 = risk of freezing |
| bit 7 | slave / master selection | 0 = slave, 1 = master |

---

### `70H` — FLAGS 5 flag variables

| Bit | Description | Values |
|-----|-------------|--------|
| bit 7 | preheating status flag | 0 = on, 1 = off |

---

### `71H` — FLAGS 6 flag variable

| Bit | Description | Values |
|-----|-------------|--------|
| bit 4 | remote control | 0 = not working, 1 = functioning — read only |
| bit 5 | activating the fireplace switch | read the variable and set this to one |
| bit 6 | fireplace / boost function | 0 = not working, 1 = functioning — read only |

---

### `79H` — FIREPLACE / POWER SWITCH COUNTER *(read only)*

Remaining time of the function in minutes, descending.

---

### `8FH` — TRANSMISSION ALLOWED *(write only)*

The modules are allowed to send data to the RS-485 bus. DATA = always 0.

---

### `91H` — TRANSMISSION PROHIBITED *(write only)*

Modules are prohibited from sending data to the RS-485 bus. DATA = always 0.

---

### `A3H` — SELECT VARIABLES: INDICATOR LIGHTS

| Bit | Description | Values |
|-----|-------------|--------|
| bit 0 | Power key | 0 = off, 1 = on |
| bit 1 | CO2 key | 0 = off, 1 = on |
| bit 2 | % RH key | 0 = off, 1 = on |
| bit 3 | Post Heating key | 0 = off, 1 = on |
| bit 4 | Filter guard LED | 0 = off, 1 = on — read only |
| bit 5 | Post Heating LED | 0 = off, 1 = on — read only |
| bit 6 | fault LED | 0 = off, 1 = on — read only |
| bit 7 | service reminder | 0 = off, 1 = on — read only |

---

### `A4H` — POST HEATING SETPOINT

Post-heating target value on the NTC sensor scale.

---

### `A5H` — MAX FAN SPEED

Maximum fan speed that can be set during adjustments.

Allowed values:

| Hex | Speed |
|-----|-------|
| `01H` | speed 1 |
| `03H` | speed 2 |
| `07H` | speed 3 |
| `0FH` | speed 4 |
| `1FH` | speed 5 |
| `3FH` | speed 6 |
| `7FH` | speed 7 |
| `FFH` | speed 8 |

---

### `A6H` — SERVICE REMINDER INTERVAL

Service reminder interval in months.

---

### `A7H` — PREHEATING SWITCHING TEMPERATURE

Preheating switching temperature on the NTC sensor scale.

---

### `A8H` — SUPPLY AIR FAN STOP TEMPERATURE

Supply air fan stop temperature on the NTC sensor scale.

---

### `A9H` — BASIC FAN SPEED

Allowed values:

| Hex | Speed |
|-----|-------|
| `01H` | speed 1 |
| `03H` | speed 2 |
| `07H` | speed 3 |
| `0FH` | speed 4 |
| `1FH` | speed 5 |
| `3FH` | speed 6 |
| `7FH` | speed 7 |
| `FFH` | speed 8 |

---

### `AAH` — PROGRAM VARIABLE

| Bit | Description | Values |
|-----|-------------|--------|
| bits 0–3 | adjustment interval in 4-bit | — |
| bit 4 | automatic humidity level search | 0 = off, 1 = on |
| bit 5 | boost / fireplace switch status | 0 = fireplace, 1 = boost switch |
| bit 6 | water / radiator model | 0 = electricity, 1 = water |
| bit 7 | cascade control | 0 = off, 1 = on |

---

### `ABH` — SERVICE REMINDER MONTHLY COUNTER

The monthly counter of the service reminder indicates the time remaining in months for the next service alarm. Downward.

---

### `AEH` — BASIC HUMIDITY LEVEL

`33H` = 0% RH, `FFH` = 100% RH — calculation formula: `(x - 51) / 2.04`

---

### `AFH` — BYPASS OPERATING TEMPERATURE

Bypass operating temperature on the NTC sensor scale.

---

### `B0H` — DC SUPPLY AIR FAN CONTROL SETPOINT

DC supply air fan control setpoint as a percentage.

---

### `B1H` — DC EXHAUST FAN CONTROL SETPOINT

DC exhaust fan control setpoint as a percentage.

---

### `B2H` — CELL ANTI-FREEZE TEMPERATURE HYSTERESIS

Hysteresis of cell antifreeze temperatures, `03H` ≅ 1°C.

---

### `B3H` — CARBON DIOXIDE CONTROL SETPOINT 16 BIT

Carbon control setpoint in 16-bit, the upper byte directly indicates the concentration PPM.

---

### `B4H` — CARBON DIOXIDE CONTROL SETPOINT 16 BIT

Carbon control setpoint in 16-bit, lower byte directly indicates PPM concentration.

---

### `B5H` — VARIABLE PROGRAM2

| Bit | Description | Values |
|-----|-------------|--------|
| bit 0 | Maximum speed limit | 0 = with adjustments, 1 = always on |

> The use of variables other than those mentioned above is strictly prohibited!

---

## ANNEX A — CONVERSION TABLE: NTC SENSOR SCALE — °C

| HEX | DEC | °C | HEX | DEC | °C | HEX | DEC | °C | HEX | DEC | °C |
|-----|-----|----|-----|-----|----|-----|-----|----|-----|-----|----|
| 00 | 0 | -74 | 40 | 64 | -12 | 80 | 128 | 9 | C0 | 192 | 34 |
| 01 | 1 | -70 | 41 | 65 | -12 | 81 | 129 | 9 | C1 | 193 | 34 |
| 02 | 2 | -66 | 42 | 66 | -12 | 82 | 130 | 9 | C2 | 194 | 35 |
| 03 | 3 | -62 | 43 | 67 | -11 | 83 | 131 | 10 | C3 | 195 | 35 |
| 04 | 4 | -59 | 44 | 68 | -11 | 84 | 132 | 10 | C4 | 196 | 36 |
| 05 | 5 | -56 | 45 | 69 | -11 | 85 | 133 | 10 | C5 | 197 | 36 |
| 06 | 6 | -54 | 46 | 70 | -10 | 86 | 134 | 11 | C6 | 198 | 37 |
| 07 | 7 | -52 | 47 | 71 | -10 | 87 | 135 | 11 | C7 | 199 | 37 |
| 08 | 8 | -50 | 48 | 72 | -9 | 88 | 136 | 11 | C8 | 200 | 38 |
| 09 | 9 | -48 | 49 | 73 | -9 | 89 | 137 | 12 | C9 | 201 | 38 |
| 0A | 10 | -47 | 4A | 74 | -9 | 8A | 138 | 12 | CA | 202 | 39 |
| 0B | 11 | -46 | 4B | 75 | -8 | 8B | 139 | 12 | CB | 203 | 40 |
| 0C | 12 | -44 | 4C | 76 | -8 | 8C | 140 | 13 | CC | 204 | 40 |
| 0D | 13 | -43 | 4D | 77 | -8 | 8D | 141 | 13 | CD | 205 | 41 |
| 0E | 14 | -42 | 4E | 78 | -7 | 8E | 142 | 13 | CE | 206 | 41 |
| 0F | 15 | -41 | 4F | 79 | -7 | 8F | 143 | 14 | CF | 207 | 42 |
| 10 | 16 | -40 | 50 | 80 | -7 | 90 | 144 | 14 | D0 | 208 | 43 |
| 11 | 17 | -39 | 51 | 81 | -6 | 91 | 145 | 14 | D1 | 209 | 43 |
| 12 | 18 | -38 | 52 | 82 | -6 | 92 | 146 | 15 | D2 | 210 | 44 |
| 13 | 19 | -37 | 53 | 83 | -6 | 93 | 147 | 15 | D3 | 211 | 45 |
| 14 | 20 | -36 | 54 | 84 | -5 | 94 | 148 | 15 | D4 | 212 | 45 |
| 15 | 21 | -35 | 55 | 85 | -5 | 95 | 149 | 16 | D5 | 213 | 46 |
| 16 | 22 | -34 | 56 | 86 | -5 | 96 | 150 | 16 | D6 | 214 | 47 |
| 17 | 23 | -33 | 57 | 87 | -4 | 97 | 151 | 16 | D7 | 215 | 48 |
| 18 | 24 | -33 | 58 | 88 | -4 | 98 | 152 | 17 | D8 | 216 | 48 |
| 19 | 25 | -32 | 59 | 89 | -4 | 99 | 153 | 17 | D9 | 217 | 49 |
| 1A | 26 | -31 | 5A | 90 | -3 | 9A | 154 | 18 | DA | 218 | 50 |
| 1B | 27 | -30 | 5B | 91 | -3 | 9B | 155 | 18 | DB | 219 | 51 |
| 1C | 28 | -30 | 5C | 92 | -3 | 9C | 156 | 18 | DC | 220 | 52 |
| 1D | 29 | -29 | 5D | 93 | -2 | 9D | 157 | 19 | DD | 221 | 53 |
| 1E | 30 | -28 | 5E | 94 | -2 | 9E | 158 | 19 | DE | 222 | 53 |
| 1F | 31 | -28 | 5F | 95 | -2 | 9F | 159 | 19 | DF | 223 | 54 |
| 20 | 32 | -27 | 60 | 96 | -1 | A0 | 160 | 20 | E0 | 224 | 55 |
| 21 | 33 | -27 | 61 | 97 | -1 | A1 | 161 | 20 | E1 | 225 | 56 |
| 22 | 34 | -26 | 62 | 98 | -1 | A2 | 162 | 21 | E2 | 226 | 57 |
| 23 | 35 | -25 | 63 | 99 | -1 | A3 | 163 | 21 | E3 | 227 | 59 |
| 24 | 36 | -25 | 64 | 100 | 0 | A4 | 164 | 21 | E4 | 228 | 60 |
| 25 | 37 | -24 | 65 | 101 | 0 | A5 | 165 | 22 | E5 | 229 | 61 |
| 26 | 38 | -24 | 66 | 102 | 0 | A6 | 166 | 22 | E6 | 230 | 62 |
| 27 | 39 | -23 | 67 | 103 | 1 | A7 | 167 | 22 | E7 | 231 | 63 |
| 28 | 40 | -23 | 68 | 104 | 1 | A8 | 168 | 23 | E8 | 232 | 65 |
| 29 | 41 | -22 | 69 | 105 | 1 | A9 | 169 | 23 | E9 | 233 | 66 |
| 2A | 42 | -22 | 6A | 106 | 2 | AA | 170 | 24 | EA | 234 | 68 |
| 2B | 43 | -21 | 6B | 107 | 2 | AB | 171 | 24 | EB | 235 | 69 |
| 2C | 44 | -21 | 6C | 108 | 2 | AC | 172 | 24 | EC | 236 | 71 |
| 2D | 45 | -20 | 6D | 109 | 3 | AD | 173 | 25 | ED | 237 | 73 |
| 2E | 46 | -20 | 6E | 110 | 3 | AE | 174 | 25 | EE | 238 | 75 |
| 2F | 47 | -19 | 6F | 111 | 3 | AF | 175 | 26 | EF | 239 | 77 |
| 30 | 48 | -19 | 70 | 112 | 4 | B0 | 176 | 26 | F0 | 240 | 79 |
| 31 | 49 | -19 | 71 | 113 | 4 | B1 | 177 | 27 | F1 | 241 | 81 |
| 32 | 50 | -18 | 72 | 114 | 4 | B2 | 178 | 27 | F2 | 242 | 82 |
| 33 | 51 | -18 | 73 | 115 | 5 | B3 | 179 | 27 | F3 | 243 | 86 |
| 34 | 52 | -17 | 74 | 116 | 5 | B4 | 180 | 28 | F4 | 244 | 90 |
| 35 | 53 | -17 | 75 | 117 | 5 | B5 | 181 | 28 | F5 | 245 | 93 |
| 36 | 54 | -16 | 76 | 118 | 5 | B6 | 182 | 29 | F6 | 246 | 97 |
| 37 | 55 | -16 | 77 | 119 | 6 | B7 | 183 | 29 | F7 | 247 | 100 |
| 38 | 56 | -16 | 78 | 120 | 6 | B8 | 184 | 30 | F8 | 248 | 100 |
| 39 | 57 | -15 | 79 | 121 | 6 | B9 | 185 | 30 | F9 | 249 | 100 |
| 3A | 58 | -15 | 7A | 122 | 7 | BA | 186 | 31 | FA | 250 | 100 |
| 3B | 59 | -14 | 7B | 123 | 7 | BB | 187 | 31 | FB | 251 | 100 |
| 3C | 60 | -14 | 7C | 124 | 7 | BC | 188 | 32 | FC | 252 | 100 |
| 3D | 61 | -14 | 7D | 125 | 8 | BD | 189 | 32 | FD | 253 | 100 |
| 3E | 62 | -13 | 7E | 126 | 8 | BE | 190 | 33 | FE | 254 | 100 |
| 3F | 63 | -13 | 7F | 127 | 8 | BF | 191 | 33 | FF | 255 | 100 |

---

## ANNEX B — Comments from translator

### READ — Reading a Register

A device can request a register from the master by sending:

```
VARIABLE = 00H
DATA = <register code>
```

### Power Off

If the master device is switched off (by setting Bit 0 (Power key) in register `A3H`), the master will still respond to READ.

### Broadcasts

The Master device (if not switched off) every 12 seconds broadcasts the important registers:
`2AH` `2BH` `2CH` `32H` `33H` `34H` `35H`, e.g.:

```
01 11 20 2b 00 5d
01 11 20 2c 00 5e
01 11 20 35 94 fb
01 11 20 34 9c 02
01 11 20 32 73 d7
01 11 20 33 7a df
01 11 20 2a 29 85
```

### SETTING — Writing into a Register

Experiences with Helios KWL show, that for setting a register, three recipients must be addressed in a sequence:

1. RECIPIENT `020H` (broadcast)
2. RECIPIENT `010H`
3. RECIPIENT `011H`

This last message MUST be followed by sending its CHECKSUM twice!

```
01 21 20 a9 1f 0a     SET A9h to 1FH
01 21 10 a9 1f fa
01 21 11 a9 1f fb fb
```

Some registers will even be written with RECIPIENT `011H` twice:

```
01 21 20 af a0 91     SET AFH to A0H
01 21 10 af a0 81
01 21 11 af a0 82
01 21 11 af a0 82 82
```

### User Terminal

The user terminal can not be switched via setting Bit 0 (Power key) in register `A3H`.

The user terminal (here ID=`2EH`) requests data for its display every 6 seconds approx, e.g.:

```
01 2e 11 00 a3 e3     READ A3H
01 11 2e a3 09 ec
01 2e 11 00 29 69     READ 29H
01 11 2e 29 1f 88
01 2e 11 00 35 75     READ 35H
01 11 2e 35 94 09
01 2e 11 00 a3 e3     READ A3H
01 11 2e a3 09 ec
01 2e 11 00 71 b1     READ 71H
01 11 2e 71 00 b1
```
