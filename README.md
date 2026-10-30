# BeatDrop

**Publie tes type beats sur YouTube directement depuis ton DAW.**
Plugin VST3 / AU à mettre en dernier sur ta chaîne master : il capture ton beat, génère la vidéo
(ton image + l'audio) et l'envoie sur ta chaîne avec le titre, la description et les tags.

Inspiré du concept de [Konnect](https://kiddynaconnect.com/plugin), en version open source et sans serveur :
le plugin parle directement à l'API YouTube avec ton propre compte Google.

## Fonctionnalités

- **Deux modes de capture**
  - **Lecture** : arme BeatDrop, lance la lecture depuis le début, stoppe. La prise est enregistrée.
  - **Export DAW** : arme BeatDrop, lance l'export audio de ton DAW. BeatDrop capte le rendu offline.
  - Le silence au début est supprimé, la fin est coupée 1 s après le dernier son.
  - Ou bien **importe un WAV/AIFF/MP3** existant.
- **Image de la vidéo** par glisser-déposer (PNG/JPG). Les images non 16:9 sont centrées sur un fond flouté.
- **Vidéo MP4** 1080p H.264 + AAC 256 kb/s générée en local (AVFoundation, aucune dépendance).
- **Titre / description / tags** générés depuis des modèles :
  `[FREE] {artist} Type Beat - "{name}"` → `[FREE] Drake Type Beat - "Midnight"`.
  Variables : `{name} {artist} {artisttag} {bpm} {key} {producer} {year}`. Le BPM est repris du DAW.
- **Visibilité** : public, non répertorié, privé ou **programmé** (date/heure).
- **Upload reprenable** (reprend après une coupure réseau), progression, annulation, lien vers la vidéo.
- **Miniature** envoyée automatiquement (si ta chaîne est vérifiée).
- Session Google stockée dans le **Trousseau macOS**. Les champs sont sauvegardés avec ton projet DAW.
- Tout est gardé en local dans `~/Music/BeatDrop/` (`Takes/` pour les WAV, `Videos/` pour les MP4).

## Installation

### Depuis les builds GitHub Actions
Onglet **Actions** → dernier build vert → artefact `BeatDrop-macOS`. Copie :
- `BeatDrop.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
- `BeatDrop.component` → `~/Library/Audio/Plug-Ins/Components/`

Les builds ne sont pas notarisés par Apple : la première fois, lance
```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/BeatDrop.vst3 ~/Library/Audio/Plug-Ins/Components/BeatDrop.component
```

### Depuis les sources
Prérequis : macOS 11+, Command Line Tools (`xcode-select --install`), CMake ≥ 3.22, Ninja.
```bash
git clone --recursive https://github.com/RecklessBoise/BeatDrop.git
cd BeatDrop
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBEATDROP_COPY_AFTER_BUILD=ON
cmake --build build
```
Le plugin est copié dans `~/Library/Audio/Plug-Ins`. Rescanne tes plugins dans ton DAW.

## Configuration (une seule fois)
Suis le guide **[docs/GOOGLE_SETUP.md](docs/GOOGLE_SETUP.md)** : création d'un client OAuth Google
(gratuit, ~5 min), import du `client_secret.json` dans *Réglages*, puis **Connecter YouTube**.

## Utilisation dans FL Studio

1. Mixer → piste **Master** → dernier slot → **BeatDrop**.
2. Remplis le **nom du beat** et l'**artiste**. Le titre, la description et les tags se remplissent tout seuls (modifiables).
3. Glisse ton **image**.
4. Capture :
   - **Lecture** : clique **ARMER**, passe en mode Song, lance depuis le début, stoppe à la fin.
   - **Export DAW** : clique **ARMER**, puis *Fichier → Exporter → WAV* (effets du master inclus).
5. **Publier sur YouTube**. Rendu vidéo, upload, miniature : le lien apparaît à la fin.

L'upload continue même si tu fermes la fenêtre du plugin.

## Limites connues
- **macOS uniquement** pour l'instant (rendu vidéo AVFoundation + Trousseau). Windows prévu
  (Media Foundation + Credential Manager), les points d'extension sont en place.
- Les vidéos peuvent être **forcées en privé** tant que ton projet Google n'a pas passé l'audit API YouTube
  (voir le guide).
- Le mode **Export DAW** dépend du fait que le DAW signale le rendu offline au plugin.
  Si ton DAW ne le fait pas, utilise le mode Lecture.

## Développement
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target BeatDropTests && ./build/BeatDropTests_artefacts/Debug/BeatDropTests
```
| Dossier | Rôle |
|---|---|
| `Source/Core` | Capture du master (`MasterRecorder`), modèles de titre, réglages, Trousseau |
| `Source/Video` | Image 16:9, miniature, encodeur MP4 |
| `Source/YouTube` | OAuth (loopback + PKCE), API YouTube (upload reprenable), job d'upload |
| `Source/UI`, `Source/Plugin` | Interface JUCE et processeur audio |
| `Tests` | Tests unitaires (sans réseau, sans Trousseau) |

## Licence
[AGPL-3.0](LICENSE), comme [JUCE](https://juce.com) qui est utilisé sous cette licence.
