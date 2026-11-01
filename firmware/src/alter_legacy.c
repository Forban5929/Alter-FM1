/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER: what the old Felucca pages still name of the engines ALTER took out (DRUM, FM6, SAMPLE, GRAIN, SLICE ..).
 * Only names and empty answers: none of it plays. Goes with those pages (step 3c: the DEEP view replaces them). */

/* DRUM's lanes as the old sequencer grid names them (General MIDI) */
#define NLANE_NOTE {36, 38, 39, 42, 46, 45, 37, 56}
static const uint8_t DRUM_LANE_NOTE[NLANE] = NLANE_NOTE;
static uint8_t drum_mute[NPART];
static uint32_t drum_lane(uint32_t note)
{
    uint32_t l;
    for (l = 0; l < NLANE; l++)
        if (DRUM_LANE_NOTE[l] == note)
            return l;
    return 0;
}
static const char *drum_lane_name(const track_t *t, uint32_t l)
{
    static const char *const N[NLANE] = {"KICK", "SNARE", "CLAP", "HATCL", "HATOP", "TOM", "RIM", "BELL"};
    (void)t;
    return N[l & (NLANE - 1u)];
}
static const char *drum_lane_abbr(const track_t *t, uint32_t l)
{
    static const char *const N[NLANE] = {"BD", "SD", "CP", "CH", "OH", "TM", "RS", "CB"};
    (void)t;
    return N[l & (NLANE - 1u)];
}
static uint32_t step_lanes(const step_t *s)
{
    uint32_t m = 0, i;
    if (s->time != ST_NOTE)
        return 0;
    for (i = 0; i < s->n && i < 4u; i++)
        if (s->note[i] == DRUM_LANE_NOTE[drum_lane(s->note[i])])
            m |= 1u << drum_lane(s->note[i]);
    return m | s->hit;
}
static uint32_t step_accents(const step_t *s) { return s->flags & SF_ACCENT ? step_lanes(s) : s->acc & step_lanes(s); }
static void step_to_grid(step_t *s) { (void)s; }
static voice_t *drum_reuse(track_t *t, uint32_t note) { (void)t; (void)note; return 0; }
enum { DV_KICK, DV_SNARE, DV_CLAP, DV_HATC, DV_HATO, DV_TOM, DV_RIM, DV_BELL };   /* the lanes' order */

/* Felucca's user presets (upreset.c) are gone: ALTER keeps its sounds in its scenes. The old pages see none */
static int up_used(uint32_t k) { (void)k; return 0; }
static uint32_t up_count(void) { return 0; }
static uint32_t up_nth(uint32_t n) { (void)n; return 0; }
static uint32_t up_rank(uint32_t slot) { (void)slot; return 0; }
static uint32_t up_cat(uint32_t k) { (void)k; return 0; }
static uint32_t up_engine(uint32_t k) { (void)k; return ENGI_ALTER; }
static int up_load(uint32_t k) { (void)k; return 0; }
static void up_name(uint32_t k, char *b) { (void)k; b[0] = 0; }
static void up_slot_label(char *b, uint32_t k) { (void)k; b[0] = 0; }
static void up_auto_name(char *b, uint32_t e, uint32_t k) { (void)e; (void)k; b[0] = 0; }
static int up_has_pat(uint32_t k) { (void)k; return 0; }
static uint32_t up_pat_count(void) { return 0; }
static uint32_t up_pat_nth(uint32_t n) { (void)n; return 0; }
static uint32_t up_pat_rank(uint32_t slot) { (void)slot; return 0; }
static void up_pat_load(track_t *t, uint32_t k) { (void)t; (void)k; }
static void up_ui_named(uint32_t op, uint32_t k, const char *name, uint32_t cat) { (void)op; (void)k; (void)name; (void)cat; }
static void up_ui(uint32_t op, uint32_t k) { (void)op; (void)k; }
static void up_boot(void) {}

/* Felucca's projects (project.c) are gone: the old PROJECT pages see four empty slots */
#define PROJ_NAME_LEN 12u
static int project_used(uint32_t slot) { (void)slot; return 0; }
static int project_info(uint32_t slot, char *b, uint16_t *len48, uint32_t *den, uint8_t *n)
{
    (void)slot; (void)len48; (void)den; (void)n;
    if (b)
        b[0] = 0;
    return 0;
}
static int project_name(uint32_t slot, char *b) { return project_info(slot, b, 0, 0, 0); }
static void project_cur_name(char *b) { b[0] = 0; }
static int project_save_as(uint32_t slot, const char *name) { (void)slot; (void)name; return -1; }
static int project_save(uint32_t slot) { return project_save_as(slot, 0); }
static int project_rename(uint32_t slot, const char *name) { (void)slot; (void)name; return -1; }
static int project_erase(uint32_t slot) { (void)slot; return -1; }
static void project_load(uint32_t slot) { (void)slot; }

/* Felucca's song chain played projects one after the other: with no projects it never starts (song_chain.c and the
 * old sequencer go with the old pages, step 3c) */
static uint32_t chain_prepare(void) { return 1; }
