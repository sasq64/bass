
#include "localterminal.h"
#include <memory>
namespace bbs {
std::unique_ptr<Terminal> create_local_terminal() {
    return std::make_unique<LocalTerminal>();
}

}