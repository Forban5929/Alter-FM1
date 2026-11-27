// SPDX-License-Identifier: GPL-3.0-only
// Relais « Feedback » d'ALTER (v1.1, 22/11) : la page compagnon envoie un retour ici (sans compte),
// ce relais crée une Issue sur github.com/Forban5929/alter-fm1 avec l'étiquette « a-trier ».
// Le jeton GitHub (GITHUB_TOKEN : fine-grained, Issues read/write sur alter-fm1 SEULEMENT) vit dans les variables
// d'environnement Vercel, jamais dans la page. Déployé sur Vercel (projet alter-feedback, équipe rominou1).

const REPO = 'Forban5929/alter-fm1';
const ORIGINS = ['https://forban5929.github.io', 'http://localhost:8765'];
const CATS = { sound: 'son', bug: 'bug', idea: 'idée', ui: 'page/écran', other: 'autre' };
const hits = new Map();            // anti-abus simple : 20 envois / 10 min par adresse (mémoire de l'instance ; 5 avant
                                   // le 28/11 : trop peu quand Romain propose plusieurs sons d'usine à la suite)

function cors(req, res) {
  const o = req.headers.origin || '';
  if (ORIGINS.includes(o)) res.setHeader('Access-Control-Allow-Origin', o);
  res.setHeader('Vary', 'Origin');
  res.setHeader('Access-Control-Allow-Methods', 'POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');
  return ORIGINS.includes(o);
}
const clip = (v, n) => String(v == null ? '' : v).replace(/\r/g, '').slice(0, n).trim();
const esc = (s) => s.replace(/[<>]/g, (c) => (c === '<' ? '&lt;' : '&gt;')).replace(/@/g, '@​');

module.exports = async (req, res) => {
  const okOrigin = cors(req, res);
  if (req.method === 'OPTIONS') return res.status(204).end();
  if (req.method === 'GET') return res.status(200).json({ ok: true, service: 'alter-feedback', configured: !!process.env.GITHUB_TOKEN });
  if (req.method !== 'POST') return res.status(405).json({ ok: false, error: 'method' });
  if (!okOrigin) return res.status(403).json({ ok: false, error: 'origin' });
  if (!process.env.GITHUB_TOKEN) return res.status(503).json({ ok: false, error: 'not configured' });

  let b = req.body;
  if (typeof b === 'string') { try { b = JSON.parse(b); } catch { b = {}; } }
  b = b || {};
  if (b.website) return res.status(200).json({ ok: true });          // champ piège rempli = robot : on ignore

  const ip = String(req.headers['x-forwarded-for'] || '').split(',')[0].trim() || 'x';
  const now = Date.now(), list = (hits.get(ip) || []).filter((t) => now - t < 600000);
  if (list.length >= 20) return res.status(429).json({ ok: false, error: 'too many' });
  list.push(now); hits.set(ip, list);

  const text = clip(b.text, 4000);
  if (text.length < 5) return res.status(400).json({ ok: false, error: 'empty' });
  const cat = CATS[b.cat] ? b.cat : 'other';
  const ver = clip(b.version, 40) || '?';
  const site = b.site === 'beta' ? 'beta' : 'public';
  const title = `[${CATS[cat]}] ${clip(text.split('\n')[0], 70)}`;
  const body = [
    esc(text), '', '---',
    `- Catégorie : ${CATS[cat]}`,
    `- Version : ${esc(ver)} (site ${site})`,
    `- Connexion : ${esc(clip(b.link, 40) || '?')}`,
    `- Moteurs des cercles : ${esc(clip(b.engines, 80) || '?')}`,
    `- Navigateur : ${esc(clip(b.ua, 200) || '?')}`,
    `- Contact (facultatif) : ${esc(clip(b.contact, 120) || '—')}`,
    '', '_Envoyé depuis le bouton Feedback de la page compagnon._',
  ].join('\n');

  const r = await fetch(`https://api.github.com/repos/${REPO}/issues`, {
    method: 'POST',
    headers: {
      Authorization: `Bearer ${process.env.GITHUB_TOKEN}`,
      Accept: 'application/vnd.github+json',
      'User-Agent': 'alter-feedback',
      'X-GitHub-Api-Version': '2022-11-28',
    },
    body: JSON.stringify({ title, body, labels: ['a-trier', 'feedback', `cat-${cat}`, `v-${ver.replace(/[^\w.]/g, '')}`.slice(0, 40)] }),
  });
  if (!r.ok) return res.status(502).json({ ok: false, error: 'github ' + r.status });
  const j = await r.json();
  return res.status(200).json({ ok: true, n: j.number });
};
