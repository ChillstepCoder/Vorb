#pragma once

namespace vorb {
    namespace graphics {
        struct ProgramError {
            ProgramError(nString m, nString c) : message(std::move(m)), code(std::move(c)) {}

            nString message;
            nString code;
        };
    }
}