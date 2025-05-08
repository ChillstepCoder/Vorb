#include "stdafx.h"
#include "SrvAdapterOLD.h"

#include "GameServerOLD.h"

void SrvAdapterOLD::OnServerClientConnected(int clientIndex) {
    mGameServer.clientConnected(clientIndex);
}

void SrvAdapterOLD::OnServerClientDisconnected(int clientIndex) {
    mGameServer.clientDisconnected(clientIndex);
}
