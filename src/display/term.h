// SPDX-License-Identifier: GPL-2.0
/*
 * xe_top - Terminal control abstraction layer
 */
#ifndef TERM_H
#define TERM_H

#include <termios.h>

void term_init(void);
void term_cleanup(void);
void term_gotoxy(int row, int col);
void term_get_size(int *cols, int *rows);
struct termios *term_orig_attr(void);

#endif /* TERM_H */