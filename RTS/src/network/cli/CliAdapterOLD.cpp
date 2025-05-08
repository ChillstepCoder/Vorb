#include "stdafx.h"
#include "CliAdapterOLD.h"

void CliAdapterOLD::OnServerClientConnected(int clientIndex)
{
    // Only server implements these
    UNUSED(clientIndex);
}

void CliAdapterOLD::OnServerClientDisconnected(int clientIndex)
{
    // Only server implements these
    UNUSED(clientIndex);
}
