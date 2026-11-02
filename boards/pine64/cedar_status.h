#ifndef JUPITER_PINE64_CEDAR_STATUS_H
#define JUPITER_PINE64_CEDAR_STATUS_H

#include "demos/support.h"

const char *pine64_cedar_encode_path(void);

static void pine64_cedar_draw_status(uint32_t *overlay, int succeeded)
{
    pine_demo_fill_rect(overlay, 0, LCD_H - 25, LCD_W, 25, 0xe0182030u);
    pine_demo_draw_text(overlay, pine64_cedar_encode_path(),
                        6, LCD_H - 21, 1, 0xffffffffu);
    pine_demo_draw_text(overlay, succeeded ? "H.264 ROUND TRIP COMPLETE" :
                                            "PIPELINE FAILED - RAW SPRITES",
                        6, LCD_H - 10, 1,
                        succeeded ? 0xff80efb0u : 0xffffc060u);
}

#endif
