#include "stdafx.h"
#include "PhysicsDebugDrawer.h"

#include "debugging/DebugRenderer.h"

const int STATIC_DEBUG_ID = 95326326;
const int STATIC_DEBUG_LIFETIME = INT32_MAX;

void PhysicsDebugDrawer::drawLine(const btVector3& from, const btVector3& to, const btVector3& color) {
    if (mIsStaticMode) {
        DebugRenderer::drawLine(btVector3ToF32v3(from), btVector3ToF32v3(to - from), color4(color.x(), color.y(), color.z(), 1.0f), STATIC_DEBUG_LIFETIME, STATIC_DEBUG_ID);
    }
    else {
        DebugRenderer::drawLine(btVector3ToF32v3(from), btVector3ToF32v3(to - from), color4(color.x(), color.y(), color.z(), 1.0f));
    }
}

void PhysicsDebugDrawer::drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color) {
    f32v3 pos = btVector3ToF32v3(PointOnB);
    f32v3 normal = btVector3ToF32v3(normalOnB);
    const int adjustedLifetime = lifeTime / 10;
    DebugRenderer::drawWireQuad(pos - f32v3(0.1f, 0.1f, 0.0f), f32v2(0.2f), color4(0.0f, 1.0f, 1.0f), adjustedLifetime);
    DebugRenderer::drawLine(pos, normal * (f32)distance, color4(0.0f, 1.0f, 1.0f), adjustedLifetime);
}

void PhysicsDebugDrawer::reportErrorWarning(const char* warningString) {
    pError(warningString);
}

void PhysicsDebugDrawer::draw3dText(const btVector3& location, const char* textString) {
    throw std::logic_error("The method or operation is not implemented.");
}

void PhysicsDebugDrawer::setDebugMode(int debugMode) {
    mDebugDrawMode = (DebugDrawModes)debugMode;
}

int PhysicsDebugDrawer::getDebugMode() const {
    return mDebugDrawMode;
}

void PhysicsDebugDrawer::drawTriangle(const btVector3& v0, const btVector3& v1, const btVector3& v2, const btVector3& color, btScalar /*alpha*/) {
    const f32v3 v0f = btVector3ToF32v3(v0);
    const f32v3 v1f = btVector3ToF32v3(v1);
    const f32v3 v2f = btVector3ToF32v3(v2);
    if (mIsStaticMode) {
        const color4 colorf(color.x(), color.y(), color.z(), 0.6f);
        DebugRenderer::drawWireTriangle(v0f, v1f, v2f, colorf, STATIC_DEBUG_LIFETIME, STATIC_DEBUG_ID);
    }
    else {
        const color4 colorf(color.x(), color.y(), color.z(), 0.8f);
        DebugRenderer::drawWireTriangle(v0f, v1f, v2f, colorf);
    }
}

void PhysicsDebugDrawer::reserveStaticLines(ui32 lineCount) {
    DebugRenderer::reserveLines(lineCount, STATIC_DEBUG_LIFETIME, STATIC_DEBUG_ID);
}

void PhysicsDebugDrawer::clearStaticLines() {
    DebugRenderer::clearAllMeshesWithId(STATIC_DEBUG_ID);
}
