#pragma once
struct PositionComponent {
    f32v3 mPosition;
    ChunkID chunkId = INVALID_CHUNK_ID;
};

// TODO: Move
struct OrientationComponent {
    glm::quat mOrientation;
};

// TODO: Move
class AngularVelocityComponent {
public:
    AngularVelocityComponent() : mAngularVelocityRadPerSec(0.0f), mCachedLength(0.0f) {}
    AngularVelocityComponent(f32v3 velocityRadPerSec) : mAngularVelocityRadPerSec(velocityRadPerSec), mCachedLength(glm::length(velocityRadPerSec)) {}

    void setAngularVelocityRadPerSec(f32v3 angularVelocityRadPerSec) {
        mAngularVelocityRadPerSec = angularVelocityRadPerSec;
        mCachedLength = glm::length(angularVelocityRadPerSec);
    }

    f32v3 getAngularVelocityRadPerSec() const {
        return mAngularVelocityRadPerSec;
    }

    f32 getAngularVelocityLength() const {
        return mCachedLength;
    }

    // Returns new rotation
    glm::quat applyToRotation(f32 elapsedSec, glm::quat rotation) {
        if (mCachedLength > 0.0f) {
            const f32 angle = glm::length(mCachedLength) * elapsedSec;
            // Normalize
            const f32v3 axis = mAngularVelocityRadPerSec / mCachedLength;

            glm::quat deltaRotation = glm::angleAxis(angle, axis);
            return glm::normalize(rotation * deltaRotation);
        }
        return rotation;
    }
private:
    f32v3 mAngularVelocityRadPerSec;
    f32 mCachedLength; // Prevent recalculating when we aren't changing
};