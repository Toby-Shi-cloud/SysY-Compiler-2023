#include <string_view>
using namespace std::literals;

#include "header/dbg.h"

int main() {
    dbg("Hello, Compiler!"sv);
    return 0;
}
