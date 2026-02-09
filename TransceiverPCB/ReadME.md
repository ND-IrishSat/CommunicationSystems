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

### To Add a New Part

> 1. Open a new github branch from main named `Import PART#`
> 
> 2. Download symbol, footprints, and 3d model (Ultra Librarian, Snap Magic, etc.)
>
> 3. Unzip the files
>
> 4. Put the new part folder in the `PARTS` folder
>
> 5. Copy all of the new footprints and paste them into the `LIB_IRISHSAT.pretty` folder
>
> 6. If KiCAD is open, in the footprint editor, press the referesh button
>
> 7. Open the KiCAD Symbol Editor
>
> 8. Search for `LIB_IRISHSAT`
>
> 9. Right click `LIB_IRISHSAT` and click `Import Symbol`
>
> 10. Go to your new part in the `PARTS` folder
>
> 11. Click on the `part.kicad_sym` or `part.lib` file to import it
>
> 12. Save
>
> 13. Create a `Pull Request` merging your new branch `Import PART#` into `main` and assign your team lead
>

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

