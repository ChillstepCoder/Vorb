#pragma once

using PFNGETGLPROC = void* (const char*);

struct GL4API
{
#	include "GLAPI.h"
};

void GetAPI4(GL4API* api, PFNGETGLPROC GetGLProc);
// Inject full trace so we can see every GL procedure call
void InjectAPITracer4(GL4API* api);

extern GL4API GL;