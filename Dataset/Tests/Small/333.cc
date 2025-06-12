#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for TCP Client-Server Communication
class TcpClientServerTest : public TestCase
{
public:
    TcpClientServerTest() : TestCase("Test for TCP Client-Server Setup") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestTcpServerSetup();
        TestTcpClientSetup();
        TestSimulationExecution();
    }

private:
    NodeContainer nodes;
    Ipv4InterfaceContainer interfaces;
    uint16_t port = 8080;

    void TestNodeCreation()
    {
        nodes.Create(2);
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Failed to create 2 nodes (Client and Server)");
    }

    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(nodes);

        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Internet stack not installed on Node " + std::to_string(i));
        }
    }

    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = ipv4.Assign(nodes);

        for (uint32_t i = 0; i < interfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to Node " + std::to_string(i));
        }
    }

    void TestTcpServerSetup()
    {
        Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));
        TcpServerHelper tcpServer(port);
        ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1)); // Install on Node 1 (Server)
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "TCP server application not installed on Node 1 (Server)");
    }

    void TestTcpClientSetup()
    {
        Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));
        TcpClientHelper tcpClient(serverAddress);
        tcpClient.SetAttribute("MaxPackets", UintegerValue(10));          // Number of packets to send
        tcpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));      // Interval between packets
        tcpClient.SetAttribute("PacketSize", UintegerValue(1024));        // Packet size in bytes
        ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0));  // Install on Node 0 (Client)
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "TCP client application not installed on Node 0 (Client)");
    }

    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error");
    }
};

// Register the test
static TcpClientServerTest tcpClientServerTestInstance;
