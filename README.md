# ESPHome — Helios KWL EC 300 Pro / Vallox via RS485

[![AI-Assisted](https://img.shields.io/badge/AI--assisted-Claude-D97757?logo=claude&logoColor=white)](https://claude.ai/new)
[![ESPHome](https://img.shields.io/badge/ESPHome-2026.3+-blue)](https://esphome.io/) [![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

Composant ESPHome externe pour piloter les VMC double flux **Helios KWL** et **Vallox** via le bus RS485 (protocole DIGIT), avec affichage LVGL sur écran tactile. Intégration native Home Assistant avec auto-découverte de toutes les entités.

> Basé sur le travail original de [Cyril Jaquier](https://github.com/lostcontrol/esphome-helios-kwl) (KWL EC 500 R). Ce fork ajoute le support complet en lecture et écriture de 40+ registres, la gestion des cycles Cheminée et Plein Air, l'écran tactile du LilyGO T-Panel S3, et une architecture protocole conforme à la spécification Vallox DIGIT.

## Aperçu de l'interface (LilyGO T-Panel S3, 480×480)

| Normal (hiver) | Air frais (été) | Cycle Grand Air | VMC éteinte |
|:---:|:---:|:---:|:---:|
| ![Mode Normal](docs/images/ui-normal.png) | ![Mode Air frais](docs/images/ui-air-frais.png) | ![Cycle en cours](docs/images/ui-cycle-grand-air.png) | ![Veille OFF](docs/images/ui-eteinte.png) |

*Captures générées par le simulateur PC [`vmc-preview.yaml`](vmc-preview.yaml) — pixels identiques à la dalle.*

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

### Architecture

Le composant repose sur **4 primitives** de communication strictement conformes au protocole Vallox DIGIT :

| Primitive | Rôle |
|---|---|
| `loop_read_bus()` | Écoute passive continue du bus RS485 dans `loop()`. Intercepte les broadcasts de la carte mère (températures, CO₂, etc.) et les réponses aux polls. Ne publie que les trames destinées aux télécommandes (`dst=0x20`) ou à notre adresse (`dst=0x2F`). |
| `read_register(reg)` | Poll robuste : attend le silence bus (≥10 ms), envoie une requête READ, puis parse le **flux** de réponse — un broadcast qui s'intercale est dispatché normalement au lieu de faire échouer la lecture. 3 tentatives max (protocole §3.1). |
| `write_register(reg, val)` | Écriture protocole strict : 3 ou 4 messages selon le registre (broadcast RC → broadcast CM → direct CM, CRC doublé sur le dernier), journalisée `[CMD]`, puis **relecture de vérification** planifiée au poll suivant et journalisée `[CM] ecriture confirmee` (ou avertissement si la CM renvoie une autre valeur). |
| `write_bit(reg, bit, state)` | Modification d'un bit unique à partir de la dernière valeur lue par les polls (comme la télécommande physique), déléguée à `write_register`. |

**Stratégies de polling :**

| Stratégie | Fréquence | Registres | Description |
|---|---|---|---|
| S1 — Passive | ~12s (broadcasts CM) | 0x2A-0x2C, 0x32-0x35 | Températures, CO₂ interceptés automatiquement |
| S2 — Cyclique | 6s | 0x29, 0xA3, 0x08, 0x71, 0x79, 0x6D, 0x2F, 0x30 | État VMC, vitesse, humidité, boost, alarmes |
| S3 — Config | Au boot puis toutes les heures | registres de configuration | Seuils, consignes, intervalles, modes |
| Secours températures | 60s | 0x32-0x35 | Filet de sécurité : le broadcast s'arrête quand la VMC est éteinte, alors qu'elle répond toujours aux READ |

Les tables de polling sont **construites dynamiquement** : un registre n'est pollé que si une entité YAML consomme sa valeur (zéro trafic bus inutile). Alternance S2/S3 en ratio 5:1, avec un poll S3 supplémentaire par seconde pendant les 2 premières minutes après le boot (tous les réglages remontent dans HA en ~20 s). Un poll S3 en échec est re-tenté après 30 s au lieu d'attendre l'heure suivante, et chaque écriture force la relecture du registre au poll suivant.

### Entités Home Assistant

#### Capteurs (sensors)

| Entité | Type | Description | Registre |
|---|---|---|---|
| Vitesse de ventilation | `sensor` | Vitesse actuelle 1-8 | 0x29 |
| Température air extérieur | `sensor` (°C) | Sonde NTC extérieure | 0x32 |
| Température air rejeté | `sensor` (°C) | Sonde NTC air rejeté | 0x33 |
| Température air intérieur | `sensor` (°C) | Sonde NTC air repris | 0x34 |
| Température air soufflé | `sensor` (°C) | Sonde NTC air pulsé | 0x35 |
| Humidité capteur 1 | `sensor` (%) | Sonde %RH n°1 | 0x2F |
| Humidité capteur 2 | `sensor` (%) | Sonde %RH n°2 | 0x30 |
| Niveau CO₂ | `sensor` (ppm) | CO₂ 16 bits — *fourni commenté dans vmc.yaml (nécessite la sonde)* | 0x2B+0x2C |
| Fin du cycle dans... | `sensor` (min) | Temps restant boost/cheminée | 0x79 |
| Prochain remplacement | `sensor` (mois) | Compteur maintenance | 0xAB |
| Code de diagnostic | `sensor` | Dernier code défaut | 0x36 |
| Gestion thermique (num) | `sensor` | 0=bypass fermé, 1=bypass ouvert | 0x08 bit 1 |
| Santé du Système | `sensor` | 0=OK, 1=filtre, 2=défaut | Agrégé |
| Cycle en cours (num) | `sensor` | 0=normal, 1=plein air, 2=cheminée | 0x71+0xAA |

#### Capteurs textuels (text_sensors)

| Entité | Description |
|---|---|
| Détail du diagnostic | Traduction FR du code défaut |
| Cycle en cours | "Normal" / "Cycle Plein Air" / "Cycle Cheminée" |
| Gestion thermique | "Air frais" / "Chaleur conservée" |

#### Capteurs binaires (binary_sensors)

| Entité | Registre | Note |
|---|---|---|
| Auto-dégivrage | 0x08 bit 4 | Préchauffage actif |
| Alerte Givre | 0x6D bit 7 | Gel échangeur |
| Alerte CO₂ | 0x6D bit 6 | CO₂ > 5000 ppm — *fourni commenté dans vmc.yaml* |
| État des filtres | 0xA3 bit 7 | Maintenance requise |
| Appoint de chaleur | 0xA3 bit 5 | Post-chauffage LED |
| Ventilateur soufflage | 0x08 bit 3 | ⚠️ Logique inversée |
| Ventilateur extraction | 0x08 bit 5 | ⚠️ Logique inversée |
| Contact externe | 0x08 bit 6 | Bornes S |
| Relais défaut | 0x08 bit 2 | ⚠️ Logique inversée |

#### Ventilateur (fan)

| Entité | Description |
|---|---|
| Ventilation | Fan natif ESPHome ON/OFF + 8 vitesses. Synchronisé en permanence avec l'état réel de la VMC via les polls S2 de 0xA3 (power) et 0x29 (speed). Garde au boot : ignore les commandes tant que le premier poll de 0xA3 n'est pas arrivé (~6s). |

#### Interrupteurs (switches)

| Entité | Primitive | Registre |
|---|---|---|
| Gestion intelligente (CO₂) | `write_bit` | 0xA3 bit 1 |
| Gestion intelligente (%HR) | `write_bit` | 0xA3 bit 2 |
| Mode Fraîcheur | `write_bit` | 0xA3 bit 3 |

#### Paramètres numériques (numbers)

| Entité | Primitive | Registre | Plage |
|---|---|---|---|
| Ventilation nominale | `write_register` | 0xA9 | 1-8 |
| Ventilation maximale | `write_register` | 0xA5 | 1-8 |
| Seuil de fraîcheur | `write_register` | 0xAF | 0-25°C |
| Seuil de dégivrage | `write_register` | 0xA7 | -6 à 15°C |
| Seuil Alerte Givre | `write_register` | 0xA8 | -6 à 15°C |
| Hystérésis antigel | `write_register` | 0xB2 | 1-10°C |
| Seuil CO₂ | `write_register` | 0xB3+0xB4 | 500-2000 ppm — *fourni commenté* |
| Seuil Humidité | `write_register` | 0xAE | 1-99% |
| Fréquence d'analyse | `write_bits_masked` | 0xAA bits 0-3 | 1-15 min |
| Ajustement Soufflage | `write_register` | 0xB0 | 65-100% |
| Ajustement Extraction | `write_register` | 0xB1 | 65-100% |
| Intervalle entretien | `write_register` | 0xA6 | 1-15 mois |

#### Sélecteurs (selects)

| Entité | Primitive | Registre |
|---|---|---|
| Cycle commande murale | `write_bit` | 0xAA bit 5 |
| Détection humidité | `write_bit` | 0xAA bit 4 |
| Ventilation maximale forcée | `write_bit` | 0xB5 bit 0 |

#### Boutons (buttons)

| Entité | Séquence d'écriture |
|---|---|
| Cycle Plein Air | `write_bit(0xAA,5,true)` → `write_register(0x79,45)` → `write_bit(0x71,5,true)` |
| Cycle Cheminée | `write_bit(0xAA,5,false)` → `write_register(0x79,15)` → `write_bit(0x71,5,true)` |
| Arrêter le cycle | `write_register(0x79, 1)` — laisse la VMC terminer naturellement en ~1 min |
| Confirmer remplacement filtres | `read_register(0xA6)` → `write_register(0xAB, val)` |

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

```
VMC Helios                          T-Panel S3
(bornier boîtier de connexion)     (bornier RS485)

  Pin 1  Orange 1  + ──────────── V
  Pin 2  Blanc 1   - ──────────── G
  Pin 3  Orange 2  A ──────────── A (L)
  Pin 4  Blanc 2   B ──────────── B (M)
  Pin 5  Gris      M ──────────── GND (DGND)
```

Le T-Panel se raccorde en parallèle de la commande à distance Helios KWL-FB existante. Le bus RS485 supporte jusqu'à 32 appareils.

> **Conseil :** Si la communication ne fonctionne pas, essayez d'inverser A et B. La convention de nommage varie selon les fabricants de transceivers.

---

## Installation

### 1. Ajouter le composant externe

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
  tx_pin: 16
  rx_pin: 15
  baud_rate: 9600
```

### 3. Déclarer le composant Helios

```yaml
helios_kwl:
  id: helios_kwl_0
  uart_id: uart_bus
  update_interval: 1s   # 1 poll toutes les secondes (S2 rotatif)
```

### 4. Configuration complète

Le fichier [`vmc.yaml`](vmc.yaml) est un exemple complet et fonctionnel pour le T-Panel S3, organisé en 9 sections numérotées lisibles de haut en bas. Il expose l'ensemble des entités Home Assistant (fan, sensors, binary_sensors, switches, numbers, selects, buttons — les entités CO₂ sont fournies commentées pour les VMC équipées de la sonde) et embarque une **interface tactile complète** :

- **Header** — heure, date française complète, icônes d'état (bypass, dégivrage, défaut, filtres, Wi-Fi modulé par la puissance du signal)
- **Arc de vitesse 0-8** avec knob tactile (zone de toucher limitée à l'anneau), pastille de mode Normal/Air frais et pastilles de régulation HR%/CO2 — les couleurs de tout l'écran suivent le mode actif (orange = hiver, cyan = été)
- **Grille de mesures** (Intérieur, Soufflé, Cuisine, SdB) avec micro-barres de progression et **échelle de couleurs de confort** ADEME/ARS (température 19-26 °C et humidité 40-60 % = zone verte)
- **Footer** — cycles Grand Air/Cheminée exclusifs avec compte à rebours et bouton Annuler, reset filtres à double confirmation (30 s), interrupteur ON/OFF avec voile d'extinction
- **UI optimiste** (l'état demandé s'affiche immédiatement, la relecture CM réconcilie en 1-2 s) et **veille anti-marquage LCD** après 5 min d'inactivité (frame noire + rétroéclairage coupé + rendu LVGL en pause, réveil au toucher absorbé)

**Avant de l'utiliser**, créez un fichier `secrets.yaml` :
```yaml
wifi_ssid: "VotreSSID"
wifi_password: "VotreMotDePasse"
api_key: "votre-clé-api-esphome"
ota_password: "votre-mot-de-passe-ota"
ap_password: "motdepasse-fallback"
```

Les polices sont téléchargées automatiquement à la compilation depuis leurs sources officielles open source (aucun fichier local à installer) :
- Montserrat via [Google Fonts](https://fonts.google.com/specimen/Montserrat) (`gfonts://`), glyphes limités au nécessaire + accents français complets
- Symboles natifs LVGL (polices `montserrat_XX` intégrées) pour les icônes d'état
- Un unique glyphe [Material Design Icons](https://github.com/Templarian/MaterialDesign-Webfont) (horloge du header, absente des symboles LVGL)

### 5. Simulateur PC (captures d'écran & développement UI)

Le fichier [`vmc-preview.yaml`](vmc-preview.yaml) fait tourner **la même interface** dans une fenêtre 480×480 sur votre ordinateur (souris = doigt), sans matériel : idéal pour itérer sur l'UI ou produire des captures d'écran. Un sélecteur de scénarios de démonstration (été, hiver, cycle actif, alerte, extinction…) permet de mettre en scène chaque état, et les boutons sont fonctionnels (compte à rebours animé).

```bash
# Une seule fois : SDL2 + ESPHome local
sudo apt install libsdl2-dev        # macOS : brew install sdl2 · Windows : via WSL2
python3 -m venv venv && venv/bin/pip install esphome

# Lancer le simulateur
venv/bin/esphome run vmc-preview.yaml
```

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

### Dispatch des trames reçues

| Destinataire | Action | Raison |
|---|---|---|
| `0x20` (broadcast RC) | **Publier** | Valeurs réelles émises par la CM |
| `0x2F` (notre adresse) | **Publier** | Réponse à notre poll |
| `0x10` (broadcast CM) | Ignorer | Échos de nos propres écritures |
| `0x11` (direct CM) | Ignorer | Requêtes, VARIABLE=0x00 pas une valeur |
| `0x21-0x3F` (autres RC) | Ignorer | Trames privées d'autres télécommandes |

### Lecture d'un registre

```
Requête :  01  2F  11  00  [REG]  [CS]
Réponse :  01  11  2F  [REG]  [VAL]  [CS]
```

### Écriture d'un registre (3 messages obligatoires)

```
01  2F  20  [REG]  [VAL]  [CS]         ← broadcast télécommandes
01  2F  10  [REG]  [VAL]  [CS]         ← broadcast cartes mères
01  2F  11  [REG]  [VAL]  [CS]  [CS]   ← direct CM + checksum doublé
```

### Silence bus

Avant toute émission, le composant attend que le bus soit silencieux pendant au moins 10ms (aucun octet UART reçu). À 9600 bps, un paquet de 6 octets prend ~6.3ms, donc 10ms de silence garantit qu'aucune trame n'est en cours de transmission.

---

## Structure du code

```
components/
├── helios_kwl/
│   ├── __init__.py                 # Composant principal (YAML → C++)
│   ├── helios_kwl.h                # Déclarations : 4 primitives + entités
│   ├── helios_kwl.cpp              # Implémentation complète
│   ├── sensor/__init__.py          # 14 sensors
│   ├── binary_sensor/__init__.py   # 9 binary_sensors
│   ├── switch/__init__.py + .h     # 3 switches (CO₂, HR, été)
│   ├── number/__init__.py + .h     # 12 numbers (seuils, vitesses, %)
│   ├── select/__init__.py + .h     # 3 selects (modes)
│   ├── button/__init__.py + .h     # 4 buttons (cycles, maintenance)
│   ├── text_sensor/__init__.py     # 3 text_sensors
│   └── fan/__init__.py             # Fan natif 8 vitesses
```

> Le tactile CST3240 du T-Panel S3 est géré par le composant natif ESPHome
> [`cst226`](https://esphome.io/components/touchscreen/cst226/) (même protocole
> Hynitron) — plus aucun driver externe n'est nécessaire pour l'écran.

---

## Dépannage

| Symptôme | Solution |
|----------|----------|
| Compilation échoue « IRAM overflow » | Ajouter les `sdkconfig_options` (voir ci-dessous) |
| Aucune réponse de la VMC | Vérifier câblage A/B (inverser) · activer `uart: debug:` |
| Températures incohérentes | Sonde VMC défectueuse — vérifier registre 0x36 |
| Écriture `verif KO` dans les logs | Normal occasionnellement (collision bus). Si systématique, vérifier le câblage. |
| Fan bagote ON/OFF après extinction | Vérifier `restore_mode: RESTORE_DEFAULT_OFF` dans le YAML |
| Bouton cycle nécessite 2 appuis | Le cycle précédent n'est peut-être pas terminé. Appuyer d'abord sur "Arrêter le cycle". |

**Libérer de l'IRAM sur ESP32-S3 :**

Les options Kconfig d'ESP-IDF doivent passer par `sdkconfig_options` — des
`-DCONFIG_*` dans `platformio_options` sont silencieusement ignorés.
À noter : le rapport mémoire affiche toujours « IRAM 100 % », c'est normal —
l'éditeur de liens remplit d'abord le segment IRAM dédié de 16 Ko puis
déborde en DIRAM ; la vraie marge se lit sur la ligne DIRAM.

```yaml
esp32:
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH: "y"
      CONFIG_RINGBUF_PLACE_FUNCTIONS_INTO_FLASH: "y"
      CONFIG_ESP_WIFI_IRAM_OPT: "n"
      CONFIG_ESP_WIFI_RX_IRAM_OPT: "n"
      CONFIG_LWIP_IRAM_OPTIMIZATION: "n"
```

---

## Crédits

- **Cyril Jaquier** ([lostcontrol](https://github.com/lostcontrol/esphome-helios-kwl)) — auteur original, KWL EC 500 R
- **loicdugay** — fork T-Panel S3, support complet lecture/écriture 40+ registres, cycles boost/cheminée, interface LVGL
- **Abeer Ash** ([ash-abeer](https://github.com/ash-abeer)) — adaptation initiale du code original au matériel T-Panel S3 (dont le driver tactile CST3240, remplacé depuis par le composant natif ESPHome `cst226`), réalisée sur commande et sous le financement de **loicdugay**
- **Protocole DIGIT** — documentation Vallox / Petteri Kähärä (2011), traduction anglaise (2021)

## Licence

[MIT](LICENSE)
