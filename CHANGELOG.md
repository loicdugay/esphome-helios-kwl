# Changelog

Toutes les modifications notables de ce fork sont documentées ici.

## [v1.1.0] — 2026-07 — Fiabilité RS485, interface tactile et dépendances

### Corrections
- **Lectures robustes** : `read_register` extrait la réponse du flux via le parseur de paquets standard — un broadcast 12 s intercalé ne fait plus échouer la lecture (cause principale des « pas de reponse apres 3 tentatives »)
- **Poll S3 en échec** : re-tentative en 30 s au lieu d'attendre l'intervalle complet (1 h)
- **Rollover `millis()`** : comparaisons par soustraction non signée (uptime > 49 jours)
- **CO₂** : publication uniquement à la réception de l'octet bas (évite de mélanger deux cycles de broadcast H/L)
- **Relais défaut** : sémantique vérifiée sur EC 300 Pro — le relais est fermé en fonctionnement normal (logique fail-safe) et s'ouvre sur défaut, le capteur est donc ON quand le contact s'ouvre
- **Options IRAM** : les `-DCONFIG_*` dans `platformio_options` étaient silencieusement ignorés — remplacés par de vraies `sdkconfig_options` ESP-IDF
- **Rétroéclairage en veille** : `zero_means_zero` sur la sortie LEDC — avec `min_power: 15%`, « éteint » sortait en réalité 15 % de PWM

### Ajouts
- **Vérification de chaque écriture** : `write_register` journalise l'ordre envoyé (`[CMD]`) puis force la relecture du registre au poll suivant et journalise la réponse de la carte mère (`[CM] ... ecriture confirmee`, ou avertissement si la valeur diffère)
- **Polling de secours des températures** (60 s) : les registres 0x32–0x35 n'arrivaient que par le broadcast du maître, qui s'arrête quand la VMC est éteinte
- **Tables de polling conditionnelles** : un registre n'est pollé que si une entité YAML consomme sa valeur (supprime 5 polls morts : 0x2D, 0x55, 0x56, 0x6F, 0x70)
- **Démarrage accéléré** : un poll de configuration supplémentaire par seconde pendant 2 minutes après le boot — tous les réglages remontent dans HA en ~20 s au lieu de ~90 s
- **Interface tactile complète** (T-Panel S3, page unique 480×480, zéro scroll) : arc de vitesse à curseur, pastille de mode Normal/Air frais, pastilles de régulation HR%/CO2, grille de mesures avec échelles de couleurs de confort (ADEME/ARS), cycles avec compte à rebours et bouton Annuler, reset filtres à double confirmation (30 s), voile d'extinction
- **UI optimiste** : l'état demandé s'affiche immédiatement, la relecture de la carte mère réconcilie en 1-2 s
- **Veille anti-marquage LCD** : après 5 min sans toucher, frame noire + rétroéclairage coupé + rendu LVGL en pause, réveil au toucher absorbé
- **Intégration continue** : validation automatique de `vmc.yaml` à chaque push/PR (GitHub Actions)

### Modifications
- **Tactile** : migration vers le composant natif ESPHome `cst226` (même protocole Hynitron que le CST3240) — suppression du composant externe `cst3240`
- **Polices** : Montserrat téléchargée à la compilation depuis Google Fonts (glyphes limités au nécessaire + accents français), symboles natifs LVGL pour les icônes — plus aucun fichier de police local
- **Entités CO₂** : fournies commentées dans `vmc.yaml` (à décommenter si la sonde est installée) ; le switch de régulation CO₂ reste actif car il pilote un bit de la carte mère
- **Performances** : CPU à 240 MHz, compilation optimisée vitesse (`-O2`), buffer LVGL plein écran
- **Dépendances** : suppression de `captive_portal`, `external_components` restreint à `helios_kwl`, logger en niveau INFO
- **Images du dépôt** : sorties de Git LFS (le quota de bande passante aurait fini par casser l'affichage du README)

## [Antérieur à v1.1.0] — historique du fork

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
