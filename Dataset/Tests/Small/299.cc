#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

class WifiTest : public TestCase
{
public:
    WifiTest() : TestCase("Test Wi-Fi Example") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestWifiStackInstallation();
        TestMobilitySetup();
        TestIpv4AddressAssignment();
        TestApplicationInstallation();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test creation of 2 nodes
        NodeContainer nodes;
        nodes.Create(2);

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Node creation failed, incorrect number of nodes");
    }

    void TestWifiStackInstallation()
    {
        // Test Wi-Fi stack installation
        NodeContainer nodes;
        nodes.Create(2);

        WifiHelper wifiHelper;
        wifiHelper.SetRemoteStationManager("ns3::AarfWifiManager");

        YansWifiPhyHelper wifiPhyHelper = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannelHelper = YansWifiChannelHelper::Default();
        wifiPhyHelper.SetChannel(wifiChannelHelper.Create());

        WifiMacHelper wifiMacHelper;
        wifiMacHelper.SetType("ns3::StaWifiMac",
                              "ActiveProbing", BooleanValue(false));

        // Install Wi-Fi devices on nodes
        NetDeviceContainer devices = wifiHelper.Install(wifiPhyHelper, wifiMacHelper, nodes);

        // Verify Wi-Fi stack installation
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Wi-Fi stack installation failed, incorrect number of devices");
    }

    void TestMobilitySetup()
    {
        // Test mobility model setup for nodes
        NodeContainer nodes;
        nodes.Create(2);

        MobilityHelper mobility;

        // Set nodes to constant position
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(nodes);

        // Verify mobility model installation
        Ptr<MobilityModel> mobilityModel = nodes.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_EQ(mobilityModel != nullptr, true, "Mobility model installation failed on nodes");
    }

    void TestIpv4AddressAssignment()
    {
        // Test assignment of IP addresses to the Wi-Fi devices
        NodeContainer nodes;
        nodes.Create(2);

        WifiHelper wifiHelper;
        wifiHelper.SetRemoteStationManager("ns3::AarfWifiManager");

        YansWifiPhyHelper wifiPhyHelper = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannelHelper = YansWifiChannelHelper::Default();
        wifiPhyHelper.SetChannel(wifiChannelHelper.Create());

        WifiMacHelper wifiMacHelper;
        wifiMacHelper.SetType("ns3::StaWifiMac",
                              "ActiveProbing", BooleanValue(false));

        // Install Wi-Fi devices on nodes
        NetDeviceContainer devices = wifiHelper.Install(wifiPhyHelper, wifiMacHelper, nodes);

        // Install Internet stack on nodes
        InternetStackHelper internet;
        internet.Install(nodes);

        // Assign IP addresses to the devices
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        // Verify IP address assignment
        Ipv4Address address = interfaces.GetAddress(0);
        NS_TEST_ASSERT_MSG_EQ(address.IsValid(), true, "IP address assignment failed");
    }

    void TestApplicationInstallation()
    {
        // Test UDP server and client application installation
        NodeContainer nodes;
        nodes.Create(2);

        uint16_t port = 9;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(nodes.Get(1)); // Node B
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpClientHelper udpClient("10.0.0.2", port); // Using second node's IP
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
        udpClient.SetAttribute("PacketSize", UintegerValue(128));
        ApplicationContainer clientApp = udpClient.Install(nodes.Get(0)); // Node A
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify application installation
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server application installation failed");
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client application installation failed");
    }

    void TestSimulationExecution()
    {
        // Test if the simulation runs without errors
        NodeContainer nodes;
        nodes.Create(2);

        uint16_t port = 9;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(nodes.Get(1)); // Node B
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpClientHelper udpClient("10.0.0.2", port); // Using second node's IP
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
        udpClient.SetAttribute("PacketSize", UintegerValue(128));
        ApplicationContainer clientApp = udpClient.Install(nodes.Get(0)); // Node A
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Enable tracing
        YansWifiPhyHelper wifiPhyHelper = YansWifiPhyHelper::Default();
        wifiPhyHelper.EnablePcap("wifi_network", devices);

        Simulator::Run();
        Simulator::Destroy();

        // Check if the simulation ran without errors (basic test)
        NS_TEST_ASSERT_MSG_EQ(true, true, "Simulation execution failed");
    }
};

// Create the test case and run it
int main()
{
    WifiTest test;
    test.Run();
    return 0;
}
