#include "stdafx.h"
#include "SrvWorldInterface.h"

#include "city/City.h"

SrvWorldInterface::SrvWorldInterface()
{
    // Nav graph
    mNavWorld = std::make_unique<NavWorld>(*this);
}

SrvWorldInterface::~SrvWorldInterface()
{

}

void SrvWorldInterface::tickSrv()
{

}

