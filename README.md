# ESPHome — Helios KWL EC 300 Pro / Vallox via RS485

![AI-Assisted](https://img.shields.io/badge/AI--assisted-Claude-D97757?logo=claude&logoColor=white)
[![ESPHome](https://img.shields.io/badge/ESPHome-2026.3+-blue)](https://esphome.io/) [![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

Composant ESPHome externe pour piloter les VMC double flux **Helios KWL** et **Vallox** via le bus RS485 (protocole DIGIT), avec affichage LVGL sur écran tactile. Intégration native Home Assistant avec auto-découverte de toutes les entités.

> Basé sur le travail original de [Cyril Jaquier](https://github.com/lostcontrol/esphome-helios-kwl) (KWL EC 500 R). Ce fork ajoute le support complet en lecture et écriture de 40+ registres, la gestion des cycles Cheminée et Plein Air, l'écran tactile du LilyGO T-Panel S3, et une architecture protocole conforme à la spécification Vallox DIGIT.

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
| `read_register(reg)` | Poll synchrone : attend le silence bus (≥10ms sans activité UART), envoie une requête READ, attend la réponse 50ms max de façon non-bloquante. |
| `write_register(reg, val)` | Écriture protocole strict : envoie les 3 messages obligatoires (broadcast RC → broadcast CM → direct CM avec CRC doublé), attend 100ms, vérifie par `read_register`, retry 1× si échec. |
| `write_bit(reg, bit, state)` | Modification d'un bit unique : `read_register` frais → modifie le bit → `write_register`. Jamais de cache stale. |

**Stratégies de polling :**

| Stratégie | Fréquence | Registres | Description |
|---|---|---|---|
| S1 — Passive | ~12s (broadcasts CM) | 0x2A-0x2C, 0x32-0x35 | Températures, CO₂ interceptés automatiquement |
| S2 — Cyclique | 6s | 0x29, 0xA3, 0x08, 0x71, 0x79, 0x6D, 0x2F, 0x30 | État VMC, vitesse, humidité, boost, alarmes |
| S3 — Init + 1h | Au boot puis toutes les heures | 21 registres config | Seuils, consignes, intervalles, modes |

L'alternance S2/S3 utilise un ratio 5:1 (5 cycles S2 puis 1 cycle S3) pour garantir que les registres de configuration soient tous lus dans les ~2 minutes après le boot.

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
| Niveau CO₂ | `sensor` (ppm) | CO₂ 16 bits | 0x2B+0x2C |
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
| Alerte CO₂ | 0x6D bit 6 | CO₂ > 5000 ppm |
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
| Seuil CO₂ | `write_register` | 0xB3+0xB4 | 500-2000 ppm |
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

Le fichier [`vmc.yaml`](vmc.yaml) contient un exemple complet et fonctionnel incluant les 14 sensors, 9 binary_sensors, 3 switches, 12 numbers, 3 selects, 4 buttons, 3 text_sensors, 1 fan, l'interface LVGL avec arc de vitesse et grille de données, et la gestion du rétroéclairage.

**Avant de l'utiliser**, créez un fichier `secrets.yaml` :
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
└── cst3240/                        # Composant tactile T-Panel S3
    ├── __init__.py
    └── touchscreen/__init__.py + .cpp + .h
```

---

## Dépannage

| Symptôme | Solution |
|----------|----------|
| Compilation échoue « IRAM overflow » | Ajouter les flags `platformio_options` (voir ci-dessous) |
| Aucune réponse de la VMC | Vérifier câblage A/B (inverser) · activer `uart: debug:` |
| Températures incohérentes | Sonde VMC défectueuse — vérifier registre 0x36 |
| Écriture `verif KO` dans les logs | Normal occasionnellement (collision bus). Si systématique, vérifier le câblage. |
| Fan bagote ON/OFF après extinction | Vérifier `restore_mode: RESTORE_DEFAULT_OFF` dans le YAML |
| Bouton cycle nécessite 2 appuis | Le cycle précédent n'est peut-être pas terminé. Appuyer d'abord sur "Arrêter le cycle". |

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

## Crédits

- **Cyril Jaquier** ([lostcontrol](https://github.com/lostcontrol/esphome-helios-kwl)) — auteur original, KWL EC 500 R
- **loicdugay** — fork T-Panel S3, support complet lecture/écriture 40+ registres, cycles boost/cheminée, interface LVGL
- **Abeer Ash** ([ash-abeer](https://github.com/ash-abeer)) — Développement de l'adaptation du code original au matériel T-Panel, réalisée sur commande et sous le financement de **loicdugay**.
- **Protocole DIGIT** — documentation Vallox / Petteri Kähärä (2011), traduction anglaise (2021)

## Licence

[MIT](LICENSE)
