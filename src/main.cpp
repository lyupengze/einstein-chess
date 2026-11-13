#include <iostream>

#include "core/board.h"

int main() {
    core::Board board;
    board.setup_initial();

    std::cout << board.to_string() << std::endl;

    return 0;
}