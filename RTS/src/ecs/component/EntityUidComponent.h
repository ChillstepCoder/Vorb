#pragma once

// These should only be created when we need them.
// get_or_emplace is useful
struct EntityUidComponent {
    EntityUidComponent() : mUid(++sGenerator) { ASSERT_GAME_THREAD(); }

    EntityUid mUid;
    inline static EntityUid sGenerator = 0;
};