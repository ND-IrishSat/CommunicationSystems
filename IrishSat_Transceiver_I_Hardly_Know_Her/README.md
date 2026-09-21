# IrishSat Transceiver? I Hardly Know Her

## To Add a New Part

1. Open a new GitHub branch from `main` named `import/PART_NUMBER` (e.g., `import/MAX31855`).
2. Download component symbol, footprints, and 3D model from Ultra Librarian, SnapMagic, Component Search Engine, etc.
3. Unzip the downloaded files.
4. Structure a new folder inside `PARTS/` named after your component (e.g., `PARTS/MAX31855/`).
5. Move the unzipped part files into your new part folder inside `PARTS/`.
6. Copy all of the new footprint files (`.kicad_mod`) and paste them directly into the `LIB_IRISHSAT.pretty/` folder.
7. If KiCad is open, in the Footprint Editor, press the **Refresh** button.
8. Open the KiCad **Symbol Editor**.
9. Search for and select `LIB_IRISHSAT` in the library list.
10. Right-click `LIB_IRISHSAT` and click **Import Symbol...**.
11. Navigate to your new part folder inside `PARTS/`.
12. Select the `part.kicad_sym` or `part.lib` file to import it into `LIB_IRISHSAT`.
13. Save changes and close the Symbol Editor.
14. Open the KiCad **Footprint Editor**.
15. Select your newly added footprint inside `LIB_IRISHSAT`.
16. Click the **Footprint Properties** button (or press `E`).
17. Click on the **3D Models** tab.
18. Click the folder icon under 3D Model(s) and navigate to your part folder inside `PARTS/`.
19. Select your 3D model (usually `.stp` or `.step`).
20. Confirm the position, scale, and rotation alignment.
21. Save and close the Footprint Editor.
22. Create a **Pull Request** merging your branch into `main` and assign your team lead for review.

---

## Project File Structure

```text
IrishSat_Transceiver_I_Hardly_Know_Her/
├── .gitignore                                           # Ignores local backups & temporary files
├── README.md                                            # Project instructions and documentation
├── sym-lib-table                                        # Symbol library table pointing to local symbols
├── fp-lib-table                                         # Footprint library table pointing to local footprints
├── LIB_IRISHSAT.kicad_sym                               # Custom project symbol library
├── LIB_IRISHSAT.pretty/                                 # Custom footprint library folder
│   └── *.kicad_mod                                      # Individual footprint files
├── PARTS/                                               # Component reference and 3D model directory
│   └── EXAMPLE_PART_ID/                                 # Subfolder for individual component assets
│       ├── part.kicad_sym                               # Original downloaded symbol
│       ├── part.step                                    # 3D STEP model
│       └── footprint.kicad_mod                          # Original footprint file
├── IrishSat_Transceiver_I_Hardly_Know_Her.kicad_pro    # Main KiCad project file
├── IrishSat_Transceiver_I_Hardly_Know_Her.kicad_sch    # Main KiCad schematic file
└── IrishSat_Transceiver_I_Hardly_Know_Her.kicad_pcb    # Main KiCad PCB layout file
