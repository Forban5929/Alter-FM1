# Relais « Feedback » d'ALTER

Page compagnon (`web/alter/feedback.js`, bouton **Feedback**) → `https://alter-feedback.vercel.app/api/feedback`
→ Issue GitHub sur Forban5929/alter-fm1, étiquettes `a-trier`, `feedback`, `cat-…`, `v-…`.

- Hébergé sur Vercel : projet **alter-feedback**, équipe **rominou1** (déployé depuis ce dossier par Claude, via l'outil Vercel).
- Variable d'environnement **`GITHUB_TOKEN`** (type « sensitive », Production) : jeton GitHub *fine-grained*, dépôt
  `alter-fm1` seul, permission **Issues : Read and write**. Créé et collé par Romain ; jamais dans le dépôt ni dans la page.
  Après l'avoir ajouté ou changé : **redéployer** (les variables ne sont lues qu'au déploiement).
- `GET /api/feedback` → `{"ok":true,"configured":true}` quand le jeton est en place.
- Origines acceptées : `https://forban5929.github.io` (+ `http://localhost:8765` pour les tests). Anti-abus : champ piège,
  5 envois / 10 min par adresse, texte 5-4000 caractères. Les `@` sont neutralisés (pas de notification GitHub).
- L'alias `alter-feedback-rominou1.vercel.app` est protégé par la connexion Vercel : utiliser `alter-feedback.vercel.app`.
