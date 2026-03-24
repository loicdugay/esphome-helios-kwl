
# ESPHome — Helios KWL EC 300 Pro / Vallox via RS485
![AI-Assisted](https://img.shields.io/badge/AI--assisted-Claude-D97757?logo=claude&logoColor=white)
[![ESPHome](https://img.shields.io/badge/ESPHome-2026.3+-blue)](https://esphome.io/) [![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

Composant ESPHome externe pour piloter les VMC double flux **Helios KWL** et **Vallox** via le bus RS485 (protocole DIGIT), avec affichage LVGL sur écran tactile. Intégration native Home Assistant avec auto-découverte de toutes les entités.

> Basé sur le travail original de [Cyril Jaquier](https://github.com/lostcontrol/esphome-helios-kwl) (KWL EC 500 R). Ce fork ajoute le support des sondes d'humidité, l'écran tactile du LilyGO T-Panel S3, les alarmes de sécurité, et corrige le protocole d'écriture RS485.

---

## Modèles compatibles

Ce composant utilise le protocole DIGIT RS485, commun à toute la gamme :

| Marque | Modèles testés | Statut |
|--------|---------------|--------|
| **Helios** | KWL EC 300 Pro R | ✅ Testé |
| **Helios** | KWL EC 500 R | ✅ Testé (auteur original) |
| **Vallox** | Digit SE / Digit 2 SE | ⚠️ Compatible (même protocole, non testé) |

Les modèles Helios KWL « ancienne génération » (sans module Ethernet intégré) sont tous compatibles. Les modèles récents avec Ethernet / easyControls utilisent un protocole différent et ne sont **pas** compatibles.

---

## Fonctionnalités

### Actuellement implémentées

| Entité Home Assistant | Type | Description | Registre |
|---|---|---|---|
| Vitesse de ventilation | `fan` (8 vitesses) | Lecture + contrôle de la vitesse | 0x29 |
| Température air extérieur | `sensor` (°C) | Sonde NTC extérieure | 0x32 |
| Température air rejeté | `sensor` (°C) | Sonde NTC air rejeté (exhaust) | 0x33 |
| Température air repris | `sensor` (°C) | Sonde NTC air repris (extract / inside) | 0x34 |
| Température air soufflé | `sensor` (°C) | Sonde NTC air pulsé (supply / incoming) | 0x35 |
| Humidité capteur 1 | `sensor` (%) | Sonde hygrométrique %RH n°1 (optionnelle) | 0x2F |
| Humidité capteur 2 | `sensor` (%) | Sonde hygrométrique %RH n°2 (optionnelle) | 0x30 |
| État d'alimentation | `binary_sensor` | VMC en marche ou arrêtée | 0xA3 bit 0 |
| Mode été / hiver | `switch` + `binary_sensor` | Bypass automatique activé / désactivé | 0xA3 bit 3 |
| Indicateur chauffage | `binary_sensor` | Post-chauffage LED | 0xA3 bit 5 |
| Indicateur défaut | `binary_sensor` | LED défaut | 0xA3 bit 6 |
| Rappel maintenance | `binary_sensor` | Alerte entretien filtre | 0xA3 bit 7 |
| Alarme CO2 élevé | `binary_sensor` | CO2 > 5000 ppm | 0x6D bit 6 |
| Risque gel échangeur | `binary_sensor` | Gel échangeur de chaleur | 0x6D bit 7 |

### Non encore implémentées (contributions bienvenues)

Des dizaines de registres supplémentaires sont documentés dans le protocole. Voir la section [Registres disponibles](#registres-disponibles) pour la liste complète.

---

## Matériel — LilyGO T-Panel S3

Ce projet utilise le [LilyGO T-Panel S3](https://www.lilygo.cc/products/t-panel-s3), une carte tout-en-un qui intègre l'ESP32-S3, un écran tactile, un transceiver RS485 et 8 MB de PSRAM. Aucun composant supplémentaire n'est nécessaire.

| Spécification | Valeur |
|---|---|
| SoC | ESP32-S3 (2 cœurs Xtensa LX7, 240 MHz) |
| Écran | 480 × 480 pixels, RGB parallèle, tactile capacitif CST3240 |
| RS485 | Transceiver intégré (TX = GPIO16, RX = GPIO15) |
| PSRAM | 8 MB QSPI (80 MHz) |
| Flash | 16 MB |
| Alimentation | 24V DC |

> **Attention :** Sur les versions V1.2 et V1.3, les pins RS485 et CAN sont partagés. Ne pas utiliser les deux simultanément.

---

## Câblage

Le bus RS485 de la VMC Helios KWL utilise un câble blindé 5 fils `JY(ST)Y 2×2×0,6 mm² + 0,5 mm²`. Le brochage sur le bornier de la VMC (boîtier de connexion) et sur la commande à distance Helios KWL-FB est identique :

```
Brochage du bornier RS485 (VMC et commande à distance)
┌─────┬───────────┬──────────┬──────────────────────────────┐
│ Pin │ Couleur   │ Signal   │ Description                  │
├─────┼───────────┼──────────┼──────────────────────────────┤
│  1  │ Orange 1  │    +     │ Alimentation +24V DC         │
│  2  │ Blanc 1   │    −     │ Alimentation GND             │
│  3  │ Orange 2  │    A     │ RS485 ligne A                │
│  4  │ Blanc 2   │    B     │ RS485 ligne B                │
│  5  │ Gris      │    M     │ Masse signal (blindage)      │
└─────┴───────────┴──────────┴──────────────────────────────┘
```

### Raccordement au T-Panel S3

Seuls les signaux RS485 (A, B) et la masse signal (M) sont nécessaires. Le T-Panel est alimenté la phase 24V DC (+) et le neutre (-) sur le connecteur rapide prévu à cet effet.

```
VMC Helios                          T-Panel S3
(bornier boîtier de connexion)     (bornier RS485)

  Pin 1	Orange 1	+ ──────────── V
  Pin 2	Blanc 1		- ──────────── G
  Pin 3	Orange 2	A ──────────── A (L)
  Pin 4	Blanc 2		B ──────────── B (M)
  Pin 5	Gris		M ──────────── GND (DGND)
```

Si vous raccordez le T-Panel en parallèle d'une commande à distance Helios KWL-FB existante (les deux restent connectés), câblez le T-Panel sur les mêmes bornes A, B, M du boîtier de connexion de la VMC. Le bus RS485 supporte jusqu'à 32 appareils.

> **Conseil :** Si la communication ne fonctionne pas, essayez d'inverser A et B. La convention de nommage varie selon les fabricants de transceivers. Cela peut venir aussi de votre masse qui est défaillante.

---

## Installation

### 1. Ajouter le composant externe

Le repo contient les deux composants (VMC RS485 et écran tactile CST3240) :

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/loicdugay/esphome-helios-kwl/
```

### 2. Configurer l'UART

```yaml
uart:
  id: uart_bus
  tx_pin: 16       # GPIO16 — connecté au transceiver RS485 intégré
  rx_pin: 15       # GPIO15 — connecté au transceiver RS485 intégré
  baud_rate: 9600
```

Le protocole DIGIT utilise 9600 baud, 8 bits, pas de parité, 1 stop bit (8N1).

### 3. Déclarer le composant Helios

```yaml
helios_kwl:
  id: helios_kwl_0
  update_interval: 2s    # Interroge 1 registre toutes les 2s
                          # Cycle complet (~9 registres) = ~18s
```

### 4. Configuration complète

Le fichier [`vmc.yaml`](vmc.yaml) contient un **exemple complet et fonctionnel** incluant :
- Tous les capteurs (températures, humidité, vitesse)
- Tous les états binaires (alimentation, bypass, chauffage, défaut, maintenance, alarmes CO2/gel)
- Le switch mode hiver et le contrôle ventilateur 8 vitesses
- La configuration matérielle T-Panel S3 (I2C, SPI, écran MIPI RGB, tactile CST3240)
- L'interface LVGL avec arc de vitesse, indicateurs d'état et grille de données
- La gestion du rétroéclairage (veilleuse automatique après 30s)
- L'horloge avec affichage date/heure en français

**Avant de l'utiliser**, créez un fichier `secrets.yaml` à côté :
```yaml
wifi_ssid: "VotreSSID"
wifi_password: "VotreMotDePasse"
api_key: "votre-clé-api-esphome"
ota_password: "votre-mot-de-passe-ota"
ap_password: "motdepasse-fallback"
```

Et placez les polices dans un dossier `fonts/` :
- [RobotoCondensed-Regular.ttf](https://fonts.google.com/specimen/Roboto+Condensed)
- [materialdesignicons-webfont.ttf](https://github.com/Templarian/MaterialDesign-Webfont/blob/master/fonts/materialdesignicons-webfont.ttf)

<details>
<summary>Aperçu des capteurs déclarés dans vmc.yaml (cliquer pour déplier)</summary>

```yaml
sensor:
  - platform: helios_kwl
    helios_kwl_id: helios_kwl_0
    fan_speed:
      name: "Vitesse de ventilation"
    temperature_outside:
      name: "Température air extérieur"
      filters:
        - exponential_moving_average:
            alpha: 0.3
            send_every: 3
    temperature_exhaust:
      name: "Température air rejeté"
      filters:
        - exponential_moving_average:
            alpha: 0.3
            send_every: 3
    temperature_inside:
      name: "Température air repris"
      filters:
        - exponential_moving_average:
            alpha: 0.3
            send_every: 3
    temperature_incoming:
      name: "Température air soufflé"
      filters:
        - exponential_moving_average:
            alpha: 0.3
            send_every: 3
    # Décommenter si sondes KWL-FF installées :
    # humidity1:
    #   name: "Humidité capteur 1"
    # humidity2:
    #   name: "Humidité capteur 2"

binary_sensor:
  - platform: helios_kwl
    helios_kwl_id: helios_kwl_0
    power_state:
      name: "État VMC"
    bypass_state:
      name: "Mode été (bypass auto)"
    heating_indicator:
      name: "Indicateur chauffage"
    fault_indicator:
      name: "Indicateur défaut"
    service_reminder:
      name: "Rappel maintenance"
    co2_alarm:
      name: "Alarme CO2 élevé"
      icon: "mdi:molecule-co2"
    freeze_alarm:
      name: "Risque gel échangeur"
      icon: "mdi:snowflake-alert"

switch:
  - platform: helios_kwl
    helios_kwl_id: helios_kwl_0
    winter_mode:
      name: "Mode hiver"

output:
  - platform: helios_kwl
    helios_kwl_id: helios_kwl_0
    fan_speed:
      id: helios_kwl_speed

fan:
  - platform: speed
    output: helios_kwl_speed
    speed_count: 8
    name: "Ventilation"
    restore_mode: RESTORE_DEFAULT_ON
```
</details>

---

## Protocole RS485 — Référence rapide

### Structure d'une trame (6 octets)

```
┌────────┬────────┬──────────┬──────────┬──────┬──────────┐
│ SYSTEM │ SENDER │ RECEIVER │ VARIABLE │ DATA │ CHECKSUM │
│  0x01  │  0x2F  │  0x11    │  0x29    │ 0x1F │  0x8F    │
└────────┴────────┴──────────┴──────────┴──────┴──────────┘
```

| Champ | Rôle |
|-------|------|
| SYSTEM | Toujours `0x01` |
| SENDER | `0x11`–`0x1F` cartes mères · `0x21`–`0x2F` télécommandes |
| RECEIVER | `0x10` toutes cartes mères · `0x11` carte mère 1 · `0x20` toutes télécommandes |
| CHECKSUM | Somme 8 bits des 5 octets précédents |

### Lecture d'un registre

```
Requête :  01  [SENDER]  [MAINBOARD]  00          [REGISTRE]  [CS]
Réponse :  01  [MAINBOARD]  [SENDER]  [REGISTRE]  [VALEUR]    [CS]
```

### Écriture d'un registre (3 messages obligatoires)

```
01  [SENDER]  20  [REG]  [VAL]  [CS]         ← broadcast télécommandes
01  [SENDER]  10  [REG]  [VAL]  [CS]         ← broadcast cartes mères
01  [SENDER]  11  [REG]  [VAL]  [CS]  [CS]   ← direct + checksum doublé
```

---

## Registres disponibles

### Températures (lecture seule · échelle NTC)

| Reg. | Description |
|------|-------------|
| 0x32 | Air extérieur (outdoor) |
| 0x33 | Air rejeté (exhaust) |
| 0x34 | Air repris (extract) |
| 0x35 | Air soufflé (supply) |
| 0x57 | Cible post-chauffage |

### Humidité (lecture seule · formule : `%RH = (val − 51) / 2.04`)

| Reg. | Description |
|------|-------------|
| 0x2A | Humidité max (tous capteurs) |
| 0x2F | Capteur %RH n°1 |
| 0x30 | Capteur %RH n°2 |

### CO2 (lecture seule · 16 bits · valeur directe en ppm)

| Reg. | Description |
|------|-------------|
| 0x2B | Octet haut |
| 0x2C | Octet bas |
| 0x2D | Capteurs installés (bits 1–5) |

### Vitesse ventilateur

| Reg. | R/W | Description | Codage |
|------|-----|-------------|--------|
| 0x29 | R | Vitesse actuelle | `01`=1  `03`=2  `07`=3  `0F`=4  `1F`=5  `3F`=6  `7F`=7  `FF`=8 |
| 0xA9 | R/W | Vitesse de base | idem |
| 0xA5 | R/W | Vitesse maximale | idem |

### Registre 0xA3 — État et LEDs

| Bit | Description | R | W |
|-----|-------------|---|---|
| 0 | Power (on/off) | ✅ | ✅ |
| 1 | Touche CO2 | ✅ | ✅ |
| 2 | Touche %RH | ✅ | ✅ |
| 3 | Mode été/hiver (bypass auto) | ✅ | ✅ |
| 4 | LED filtre | ✅ | — |
| 5 | LED post-chauffage | ✅ | — |
| 6 | LED défaut | ✅ | — |
| 7 | Rappel maintenance | ✅ | — |

### Registre 0x08 — Ports E/S physiques

| Bit | Description | Note |
|-----|-------------|------|
| 1 | Volet bypass (0 = hiver · 1 = été) | Position physique réelle |
| 3 | Ventilateur soufflage | ⚠️ Logique inversée (0 = ON) |
| 4 | Préchauffage | |
| 5 | Ventilateur extraction | ⚠️ Logique inversée (0 = ON) |
| 6 | Contact cheminée / boost | |

### Registre 0x07

| Bit | Description |
|-----|-------------|
| 5 | Post-chauffage actif |

### Registre 0x6D — Alertes

| Bit | Description |
|-----|-------------|
| 0 | CO2 → accélération demandée |
| 1 | CO2 → ralentissement demandé |
| 2 | %RH → ralentissement demandé |
| 6 | **Alarme CO2 (> 5000 ppm)** |
| 7 | **Alarme gel échangeur** |

### Registre 0x6F — Flags

| Bit | Description |
|-----|-------------|
| 4 | Risque gel serpentin eau |
| 7 | Maître (0) / esclave (1) |

### Registre 0x71 — Boost / cheminée

| Bit | Description |
|-----|-------------|
| 4 | Télécommande fonctionnelle |
| 5 | Activer boost ou cheminée (écrire 1) |
| 6 | Boost / cheminée en cours |

### Registre 0x36 — Dernier défaut

| Val. | Défaut |
|------|--------|
| 0x05 | Sonde air soufflé |
| 0x06 | Alarme CO2 |
| 0x07 | Sonde extérieure |
| 0x08 | Sonde air extrait |
| 0x09 | Gel serpentin eau |
| 0x0A | Sonde air extrait (2) |

### Consignes modifiables (R/W)

| Reg. | Description | Plage |
|------|-------------|-------|
| 0xA4 | Post-chauffage | NTC °C |
| 0xA6 | Intervalle maintenance | 1–15 mois |
| 0xA7 | Seuil préchauffage | NTC °C |
| 0xA8 | Arrêt soufflage | NTC °C |
| 0xAA | Programme (multi-bits) | voir protocole |
| 0xAB | Compteur maintenance | mois restants |
| 0xAE | Consigne humidité | 0x33–0xFF → %RH |
| 0xAF | Température bypass | NTC °C |
| 0xB0 | DC soufflage | 65–100 % |
| 0xB1 | DC extraction | 65–100 % |
| 0xB2 | Hystérésis antigivrage | °C × 3 |
| 0xB3 | CO2 octet haut | 500–2000 ppm |
| 0xB4 | CO2 octet bas | ppm |

### Compteurs (lecture seule)

| Reg. | Description | Unité |
|------|-------------|-------|
| 0x55 | Post-chauffage ON | sec (÷ 2.5 → %) |
| 0x56 | Post-chauffage OFF | sec (÷ 2.5 → %) |
| 0x79 | Temps restant boost / cheminée | minutes |
| 0x2E | Signal entrée analogique mA/V | 0x00–0xFF |

### Contrôle du bus

| Reg. | Description |
|------|-------------|
| 0x8F | Autoriser émissions (DATA = 0, écriture seule) |
| 0x91 | Interdire émissions (DATA = 0, écriture seule) |

---

## Dépannage

| Symptôme | Solution |
|----------|----------|
| Compilation échoue « IRAM overflow » | Ajouter les flags `platformio_options` (voir ci-dessous) |
| Warning « took a long time (400+ ms) » | Vérifier que `update_interval: 2s` et le polling rotatif sont actifs |
| Aucune réponse de la VMC | Vérifier câblage A/B (inverser) · activer `uart: debug:` |
| Températures incohérentes (> ±5 °C) | Sonde VMC défectueuse — vérifier registre 0x36 |

**Libérer de l'IRAM sur ESP32-S3 :**
```yaml
esphome:
  platformio_options:
    board_build.extra_flags:
      - "-DCONFIG_RINGBUF_PLACE_FUNCTIONS_INTO_FLASH=1"
      - "-DCONFIG_LWIP_IRAM_OPTIMIZATION=0"
      - "-DCONFIG_SPI_MASTER_IN_IRAM=0"
```

---

## Documentation

### Liens en ligne

- [Protocole DIGIT RS485 (PDF)](https://wiki.fhem.de/w/images/7/7e/Digit_protocol_english_RS485.pdf) — Annexes A (table NTC) et B (séquences Helios)
- [FHEM Wiki — Helios KWL](https://wiki.fhem.de/wiki/Helios_KWL) — source communautaire du protocole

### Copies locales (dossier `docs/`)

| Fichier | Description |
|---------|-------------|
| [`helios-kwl-ec-300-pro-notice.pdf`](docs/helios-kwl-ec-300-pro-notice.pdf) | Notice constructeur Helios KWL EC 300 Pro (FR pages 45-66) |
| [`vallox-digit-protocol-rs485.pdf`](docs/vallox-digit-protocol-rs485.pdf) | Protocole DIGIT RS485 — registres, trames, séquences Helios |
| [`rs485-bus-capture.txt`](docs/rs485-bus-capture.txt) | Capture brute de trames RS485 du bus VMC (référence débogage) |

---

## Contribuer

Contributions bienvenues, en particulier :

1. **Nouveaux registres** — chaque registre peut devenir une entité HA
2. **Tests sur d'autres modèles** — KWL EC 200 / 340 / 500, Vallox Digit
3. **Composant `climate:`** — intégration HA de premier rang
4. **Interception des broadcasts** — réduire les requêtes actives

### Structure du code

```
components/
├── helios_kwl/                    # Composant VMC RS485
│   ├── __init__.py
│   ├── helios_kwl.h
│   ├── helios_kwl.cpp
│   ├── binary_sensor/__init__.py
│   ├── output/__init__.py + helios_kwl_output.h
│   ├── sensor/__init__.py
│   └── switch/__init__.py + helios_kwl_switch.h
└── cst3240/                       # Composant tactile T-Panel S3
    ├── ___init__.py
    └── touchscreen/__init__.py + .cpp + .h
```

---

## Crédits

- **Cyril Jaquier** ([lostcontrol](https://github.com/lostcontrol/esphome-helios-kwl)) — auteur original, KWL EC 500 R
- **loicdugay** — fork T-Panel S3, sondes d'humidité, alarmes, corrections protocole
- **Protocole DIGIT** — documentation Vallox / Petteri Kähärä (2011), traduction anglaise (2021)

## Licence

[MIT](LICENSE)
