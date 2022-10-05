#include "stdafx.h"

#include "CppUnitTest.h"

#include "network/cli/GameClient.h"
#include "network/srv/GameServer.h"

#include <thread>
#include <chrono>

// For Sleep
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// To run these tests, Test->Test Explorer
namespace UNITTESTS
{
    TEST_CLASS(NETWORK)
    {
    public:

        TEST_METHOD(GameServerClientConnectDedicated)
        {
            try {
                // Start network API
                InitializeYojimbo();

                // Start server
                GameServer gameServer(ServerType::ONLINE);
                yojimbo::Address address = gameServer.getServerAddress();

                char buffer[256];
                address.ToString(buffer, 256);
                Logger::WriteMessage((std::string("Server address: ") + std::string(buffer) + "\n").c_str());

                // Run server loop
                std::thread serverThread([&]() {
                    gameServer.start();
                    Logger::WriteMessage("Server shutting down\n");
                });

                // Await startup
                while (!gameServer.isRunning());
                Sleep(1000);

                std::atomic_bool quitClient = false;
                std::atomic_bool clientConnected = false;
                std::thread clientThread([address, &quitClient, &clientConnected]() {
                    GameClient client(ClientConnectionType::ONLINE);
                    client.connect(DEFAULT_PRIVATE_KEY, address);

                    char buffer[256];
                    address.ToString(buffer, 256);
                    Logger::WriteMessage((std::string("Client address: ") + std::string(buffer) + "\n").c_str());

                    auto tStart = std::chrono::high_resolution_clock::now();
                    while (!quitClient) {
                        // Get DT in seconds
                        auto tNow = std::chrono::high_resolution_clock::now();
                        double dt = std::chrono::duration<double>(tNow - tStart).count();
                        tStart = tNow;
                        // Update client
                        client.update(dt);
                        // Connection check
                        if (client.isConnected()) {
                            clientConnected = true;
                        }
                    }
                    client.disconnect();


                    sprintf_s(buffer, "Client Ping %.2f\n", client.getCurrentPingMS());
                    Logger::WriteMessage(buffer);
                    Logger::WriteMessage("Client shutting down\n");
                });
                Sleep(1000);


                // End server
                gameServer.shutdown();
                quitClient = true;
                Sleep(200);

                // End thread
                Logger::WriteMessage("Joining threads\n");
                serverThread.join();
                clientThread.join();

                Assert::IsTrue(clientConnected, L"Client was unable to establish a connection");
            }
            catch (int e) {
                Assert::Fail(L"Server failed to startup with exception");
            }

        }
    };
}
