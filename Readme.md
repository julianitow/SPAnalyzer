# SoulPot Analyzer Firmware

Firmware pour analyzer accompagnant l'application SoulPot
> [Apple iOS]()

> [Android]()

## Installation
Deux options, soit reconstruire le code avec PlatformIO. Soit télécharger le firmware pré-construit dans les releases de ce repo: [Releases](https://github.com/julianitow/SPAnalyzer/tags)

>***Warning***
>Use the file `min_spliffs.csv` to partition the board before uploading the firmware
>Don't forget to change the device id in `include/iot_configs.h` to match to the device id on azure iot hub