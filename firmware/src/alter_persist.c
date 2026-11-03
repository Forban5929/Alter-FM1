/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER: the settings kept in flash (MENU's settings, the panel's learned table, the theme: settings_persist.c), in
 * storage object 0. Felucca's projects, user presets and autosave are gone: ALTER keeps its music in its scenes and
 * patterns (alter_store.c, objects 2..4). Objects 1 and 5.. are free now. The flash copy wins at boot. */
#include "settings_persist.c"
#if FELUCCA_FLASH
static persist_t persist_saved __attribute__((section(".pool")));
static uint8_t persist_pending;                 /* 1 requested, 2 waiting after a flash error */
static uint32_t persist_retry_ms;
_Static_assert(sizeof(persist_t) <= ST_PAYLOAD_MAX, "settings do not fit one flash sector");
#endif

static void persist_boot(void)                    /* before settings_init / panel_init */
{
#if FELUCCA_FLASH
    persist_t p;
    int n;
    uint32_t f = irq_save();
    {                                              /* the known 1 MiB parts, else stay RAM-only: Puya P25Q80 (0x856014)
                                                    * and the UC25HQ80 of newer FM-1 units (0xB36014 : Felucca 7435517,
                                                    * issue #320 ; sans lui : SAVE en RAM seulement, mise à jour USB
                                                    * refusée en silence — la FM-1 de Romain, 03/11) */
        uint32_t id = FL_FAR(fl_jedec_ram)();
        flash_ok = id == 0x856014u || id == 0xB36014u;
    }
    irq_restore(f);
    if (!flash_ok)
        return;
    n = st_load(OBJ_SETTINGS, &p, sizeof p);
    if (settings_import(&p, n))
        persist_saved = p;
#endif
}

static void settings_poll(void)
{
#if FELUCCA_FLASH
    persist_t p;
    if (!persist_pending || !flash_ok || transport_busy() ||
        (persist_pending == 2u && (uint32_t)(fm1_ms - persist_retry_ms) < 1000u))
        return;
    p = persist_saved;
    settings_export(&p);
    if (!memcmp(&p, &persist_saved, sizeof p)) {
        persist_pending = 0;
        return;                                    /* unchanged: no erase cycle */
    }
    if (st_save(OBJ_SETTINGS, &p, sizeof p) == 0) {
        persist_saved = p;
        persist_pending = 0;
    } else {
        persist_pending = 2;
        persist_retry_ms = fm1_ms;
    }
#endif
}

static void settings_save(void)
{
#if FELUCCA_FLASH
    persist_pending = 1;
#endif
    settings_poll();
}
