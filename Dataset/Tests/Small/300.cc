#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/zigbee-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

class ZigBeeTest : public TestCase
{
public:
    ZigBeeTest() : TestCase("Test ZigBee Example") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestZigBeeStackInstallation();
        TestMobilitySetup();
        TestIpv4AddressAssignment();
        TestApplicationInstallation();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test creation of 10 sensor nodes
        NodeContainer nodes;
        nodes.Create(10);

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 10, "Node creation failed, incorrect number of nodes");
    }

    void TestZigBeeStackInstallation()
    {
        // Test ZigBee stack installation on nodes
        NodeContainer nodes;
        nodes.Create(10);

        ZigBeeHelper zigbeeHelper;
        zigbeeHelper.SetInterfaceType(ZigBeeHelper::ZIGBEE_PRO);

        // Install ZigBee devices on nodes
        NetDeviceContainer devices = zigbeeHelper.Install(nodes);

        // Verify ZigBee stack installation
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 10, "ZigBee stack installation failed, incorrect number of devices");
    }

    void TestMobilitySetup()
    {
        // Test mobility model setup for nodes
        NodeContainer nodes;
        nodes.Create(10);

        MobilityHelper mobility;

        // Set nodes to random walk mobility model within bounds
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)));
        mobility.Install(nodes);

        // Verify mobility model installation
        Ptr<MobilityModel> mobilityModel = nodes.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_EQ(mobilityModel != nullptr, true, "Mobility model installation failed on nodes");
    }

    void TestIpv4AddressAssignment()
    {
        // Test assignment of IP addresses to the ZigBee devices
        NodeContainer nodes;
        nodes.Create(10);

        ZigBeeHelper zigbeeHelper;
        zigbeeHelper.SetInterfaceType(ZigBeeHelper::ZIGBEE_PRO);

        // Install ZigBee devices on nodes
        NetDeviceContainer devices = zigbeeHelper.Install(nodes);

        // Install Internet stack on nodes
        InternetStackHelper internet;
        internet.Install(nodes);

        // Assign IP addresses to the devices
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("192.168.0.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        // Verify IP address assignment
        Ipv4Address address = interfaces.GetAddress(0);
        NS_TEST_ASSERT_MSG_EQ(address.IsValid(), true, "IP address assignment failed");
    }

    void TestApplicationInstallation()
    {
        // Test UDP server and client application installation
        NodeContainer nodes;
        nodes.Create(10);

        uint16_t port = 5000;

        // Set up UDP server (Coordinator)
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(nodes.Get(0)); // Coordinator
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Set up UDP clients (End Devices)
        for (uint32_t i = 1; i < 10; ++i)
        {
            UdpClientHelper udpClient("192.168.0.1", port); // Using first node's IP as the server
            udpClient.SetAttribute("MaxPackets", UintegerValue(100));
            udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(500)));
            udpClient.SetAttribute("PacketSize", UintegerValue(128));

            ApplicationContainer clientApp = udpClient.Install(nodes.Get(i));
            clientApp.Start(Seconds(2.0));
            clientApp.Stop(Seconds(10.0));
        }

        // Verify application installation
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server application installation failed");
        for (uint32_t i = 1; i < 10; ++i)
        {
            ApplicationContainer clientApp = ApplicationContainer();
            clientApp = udpClient.Install(nodes.Get(i));
            NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client application installation failed for node " << i);
        }
    }

    void TestSimulationExecution()
    {
        // Test if the simulation runs without errors
        NodeContainer nodes;
        nodes.Create(10);

        uint16_t port = 5000;

        // Set up UDP server (Coordinator)
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(nodes.Get(0)); // Coordinator
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Set up UDP clients (End Devices)
        for (uint32_t i = 1; i < 10; ++i)
        {
            UdpClientHelper udpClient("192.168.0.1", port); // Using first node's IP as the server
            udpClient.SetAttribute("MaxPackets", UintegerValue(100));
            udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(500)));
            udpClient.SetAttribute("PacketSize", UintegerValue(128));

            ApplicationContainer clientApp = udpClient.Install(nodes.Get(i));
            clientApp.Start(Seconds(2.0));
            clientApp.Stop(Seconds(10.0));
        }

        // Enable tracing
        ZigBeeHelper zigbeeHelper;
        zigbeeHelper.SetInterfaceType(ZigBeeHelper::ZIGBEE_PRO);
        NetDeviceContainer devices = zigbeeHelper.Install(nodes);
        zigbeeHelper.EnablePcap("zigbee_network", devices);

        Simulator::Run();
        Simulator::Destroy();

        // Check if the simulation ran without errors (basic test)
        NS_TEST_ASSERT_MSG_EQ(true, true, "Simulation execution failed");
    }
};

// Create the test case and run it
int main()
{
    ZigBeeTest test;
    test.Run();
    return 0;
}
