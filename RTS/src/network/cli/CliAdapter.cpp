#include "stdafx.h"
#include "CliAdapter.h"

void CliAdapter::OnServerClientConnected(int clientIndex)
{
    // Only server implements these
    UNUSED(clientIndex);
}

void CliAdapter::OnServerClientDisconnected(int clientIndex)
{
    // Only server implements these
    UNUSED(clientIndex);
}
