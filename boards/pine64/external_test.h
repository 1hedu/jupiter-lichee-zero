#ifndef PINE64_EXTERNAL_TEST_H
#define PINE64_EXTERNAL_TEST_H
/* A host-only electrical model observes the same MMIO writes as the board.
 * The production build has no extra calls or alternate protocol path. */
#ifdef PINE64_HOST_TEST
void pine64_external_gpio_changed(void);
#define PINE_EXTERNAL_GPIO_CHANGED() pine64_external_gpio_changed()
#else
#define PINE_EXTERNAL_GPIO_CHANGED() ((void)0)
#endif
#endif
