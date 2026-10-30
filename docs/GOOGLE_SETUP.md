# Connecter BeatDrop à ta chaîne YouTube

BeatDrop parle directement à l'API YouTube, sans serveur intermédiaire. Pour ça, Google exige
que tu crées ton propre « client OAuth » (gratuit, ~5 minutes, à faire une seule fois).

## 1. Créer le projet Google Cloud

1. Va sur <https://console.cloud.google.com/> avec le compte Google de ta chaîne.
2. En haut, sélecteur de projet → **Nouveau projet** → nom : `BeatDrop` → **Créer**.

## 2. Activer l'API YouTube

1. Menu ☰ → **API et services** → **Bibliothèque**.
2. Cherche **YouTube Data API v3** → **Activer**.

## 3. Écran de consentement

1. Menu ☰ → **API et services** → **Écran de consentement OAuth** (ou *Google Auth Platform*).
2. Type d'utilisateur : **Externe**. Nom de l'application : `BeatDrop`, e-mail d'assistance : le tien.
3. Dans **Audience / Utilisateurs test**, ajoute **ton adresse Gmail**.
4. Conseillé : clique sur **Publier l'application** (passer en « En production »).
   En mode « Test », Google fait expirer la connexion tous les 7 jours.
   En production non vérifiée, Google affiche « Google n'a pas validé cette application » lors de la
   connexion : clique sur **Paramètres avancés → Accéder à BeatDrop (non sécurisé)**. C'est normal :
   c'est *ton* application, utilisée par toi seul.

## 4. Créer le client OAuth

1. **API et services** → **Identifiants** (ou *Clients*) → **Créer des identifiants** → **ID client OAuth**.
2. Type d'application : **Application de bureau** (Desktop app). Nom : `BeatDrop`.
3. **Télécharger le JSON** (`client_secret_xxxx.json`).

> ⚠️ Ne publie jamais ce fichier (ni sur GitHub, ni ailleurs).

## 5. Dans BeatDrop

1. Ouvre le plugin → **Réglages** → **Importer client_secret.json...** → choisis le fichier téléchargé.
2. Ferme les réglages → **Connecter YouTube** → ton navigateur s'ouvre → choisis ta chaîne → **Autoriser**.
3. Le nom de ta chaîne apparaît en vert en haut du plugin. C'est prêt.

La session est stockée dans le **Trousseau macOS** (`com.recklessboise.beatdrop`). « Déconnecter » l'efface.
Tu peux aussi révoquer l'accès à tout moment sur <https://myaccount.google.com/permissions>.

## Limites imposées par Google (important)

- **Vidéos bloquées en privé** : les projets API créés après juillet 2020 qui n'ont pas passé
  l'*audit API YouTube* voient leurs uploads forcés en **privé**. Solutions :
  - passer la vidéo en public d'un clic dans YouTube Studio ;
  - ou demander l'audit (gratuit, quelques semaines) :
    <https://support.google.com/youtube/contact/yt_api_form>.
- **Quota** : 10 000 unités / jour par projet. Un upload coûte beaucoup d'unités : compte quelques
  uploads par jour. Le compteur se remet à zéro à minuit (heure du Pacifique).
- **Miniature personnalisée** : nécessite une chaîne vérifiée par téléphone
  (<https://www.youtube.com/verify>). Sans ça, l'upload marche quand même, sans miniature.
