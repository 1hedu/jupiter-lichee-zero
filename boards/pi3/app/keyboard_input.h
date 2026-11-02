// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Circle supplies complete strings. Never interpret an escape prefix alone.
inline char mercury_keyboard_key(const char *s) {
 if(!s || !s[0])return 0;
 if(s[0]!=27)return s[1]?0:s[0];
 if(s[1]=='[' && s[2] && !s[3]) {
  if(s[2]=='A'||s[2]=='D')return 1;
  if(s[2]=='B'||s[2]=='C')return 2;
 }
 return 0; // Escape and unsupported special keys cannot reboot the board.
}
