#pragma once

namespace vorb {
    namespace ui {

        /*! @brief Keeps track of time for an application.
         */
        struct GameTime {
        public:
            f64 totalSec; ///< Total time since the update/draw loop started.
            f64 elapsedSec; ///< Elapsed time of the previous frame.
            f32 deltaTime;
        };
    }
}
namespace vui = vorb::ui;