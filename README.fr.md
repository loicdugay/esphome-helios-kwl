# ESPHome — Helios KWL / Vallox · Commande tactile & Home Assistant

**🇫🇷 Français** · [🇬🇧 English](README.md)

![AI-Assisted](https://img.shields.io/badge/AI--assisted-Claude-D97757?logo=claude&logoColor=white)
[![ESPHome](https://img.shields.io/badge/ESPHome-2026.3+-blue)](https://esphome.io/)
[![Release](https://img.shields.io/github/v/release/loicdugay/esphome-helios-kwl)](https://github.com/loicdugay/esphome-helios-kwl/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

**La commande murale de ma VMC Helios est tombée en panne. Remplacement constructeur : ~600 €.**
**Ce projet la remplace par une carte tout-en-un à ~60 € — qui fait beaucoup plus.**

<p align="center">
  <img src="docs/images/mockup-t-panel.png" width="440" alt="Interface tactile sur LilyGO T-Panel S3">
</p>

Un [LilyGO T-Panel S3](https://www.lilygo.cc/products/t-panel-s3) (ESP32-S3 + écran tactile 480×480 + transceiver RS485 intégrés), le protocole DIGIT RS485 de la VMC décodé registre par registre, et vous obtenez :

- 🖐️ **Une commande murale tactile** complète : vitesse, modes, cycles, mesures, entretien
- 🏠 **46 entités Home Assistant** auto-découvertes : capteurs, réglages, diagnostics, historiques
- 🔍 **Une communication vérifiée** : chaque ordre envoyé à la VMC est relu et confirmé dans les logs
- 💶 **Un budget de ~60 €**, sans modification de la VMC — la carte se branche en parallèle du bus existant

> Basé sur le travail original de [Cyril Jaquier](https://github.com/lostcontrol/esphome-helios-kwl) (KWL EC 500 R), poussé jusqu'au remplacement complet de la commande d'origine : 40+ registres en lecture/écriture, cycles Cheminée/Plein Air, interface tactile, vérification d'écriture.

---

## L'interface tactile

| Normal (hiver) | Air frais (été) | Cycle en cours | Veille (OFF) |
|:---:|:---:|:---:|:---:|
| ![Mode Normal](docs/images/ui-normal.png) | ![Mode Air frais](docs/images/ui-air-frais.png) | ![Cycle Grand Air](docs/images/ui-cycle-grand-air.png) | ![VMC éteinte](docs/images/ui-eteinte.png) |

### Mini-notice de l'écran

| Zone | Action |
|---|---|
| **Arc + curseur** | Glisser pour régler la vitesse **1-8**. Le chiffre central affiche la vitesse en cours. |
| **Pastille centrale** (`Normal` / `Air frais`) | Bascule le mode saisonnier. *Normal* (orange) = hiver, récupération de chaleur. *Air frais* (cyan) = été, le bypass peut s'ouvrir pour rafraîchir. Tout l'écran adopte la couleur du mode. |
| **`HR%`** | Active la régulation automatique par l'humidité : la VMC module elle-même la vitesse entre nominale et maximale. |
| **`CO2`** | Active la régulation automatique par le CO₂ (nécessite la sonde en option). |
| **Grille de mesures** | Températures intérieur/soufflé et humidités des pièces. La couleur suit le **confort** (zones ADEME/ARS) : vert = confort (19-26 °C, 40-60 %HR), ambre = chaud/sec, bleu = froid/humide, rouge = trop chaud. |
| **`Grand Air`** | Surventilation à vitesse maximale pendant **45 min** (grand ménage, cuisine odorante…). Compte à rebours affiché, l'autre bouton devient `Annuler`. |
| **`Cheminée`** | Coupe l'extraction pendant **15 min** pour éviter la dépression au démarrage d'un feu. |
| **`Filtres`** | Affiche les mois restants avant remplacement. Un appui → `Confirmer ?` (30 s) → un second appui remet le compteur à zéro après avoir changé les filtres. |
| **`⏻`** | Allume/éteint la VMC. Éteinte, l'écran se voile et seul ce bouton reste actif. |
| **Bandeau supérieur** | Heure, date, et icônes d'état : bypass ouvert, dégivrage, défaut, filtres, Wi-Fi. |
| **Veille** | Après 5 min sans toucher, l'écran s'éteint complètement (anti-marquage). Un toucher le réveille sans risque d'appui accidentel. |

Pour le fonctionnement de la VMC elle-même : [notice constructeur Helios KWL EC 300 Pro](docs/helios-kwl-ec-300-pro-notice.pdf) (archivée dans ce dépôt).

---

## Dans Home Assistant

Toutes les entités sont auto-découvertes et classées (contrôles, capteurs, configuration, diagnostic) :

| Contrôles | Capteurs |
|:---:|:---:|
| ![Contrôles](docs/images/ha-controles.png) | ![Capteurs](docs/images/ha-capteurs.png) |

| Configuration | Diagnostic |
|:---:|:---:|
| ![Configuration](docs/images/ha-configuration.png) | ![Diagnostic](docs/images/ha-diagnostic.png) |

De quoi construire automatisations et tableaux de bord : surventilation quand l'humidité de la salle de bain dépasse un seuil, passage en mode été selon la météo, alerte de remplacement des filtres, suivi des températures de l'échangeur…

---

## Architecture & registres

### Les 4 primitives de communication

Le composant repose sur **4 primitives** strictement conformes au protocole Vallox DIGIT :

| Primitive | Rôle |
|---|---|
| `loop_read_bus()` | Écoute passive continue du bus RS485 dans `loop()`. Intercepte les broadcasts de la carte mère (températures, CO₂, etc.) et les réponses aux polls. Ne publie que les trames destinées aux télécommandes (`dst=0x20`) ou à notre adresse (`dst=0x2F`). |
| `read_register(reg)` | Poll robuste : attend le silence bus (≥10 ms), envoie une requête READ, puis parse le **flux** de réponse — un broadcast qui s'intercale est dispatché normalement au lieu de faire échouer la lecture. 3 tentatives max (protocole §3.1). |
| `write_register(reg, val)` | Écriture protocole strict : 3 ou 4 messages selon le registre (broadcast RC → broadcast CM → direct CM, CRC doublé sur le dernier), journalisée `[CMD]`, puis **relecture de vérification** planifiée au poll suivant et journalisée `[CM] ecriture confirmee` (ou avertissement si la CM renvoie une autre valeur). |
| `write_bit(reg, bit, state)` | Modification d'un bit unique à partir de la dernière valeur lue par les polls (comme la télécommande physique), déléguée à `write_register`. |

### Stratégies de polling

| Stratégie | Fréquence | Registres | Description |
|---|---|---|---|
| S1 — Passive | ~12s (broadcasts CM) | 0x2A-0x2C, 0x32-0x35 | Températures, CO₂ interceptés automatiquement |
| S2 — Cyclique | 6s | 0x29, 0xA3, 0x08, 0x71, 0x79, 0x6D, 0x2F, 0x30 | État VMC, vitesse, humidité, boost, alarmes |
| S3 — Config | Au boot puis toutes les heures | registres de configuration | Seuils, consignes, intervalles, modes |
| Secours températures | 60s | 0x32-0x35 | Filet de sécurité : le broadcast s'arrête quand la VMC est éteinte, alors qu'elle répond toujours aux READ |

Les tables de polling sont **construites dynamiquement** : un registre n'est pollé que si une entité YAML consomme sa valeur (zéro trafic bus inutile). Alternance S2/S3 en ratio 5:1, avec un poll S3 supplémentaire par seconde pendant les 2 premières minutes après le boot (tous les réglages remontent dans HA en ~20 s). Un poll S3 en échec est re-tenté après 30 s au lieu d'attendre l'heure suivante, et chaque écriture force la relecture du registre au poll suivant.

### Entités Home Assistant

Le référentiel complet (chaque entité, son registre, sa primitive, sa logique) est documenté dans [`docs/helios-kwl-referentiel-v7.md`](docs/helios-kwl-referentiel-v7.md).

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

## Modèles de VMC compatibles

Ce composant utilise le protocole DIGIT RS485, commun à toute la gamme :

| Marque | Modèles testés | Statut |
|--------|---------------|--------|
| **Helios** | KWL EC 300 Pro R | ✅ Testé |
| **Helios** | KWL EC 500 R | ✅ Testé (auteur original) |
| **Vallox** | Digit SE / Digit 2 SE | ⚠️ Compatible (même protocole, non testé) |

Les modèles Helios KWL « ancienne génération » (sans module Ethernet intégré) sont tous compatibles. Les modèles récents avec Ethernet / easyControls utilisent un protocole différent et ne sont **pas** compatibles.

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

## Câblage

> ⚠️ **Coupez l'alimentation de la VMC avant toute intervention sur le bornier.**

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

La méthode recommandée : **partir du fichier [`vmc.yaml`](vmc.yaml)**, l'exemple complet et fonctionnel pour le T-Panel S3 (toutes les entités + l'interface tactile + la veille anti-marquage). Copiez-le dans votre configuration ESPHome, puis :

**1. Créez un fichier `secrets.yaml` :**
```yaml
wifi_ssid: "VotreSSID"
wifi_password: "VotreMotDePasse"
api_key: "votre-clé-api-esphome"
ota_password: "votre-mot-de-passe-ota"
ap_password: "motdepasse-fallback"
```

**2. Adaptez les substitutions** en tête de fichier (noms des pièces des sondes d'humidité, fuseau horaire), et décommentez les entités CO₂ si votre VMC est équipée de la sonde.

**3. Compilez et flashez.** Les polices sont téléchargées automatiquement à la compilation depuis leurs sources open source officielles (Montserrat via Google Fonts, symboles LVGL natifs) — aucun fichier local à installer.

Pour intégrer le composant dans une configuration existante (sans l'écran) :

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/loicdugay/esphome-helios-kwl/
    components: [helios_kwl]

uart:
  id: uart_bus
  tx_pin: 16
  rx_pin: 15
  baud_rate: 9600

helios_kwl:
  id: helios_kwl_0
  uart_id: uart_bus
  update_interval: 1s   # 1 poll par seconde (rotation S2/S3)
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
| VARIABLE | `0x00` = requête de lecture, sinon numéro du registre écrit |
| DATA | Registre demandé (lecture) ou valeur écrite |
| CHECKSUM | Somme des 5 octets précédents, modulo 256 |

La traduction anglaise complète de la spécification est dans [`docs/vallox-digit-protocol-rs485.md`](docs/vallox-digit-protocol-rs485.md).

## Dépannage

| Symptôme | Solution |
|----------|----------|
| Compilation échoue « IRAM overflow » | Ajouter les `sdkconfig_options` (voir ci-dessous) |
| Aucune réponse de la VMC | Vérifier câblage A/B (inverser) · activer `uart: debug:` |
| Températures incohérentes | Sonde VMC défectueuse — vérifier registre 0x36 |
| Écriture non confirmée (`[CM] ... la CM renvoie ...`) | Normal occasionnellement (registre volatil). Si systématique, vérifier le câblage. |
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
- **Protocole DIGIT** — documentation Vallox / Petteri Kähärä (2011) : [spécification originale (PDF, archivée dans ce dépôt)](docs/Archive/vallox-digit-protocol-rs485.pdf) · [traduction anglaise (markdown)](docs/vallox-digit-protocol-rs485.md) · [wiki FHEM Vallox](https://wiki.fhem.de/wiki/Vallox) · [copie communautaire (Symcon)](https://community.symcon.de/uploads/short-url/fp2ucSqkcPPBqQ4Lc2UWcsJr4Pn.pdf)

## ⚠️ Avertissement — utilisation à vos risques et périls

Ce projet est un travail communautaire **fourni « EN L'ÉTAT », sans garantie d'aucune sorte**, expresse ou implicite, y compris — sans s'y limiter — les garanties de qualité marchande, d'adéquation à un usage particulier et d'absence de contrefaçon (voir la [licence MIT](LICENSE)).

**En utilisant ce projet, vous reconnaissez et acceptez que :**

- Vous intervenez sur un **équipement de ventilation raccordé au secteur**. Coupez toujours l'alimentation avant toute intervention sur le câblage. Faites appel à un professionnel qualifié en cas de doute.
- L'écriture de registres non documentés par le constructeur peut **endommager votre VMC de façon irréversible** (la spécification Vallox interdit explicitement certains registres — ce composant ne les utilise pas, mais toute modification du code est sous votre responsabilité).
- L'utilisation de ce projet peut **annuler la garantie constructeur** de votre équipement.
- Dans toute la mesure permise par la loi applicable, les auteurs et contributeurs **ne sauraient être tenus responsables d'aucun dommage** direct, indirect, accessoire, consécutif ou spécial (dégradation du matériel, de l'installation, perte de jouissance, frais de remplacement ou de réparation, blessure…) résultant de l'utilisation ou de l'impossibilité d'utiliser ce logiciel et la documentation associée, même s'ils ont été informés de la possibilité de tels dommages.
- Ce projet est **indépendant** : il n'est ni affilié à, ni approuvé, ni supporté par Helios Ventilatoren, Vallox Oy ou LilyGO. Les marques citées appartiennent à leurs propriétaires respectifs.

## Licence

Distribué sous licence [MIT](LICENSE). Vous êtes libre de l'utiliser, le modifier et le redistribuer, à condition de conserver la notice de copyright — et la clause d'exclusion de garantie ci-dessus en fait partie intégrante.
