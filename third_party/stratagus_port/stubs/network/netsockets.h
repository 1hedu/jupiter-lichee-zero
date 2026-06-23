/* Net sockets stub — networking is skipped in the Jupiter port. */
#ifndef JUPITER_NETSOCKETS_STUB_H
#define JUPITER_NETSOCKETS_STUB_H
#include <stdint.h>
#include <string>
class CHost { public: CHost() = default; };
class CTCPSocket { public: CTCPSocket() = default; bool IsValid() const { return false; } };
class CUDPSocket { public: CUDPSocket() = default; bool IsValid() const { return false; } };
#endif
