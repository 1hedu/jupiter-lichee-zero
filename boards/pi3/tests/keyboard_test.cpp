#include "../app/keyboard_input.h"
#include <cassert>
#include <cstdio>
int main(){
 assert(mercury_keyboard_key(nullptr)==0);
 assert(mercury_keyboard_key("")==0);
 assert(mercury_keyboard_key("c")=='c');
 assert(mercury_keyboard_key("\r")=='\r');
 assert(mercury_keyboard_key("\x1b")==0);
 assert(mercury_keyboard_key("\x1b[A")==1);
 assert(mercury_keyboard_key("\x1b[D")==1);
 assert(mercury_keyboard_key("\x1b[B")==2);
 assert(mercury_keyboard_key("\x1b[C")==2);
 assert(mercury_keyboard_key("\x1b[5~")==0);
 assert(mercury_keyboard_key("\x1bOP")==0);
 assert(mercury_keyboard_key("\x1b[")==0);
 puts("keyboard: arrows navigate; escape/special keys cannot reboot: PASS");
}
