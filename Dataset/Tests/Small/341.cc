#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for MANET UDP Communication
class ManetUdpExampleTest : public TestCase
{
public:
    ManetUdpExampleTest() : TestCase("Test for MANET UDP Communication Setup") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestMobilitySetup();
        TestDeviceInstallation();
        TestIpAddressAssignment();
        TestUdpServerSetup();
        TestUdpClientSetup();
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

    void TestMobilitySetup()
    {
        // Setup mobility model for the nodes (simulate movement)
        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                      "X", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"),
                                      "Y", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"));
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Time", TimeValue(Seconds(2.0)),
                                  "Speed", StringValue("ns3::UniformRandomVariable[Min=1|Max=5]"));
        mobility.Install(nodes);

        // Verify mobility model is set for both nodes
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<MobilityModel> mobilityModel = nodes.Get(i)->GetObject<MobilityModel>();
            NS_TEST_ASSERT_MSG_NOT_NULL(mobilityModel, "Mobility model not installed on node");
        }
    }

    void TestDeviceInstallation()
    {
        // Install the point-to-point link (for logical communication)
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));
        devices = pointToPoint.Install(nodes);

        // Verify devices are installed on both nodes
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Failed to install devices on nodes");
    }

    void TestIpAddressAssignment()
    {
        // Assign IP addresses to the devices
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = address.Assign(devices);

        // Verify that IP addresses are assigned correctly to both nodes
        for (uint32_t i = 0; i < interfaces.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(i), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to node");
        }
    }

    void TestUdpServerSetup()
    {
        uint16_t port = 9; // Port number for server
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(nodes.Get(1)); // Install on server node
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(20.0));

        // Verify UDP server application is installed correctly
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP server application not installed on server node");
    }

    void TestUdpClientSetup()
    {
        uint16_t port = 9; // Port number for client
        UdpClientHelper udpClient(interfaces.GetAddress(1), port); // Connect to server node
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));    // Send 100 packets
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.1))); // Send packets every 0.1 seconds
        udpClient.SetAttribute("PacketSize", UintegerValue(512));    // 512 bytes packets
        ApplicationContainer clientApp = udpClient.Install(nodes.Get(0)); // Install on client node
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(20.0));

        // Verify UDP client application is installed correctly
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP client application not installed on client node");
    }

    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error");
    }
};

// Register the test
static ManetUdpExampleTest manetUdpExampleTestInstance;
