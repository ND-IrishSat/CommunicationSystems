```
 /$$$$$$ /$$$$$$$  /$$$$$$  /$$$$$$  /$$   /$$  /$$$$$$   /$$$$$$  /$$$$$$$$                                                            
|_  $$_/| $$__  $$|_  $$_/ /$$__  $$| $$  | $$ /$$__  $$ /$$__  $$|__  $$__/                                                            
  | $$  | $$  \ $$  | $$  | $$  \__/| $$  | $$| $$  \__/| $$  \ $$   | $$                                                               
  | $$  | $$$$$$$/  | $$  |  $$$$$$ | $$$$$$$$|  $$$$$$ | $$$$$$$$   | $$                                                               
  | $$  | $$__  $$  | $$   \____  $$| $$__  $$ \____  $$| $$__  $$   | $$                                                               
  | $$  | $$  \ $$  | $$   /$$  \ $$| $$  | $$ /$$  \ $$| $$  | $$   | $$                                                               
 /$$$$$$| $$  | $$ /$$$$$$|  $$$$$$/| $$  | $$|  $$$$$$/| $$  | $$   | $$                                                               
|______/|__/  |__/|______/ \______/ |__/  |__/ \______/ |__/  |__/   |__/                                                               
                                                                                                                                        
                                                                                                                                        
                                                                                                                                        
 /$$$$$$$$                                                         /$$                                     /$$$$$$$   /$$$$$$  /$$$$$$$ 
|__  $$__/                                                        |__/                                    | $$__  $$ /$$__  $$| $$__  $$
   | $$  /$$$$$$  /$$$$$$  /$$$$$$$   /$$$$$$$  /$$$$$$$  /$$$$$$  /$$ /$$    /$$ /$$$$$$   /$$$$$$       | $$  \ $$| $$  \__/| $$  \ $$
   | $$ /$$__  $$|____  $$| $$__  $$ /$$_____/ /$$_____/ /$$__  $$| $$|  $$  /$$//$$__  $$ /$$__  $$      | $$$$$$$/| $$      | $$$$$$$ 
   | $$| $$  \__/ /$$$$$$$| $$  \ $$|  $$$$$$ | $$      | $$$$$$$$| $$ \  $$/$$/| $$$$$$$$| $$  \__/      | $$____/ | $$      | $$__  $$
   | $$| $$      /$$__  $$| $$  | $$ \____  $$| $$      | $$_____/| $$  \  $$$/ | $$_____/| $$            | $$      | $$    $$| $$  \ $$
   | $$| $$     |  $$$$$$$| $$  | $$ /$$$$$$$/|  $$$$$$$|  $$$$$$$| $$   \  $/  |  $$$$$$$| $$            | $$      |  $$$$$$/| $$$$$$$/
   |__/|__/      \_______/|__/  |__/|_______/  \_______/ \_______/|__/    \_/    \_______/|__/            |__/       \______/ |_______/ 
                                                                                                                                        
                                                                                                                                        
                                                                                                                                        
```

### Team

```
├── Communication Systems
│   ├── Rylan Paul
│   ├── Joaquin Caraveo
│   ├── Caden Niziol
│   ├── Alyssa Riter
│   ├── Anna Arnett
│   ├── Thomas Mancini

```

### Project File Structure

```

├── fp-info-cache
├── fp-lib-table
├── LIB_IRISHSAT.bak
├── LIB_IRISHSAT.kicad_sym                          # All Imported Symbols Library
├── LIB_IRISHSAT.pretty                             # List of all footprints from the `PARTS` folder
│   ├── ...
├── PARTS                                            # List of all parts
│   ├── ABM8_272_T3                                 # Part name
│   │   ├── 2026-02-07_18-50-39.kicad_sym           # Sybmol file
│   │   ├── ABM8-272-T3_ABR.step                    # 3D Model
│   │   └── footprints.pretty                       # Footpints
│   │       └── ABM8-272-T3_ABR.kicad_mod           # Footprint files
|   |
│   ├── ABM8_272_T3                                 # Crystal Oscillator 12MHz for RP2040
│   │   ├── ...
│   ├── AD9218BSTZ-65                               # Analog to Digital Converter
│   │   ├── ...
│   ├── AD9763ASTZ                                  # Digital to Analog Converter
│   │   ├── ...
│   ├── ADA4940_2ACPZ_R7                            # Fully Differential Amplifier (ADC Driver)
│   │   ├── ...
│   ├── ADF4351BCPZ-RL7                             # Local Oscillator
│   │   ├── ...
│   ├── ADL5320ARKZ_R7                              # Power Amplifier
│   │   ├── ...
│   ├── ADL5375_05ACPZ_R7                           # IQ Modulator
│   │   ├── ...
│   ├── ADL5380ACPZ_R7                              # IQ Demodulator
│   │   ├── ...
│   ├── B39921B2672P810                             # Band Pass Filter (ISM 915MHz)
│   │   ├── ...
│   ├── BLM18KG601SN1D                              # Ferrite Capacitor
│   │   ├── ...
│   ├── CFR_25JB_52_43R                             # 43 Ohm Resistor ±5%
│   │   ├── ...
│   ├── DSC1001CI2-100-0000T                        # Crystal Oscillator 100MHz for FPGA
│   │   ├── ...
│   ├── QPL6220QTR7                                 # Low Noise Amplifier
│   │   ├── ...
│   ├── W25Q32JVSSIQ                                # Flash Memory QSPI
│   │   ├── ...
│   └── XC7A35T_1FTG256C                            # FPGA
│       ├── ...
├── ReadME.md                                       # ReadMe
├── sym-lib-table
├── TransceiverPCB-backups
│   ├── TransceiverPCB-2026-02-07_141501.zip
│   ├── TransceiverPCB-2026-02-08_221813.zip
│   └── TransceiverPCB-2026-02-08_225630.zip
├── TransceiverPCB.kicad_pcb                        # Main KiCAD file
├── TransceiverPCB.kicad_prl                        # Main KiCAD file
├── TransceiverPCB.kicad_pro                        # Main KiCAD file
└── TransceiverPCB.kicad_sch                        # Main KiCAD file

```

