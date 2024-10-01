# Communication Systems
Created during the fall of 2024, this is an extension to our SDR repo. This is a more updated repo where the development on the sidekiq and usrp takes place.


# Parameters
- test frequencies
    - 70cm | 418274940 Hz | 420 MHz
- Actual frequencies
    - Rx:
    - Tx:
- Hardware
    - amplifiers?
    - antennas

S-Band downlink, but we do not have permission
UHF uplink

Rylan - General liscense


Sidekiq: ports 1 and 2 are received between 50 MHz and 6 GHz
        Port 3 receive or transmit 50 MHz and 6 GHz

Copy to irishsat    
    $ scp -r ~/path/CommunicationSystems/ irishsat:~/Documents/Github/

To compile test apps in the test folder on irishsat desktop run:
        $ make BUILD_CONFIG=arm_cortex-a9.gcc7.2.1_gnueabihf
        Add clean after to clean builds
        This will build the test files in the bin folder

