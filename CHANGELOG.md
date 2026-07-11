# Changelog

Toutes les modifications notables de ce fork sont documentées ici.

## [Revue 2026-07] — Fiabilité RS485, mémoire et dépendances

### Corrections
- **Lectures robustes** : `read_register` extrait la réponse du flux via le parseur de paquets standard — un broadcast 12 s intercalé ne fait plus échouer la lecture (cause principale des « pas de reponse apres 3 tentatives »)
- **Poll S3 en échec** : re-tentative en 30 s au lieu d'attendre l'intervalle complet (1 h)
- **Vérification après écriture** : chaque `write_register` force la relecture du registre au prochain `update()` — HA reflète la valeur réellement prise en compte par la VMC
- **Relais défaut** : inversion corrigée (registre 0x08 bit 2 : 0 = ouvert, 1 = fermé sur défaut)
- **Rollover `millis()`** : comparaisons par soustraction non signée (uptime > 49 jours)
- **CO₂** : publication uniquement à la réception de l'octet bas (évite de mélanger deux cycles de broadcast H/L)
- **Options IRAM** : les `-DCONFIG_*` dans `platformio_options` étaient silencieusement ignorés — remplacés par de vraies `sdkconfig_options` ESP-IDF

### Ajouts
- **Polling de secours des températures** (60 s) : les registres 0x32–0x35 n'arrivaient que par le broadcast du maître, qui s'arrête quand la VMC est éteinte
- **Tables de polling conditionnelles** : un registre n'est pollé que si une entité YAML consomme sa valeur (supprime 5 polls morts : 0x2D, 0x55, 0x56, 0x6F, 0x70)

### Modifications
- **Tactile** : migration vers le composant natif ESPHome `cst226` (même protocole Hynitron que le CST3240) — suppression du composant externe `cst3240`
- **Polices** : téléchargées à la compilation depuis les sources officielles open source (Google Fonts, dépôt Templarian MDI) — plus de fichiers locaux ; police 84 px limitée aux chiffres
- **Entités CO₂ retirées du YAML** (aucun capteur installé sur cette VMC)
- **Dépendances** : suppression de `captive_portal`, `external_components` restreint à `helios_kwl` avec `refresh: 1d`, buffer LVGL 25 %, logger en niveau INFO

## [Unreleased] — Corrections et améliorations

### Corrections
- **Polling rotatif** : un seul registre par cycle `update()` au lieu de tous en séquence (résout le blocage de 400ms)
- **Boucle flush bornée** : `flush_read_buffer()` a maintenant un timeout de sécurité de 200ms
- **Humidité clampée** : les valeurs brutes < 0x33 retournent 0% au lieu d'un nombre négatif
- **Séquence d'écriture RS485** : envoi des 3 messages requis par le protocole Helios (broadcast 0x20, broadcast 0x10, direct 0x11 + checksum doublé)

### Ajouts
- Alarme CO2 élevé (registre 0x6D bit 6) — `binary_sensor`
- Risque gel échangeur (registre 0x6D bit 7) — `binary_sensor`

## [humiditysensors] — Fork loicdugay

### Ajouts
- Support des sondes d'humidité KWL-FF (registres 0x2F et 0x30)
- Sous-plateforme `sensor` pour `humidity1` et `humidity2`

### Modifications
- Adresse du composant changée de 0x21 à 0x2F (évite le conflit avec la télécommande physique)
- Polling séquentiel de tous les registres dans `update()` (avec `delay(20)` entre chaque)
- Mise à jour de l'état du switch `winter_mode` depuis la VMC

## [Original] — Cyril Jaquier (lostcontrol)

### Fonctionnalités initiales
- Lecture des 4 sondes de température via table NTC
- Lecture de la vitesse ventilateur (registre 0x29, codage masque bits)
- Lecture des états : alimentation, bypass, chauffage, défaut, maintenance (registre 0xA3)
- Écriture de la vitesse ventilateur
- Switch mode hiver (registre 0xA3 bit 3)
- Output float pour intégration avec le composant `fan:` ESPHome
