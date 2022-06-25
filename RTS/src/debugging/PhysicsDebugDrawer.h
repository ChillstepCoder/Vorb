#pragma once
#include "LinearMath/btIDebugDraw.h"


class PhysicsDebugDrawer : public btIDebugDraw
{
public:
	void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override;
	void drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color) override;
	void reportErrorWarning(const char* warningString) override;
	void draw3dText(const btVector3& location, const char* textString) override;
	void setDebugMode(int debugMode) override;
	int getDebugMode() const override;

	void drawTriangle(const btVector3& v0, const btVector3& v1, const btVector3& v2, const btVector3& color, btScalar /*alpha*/) override;

	void setIsStaticMode(bool isStaticMode) { mIsStaticMode = isStaticMode; }
	void reserveStaticLines(ui32 lineCount);
	void clearStaticLines();

private:
	DebugDrawModes mDebugDrawMode = DBG_MAX_DEBUG_DRAW_MODE;
	bool mIsStaticMode = false;
};

