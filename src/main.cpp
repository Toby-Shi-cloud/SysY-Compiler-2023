#include <string_view>
using namespace std::literals;

#include "dbg.h"

int main() {
    dbg("Hello, Compiler!"sv);
    return 0;
}
