
#ifdef _WIN32
#include "win_terminal.h"
#else
#include "unix_terminal.h"
#endif
#include <memory>
namespace bbs {
std::unique_ptr<Terminal> create_local_terminal() {
    return std::make_unique<LocalTerminal>();
}

}