#pragma once
struct PositionComponent {
    f32v3 mPosition;
    ChunkID chunkId = INVALID_CHUNK_ID;
};

// TODO: Move
struct OrientationComponent {
    glm::quat mOrientation;
};
