/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's USER sounds (v1.26, Romain 24/11: "que l'utilisateur puisse save ses propres presets d'engine"): 16 places, each
 * a circle's sound as it is (its engine, E1..E8, the envelope, LEVEL, its RUMBLE), a 4-character NAME. PRESETS turns
 * through them after the factory ones (the empty ones skipped); the SAVE page's 4th tab, SOUNDS, loads and stores them.
 * A circle's sound index (au.pre) for USER sound u is ALT_UPRE0 + u: past any factory list to come, so a scene keeps
 * pointing at the same place when factory sounds are added. On the flash in object 31 (storage.c: one of the old
 * patterns' places, unused since 10/11; another magic there = empty). Included in alter_ui.c after alter_sounds.c. */

#define ALT_NUSND 16u
#define ALT_UPRE0 200u                           /* (au.pre of USER sound 0) */
#define ALT_OBJ_SND 31u
#define ALT_MAGIC_SND 0x444E5341u                /* "ASND" */
static int st_load(uint32_t obj, void *dst, uint32_t max);   /* (storage.c, included later) */
static int st_save(uint32_t obj, const void *src, uint32_t len);

typedef struct {                                 /* one place as stored (22 bytes; only add fields at the end) */
    uint8_t used, eng;
    int8_t e[8];
    uint8_t env[4];
    uint8_t level, mode, rumble, rsv;
    char nm[4];                                  /* (space, A-Z, 0-9, -: alter_ui.c ALT_NM_CH) */
} alt_usnd_t;
typedef struct {
    uint32_t magic;
    alt_usnd_t s[ALT_NUSND];
} alt_usnd_bank_t;
_Static_assert(sizeof(alt_usnd_t) == 22u, "a USER sound's bytes stay");

static alt_usnd_bank_t aus_bank;                 /* (the flash's copy, 356 bytes) */
static alt_pre_t aus_pre[ALT_NUSND];             /* (as the factory ones, for alt_load_sound and the names shown) */
static char aus_name[ALT_NUSND][9];              /* "U03 GRIT" */

static int aus_is(uint32_t i) { return i >= ALT_UPRE0 && i < ALT_UPRE0 + ALT_NUSND; }
static int aus_used(uint32_t u) { return aus_bank.s[u % ALT_NUSND].used != 0; }

static void aus_mk(uint32_t u)                   /* place u's alt_pre_t and name from the bank */
{
    const alt_usnd_t *s = &aus_bank.s[u];
    alt_pre_t *p = &aus_pre[u];
    char *n = aus_name[u];
    uint32_t k, j = 4;
    n[0] = 'U';
    n[1] = (char)('0' + (u + 1u) / 10u);
    n[2] = (char)('0' + (u + 1u) % 10u);
    n[3] = ' ';
    for (k = 0; k < 4u; k++)
        n[j++] = s->used ? s->nm[k] : '-';
    while (j > 4u && n[j - 1u] == ' ')
        j--;
    n[j == 4u ? 3u : j] = 0;
    p->name = n;
    p->eng = (uint8_t)(s->eng % ALT_NENG);
    for (k = 0; k < 8u; k++)
        p->e[k] = s->e[k];
    for (k = 0; k < 4u; k++) {
        p->env[k] = s->env[k];
        p->fx[k] = 0;                            /* (a sound loads dry: v1.18) */
    }
    p->level = s->level;
    p->mode = s->mode;
    p->rumble = s->rumble;
    p->hide = !s->used;
}

static void aus_load(void)                       /* the bank from the flash (power-on, a backup put back) */
{
    uint32_t u, k;
    if (st_load(ALT_OBJ_SND, &aus_bank, sizeof aus_bank) != (int)sizeof aus_bank || aus_bank.magic != ALT_MAGIC_SND) {
        for (k = 0; k < sizeof aus_bank; k++)
            ((uint8_t *)&aus_bank)[k] = 0;
        aus_bank.magic = ALT_MAGIC_SND;
    }
    for (u = 0; u < ALT_NUSND; u++)
        aus_mk(u);
}

/* sound i (au.pre): a factory one or a USER one */
static const alt_pre_t *alt_pre(uint32_t i)
{
    if (aus_is(i))
        return &aus_pre[i - ALT_UPRE0];
    return &ALT_PRE[i % ALT_NPRE];
}
static uint32_t alt_pre_ok(uint32_t i) { return aus_is(i) ? i : i % ALT_NPRE; }

/* PRESETS' order: the factory ones, then the USER ones; place n of it (0 .. ALT_NPRE + 15) and back */
static uint32_t alt_pre_at(uint32_t n) { return n < ALT_NPRE ? n : ALT_UPRE0 + (n - ALT_NPRE) % ALT_NUSND; }
static uint32_t alt_pre_pos(uint32_t i) { return aus_is(i) ? ALT_NPRE + (i - ALT_UPRE0) : i % ALT_NPRE; }
#define ALT_NPRE_ALL (ALT_NPRE + ALT_NUSND)

static int32_t alt_base(uint32_t c, uint32_t p);   /* (alter_store.c) */
/* circle c's sound into USER place u, named nm (4 characters); 1 stored, 0 the flash failed */
static int aus_store(uint32_t c, uint32_t u, const char *nm)
{
    alt_usnd_t *s = &aus_bank.s[u % ALT_NUSND];
    const track_t *t = &trk[c % AS_NCIRC];
    uint32_t k;
    s->used = 1;
    s->eng = 0;
    for (k = 0; k < ALT_NENG; k++)
        if (t->eng_req == ALT_ENGS[k])
            s->eng = (uint8_t)k;
    for (k = 0; k < 8u; k++)
        s->e[k] = (int8_t)clamp(alt_base(c, P_E0 + k), -128, 127);
    s->env[0] = (uint8_t)clamp(alt_base(c, P_ATK), 0, 127);
    s->env[1] = (uint8_t)clamp(alt_base(c, P_DEC), 0, 127);
    s->env[2] = (uint8_t)clamp(alt_base(c, P_SUS), 0, 127);
    s->env[3] = (uint8_t)clamp(alt_base(c, P_REL), 0, 127);
    s->level = (uint8_t)clamp(alt_base(c, P_LEVEL), 0, 127);
    s->mode = alt_emode[c % AS_NCIRC];
    s->rumble = (uint8_t)(alt_pre(au.pre[c % AS_NCIRC])->rumble ? (alt_rmb[c % AS_NCIRC] ? alt_rmb[c % AS_NCIRC] : 1u) : 0u);
    s->rsv = 0;
    for (k = 0; k < 4u; k++)
        s->nm[k] = nm[k] ? nm[k] : ' ';
    aus_mk(u % ALT_NUSND);
    return st_save(ALT_OBJ_SND, &aus_bank, sizeof aus_bank) == 0;
}
