#include "stdafx.h"
#include "SrvAdapter.h"

#include "GameServer.h"

void SrvAdapter::OnServerClientConnected(int clientIndex) {
    mGameServer.clientConnected(clientIndex);
}

void SrvAdapter::OnServerClientDisconnected(int clientIndex) {
    mGameServer.clientDisconnected(clientIndex);
}
