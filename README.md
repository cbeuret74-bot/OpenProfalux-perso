# OpenProfalux

> Fork personnel de [Shad107/OpenProfalux](https://github.com/Shad107/OpenProfalux) avec mes adaptations pour une installation multi-volets (11+ volets). Projet et guide d'origine : https://www.isno.fr/projets/openprofalux

## Mes modifications

Adaptations apportées au firmware d'origine pour fiabiliser une installation avec un grand nombre de volets. Toutes côté `firmware/`.

### Capacité & persistance
- **Partition NVS agrandie à 80 Ko** (`partitions.csv`, contre 16 Ko d'origine) : les apps OTA sont décalées à `0x20000`, l'espace récupéré sur le SPIFFS. Permet de stocker la configuration jusqu'aux 24 volets max du firmware sans saturer la flash.
- **Configuration stockée en blob NVS** au lieu d'une chaîne (`shutters.c`) : lève le plafond des ~4000 octets de `nvs_set_str` qui empêchait de dépasser ~9 volets. Migration automatique depuis l'ancien format string, avec contrôle d'erreur explicite (fini les échecs de sauvegarde silencieux).
- **Buffer JSON `/api/status` porté à 16 Ko** (`web_ui.c`) : évite la troncature de la liste des volets dans l'UI web au-delà de ~9 volets.

### Fiabilité radio
- **Émission TX protégée par section critique** (`cc1101.c`) : chaque trame OOK est émise sans préemption par le WiFi/l'ordonnanceur, ce qui supprime les commandes perdues aléatoires. Le gap inter-trame reste hors section critique pour laisser respirer le WiFi.
- **Cible de compilation forcée sur `external`** (`sdkconfig.defaults`) : corrige le brochage (le `sdkconfig` généré partait parfois sur la cible m5stack, ce qui empêchait toute émission).

### Stabilité système
- **Vérification OTA périodique GitHub retirée** (`shutters.c`) : supprime le pic mémoire TLS qui faisait décrocher le WiFi et MQTT toutes les quelques minutes sur signal faible.
- **Pause du rafraîchissement automatique pendant l'édition** (`web/app.js`) : la page ne recharge plus le statut tant qu'un champ est en cours de saisie, ce qui rend la calibration utilisable.

### Réglages par défaut ajustés
- Gain RX par défaut : `0x2F` (gain réduit) pour limiter le décodage du bruit ambiant en écoute permanente.
- `PRESS_REPEATS = 18` trames par appui (robustesse d'émission).

---

*Note : ces modifications sont propres à mon installation. Le travail de base — capture/rejeu KeeLoq, enrôlement virtuel, UI web, intégration Home Assistant — est celui de l'auteur original.*

**Piloter ses volets roulants Profalux depuis Home Assistant avec un ESP32 à ~15 €, sans la clé constructeur.**

Firmware ESP32 + CC1101 open-source pour les volets **Profalux 868 MHz** (moteurs MAI-EMPX /
MAI-EMNOE). Tout reste local : pas de cloud, pas de passerelle propriétaire, aucun fil à tirer.

📖 **Montage pas à pas, photos et l'histoire complète du reverse** : [www.isno.fr/projets/openprofalux](https://www.isno.fr/projets/openprofalux)

## Le principe : cloner une télécommande, pas casser la crypto

Les télécommandes Profalux sont en KeeLoq (rolling code), donc une trame est a priori
impossible à rejouer. J'ai passé des jours à chasser la clé constructeur... avant de tester la
chose la plus bête, celle que j'aurais dû essayer en premier :

> **Le moteur accepte une trame rejouée.** Le compteur anti-rejeu n'est **pas** vérifié par le
> récepteur : une trame capturée puis réémise fait bouger le volet, même après avoir utilisé la
> vraie télécommande entre-temps. Donc **aucune clé n'est nécessaire.**

OpenProfalux **clone** donc une télécommande, sans crypto : on capture les trames ▲ / ■ / ▼
d'une vraie télécommande, on les nomme, et on les rejoue à la demande depuis Home Assistant.
C'est tout. La clé constructeur, explorée longuement (cf. `firmware-capture-test/` et `docs/`),
s'est révélée inutile pour le pilotage.

## Ce que fait le firmware (`firmware/`)

- **Cover Home Assistant** par volet (MQTT discovery) : ouvrir / fermer / stop / position.
- **UI web embarquée** : pilotage, apprentissage, calibration, moniteur RF, config.
- **Apprentissage** : capture ▲/▼/■ d'une vraie télécommande, nommage du volet.
- **Position** estimée par le temps (calibration montée/descente, recalage aux butées).
- **Suivi de la vraie télécommande** : ▲/▼ lance le moteur, ■ fige (modèle appui + stop).
- **Multi-télécommandes** par volet.
- **Sauvegarde / restauration** de la config (noms + trames de référence + calibration).
- **OTA** : upload web + pull MQTT + rollback (2 partitions OTA).
- **Capture de trames** optionnelle vers MQTT (dédup par serial).

→ Détails, build et flash : **[`firmware/README.md`](firmware/README.md)**.

## Matériel

| Composant | Réf | Coût |
|-----------|-----|------|
| ESP32 (M5Stack ATOM Lite ou DevKit) | ESP32-WROOM / PICO | ~5-12 € |
| Module CC1101 868 MHz | petit module vert 868 (pastilles au pas de 2 mm, pas le 2.54) | ~3 € |
| Antenne 868 MHz | fil 8.6 cm quart d'onde, ou hélicoïdale SMA | ~2 € |

Fréquence mesurée : **868.425 MHz** OOK (cf. `docs/`). **CC1101 = 3.3 V max, jamais 5 V.**

## Structure du dépôt

| Dossier | Rôle |
|---------|------|
| `firmware/` | **Firmware produit** : cover HA, UI, clone/replay, OTA, sauvegarde |
| `firmware-capture-test/` | Firmware de **banc / reverse** : capture RF, tests KeeLoq, mesures |
| `docs/` | Architecture, mesures RF, synthèses de la recherche |
| `sim/` | Harness de vérification KeeLoq (cipher + codec) côté PC |

## Statut

- ✅ Firmware produit **compile** (ESP-IDF v5.2.2, ~1.18 Mo, partitions OTA).
- ✅ **Rejeu validé au banc** (le moteur suit une trame rejouée, y compris après la vraie télécommande).
- ⏳ À valider sur l'installation : calibration des temps de course, portée antenne, multi-volets.

## Extensions possibles

Même approche (capture + rejeu) potentiellement applicable à d'autres volets 868 MHz à
rolling code non contraint (Delta Dore X2D, France Fermetures LIBRIO…), non vérifié.
