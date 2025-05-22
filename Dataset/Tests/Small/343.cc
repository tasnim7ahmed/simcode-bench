#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for Point-to-Point UDP Communication
class P2PUdpExampleTest : public TestCase
{
public:
    P2PUdpExampleTest() : TestCase("Test for P2P UDP Communication Setup") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestP2PLinkConfiguration();
        TestDeviceInstallation();
        TestIpAddressAssignment();
        TestUdpEchoServerSetup();
        TestUdpEchoClientSetup();
        TestSimulationExecution();
    }

private:
    NodeContainer nodes;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;

    void TestNodeCreation()
    {
        nodes.Create(2); // Create two nodes: one for the client and one for the server
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Failed to create two nodes");
    }

    void TestP2PLinkConfiguration()
    {
        // Set up the Point-to-Point link (bandwidth: 5Mbps, delay: 2ms)
        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        // Install devices on nodes
        devices = p2p.Install(nodes);

        // Verify that devices have been installed
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Failed to install devices on nodes");
    }

    void TestDeviceInstallation()
    {
        // Verify devices are installed correctly
        for (uint32_t i = 0; i < devices.GetN(); ++i)
        {
            Ptr<NetDevice> device = devices.Get(i);
            NS_TEST_ASSERT_MSG_NOT_NULL(device, "Device not installed on node");
        }
    }

    void TestIpAddressAssignment()
    {
        // Assign IP addresses to devices
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = address.Assign(devices);

        // Verify that IP addresses are assigned correctly
        for (uint32_t i = 0; i < interfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to node");
        }
    }

    void TestUdpEchoServerSetup()
    {
        uint16_t port = 9; // Port for server
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApp = echoServer.Install(nodes.Get(1)); // Install on server node
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify server application is installed correctly
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP Echo Server application not installed on server node");
    }

    void TestUdpEchoClientSetup()
    {
        uint16_t port = 9; // Port for client
        UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port); // Server address and port 9
        echoClient.SetAttribute("MaxPackets", UintegerValue(10));   // Send 10 packets
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // Send packets every 1 second
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));   // 1024 bytes packet
        ApplicationContainer clientApp = echoClient.Install(nodes.Get(0)); // Install on client node
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify client application is installed correctly
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP Echo Client application not installed on client node");
    }

    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error");
    }
};

// Register the test
static P2PUdpExampleTest p2pUdpExampleTestInstance;
