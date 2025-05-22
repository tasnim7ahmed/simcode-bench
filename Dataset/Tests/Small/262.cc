#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class MeshNetworkTestSuite : public TestCase
{
public:
    MeshNetworkTestSuite() : TestCase("Test Mesh Network Setup for Nodes") {}
    virtual ~MeshNetworkTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestMobilityModel();
        TestMeshNetworkInstallation();
        TestWaveDeviceInstallation();
        TestIpAddressAssignment();
        TestUdpApplicationSetup();
    }

private:
    // Test node creation (ensure correct number of nodes)
    void TestNodeCreation()
    {
        NodeContainer nodes;
        nodes.Create(5); // Create 5 mesh nodes

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 5, "Failed to create 5 nodes.");
    }

    // Test the mobility configuration (verify RandomWalk2dMobilityModel)
    void TestMobilityModel()
    {
        NodeContainer nodes;
        nodes.Create(5);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
        mobility.Install(nodes);

        // Verify that mobility has been installed (can check mobility model here)
        Ptr<MobilityModel> mobilityModel = nodes.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_NE(mobilityModel, nullptr, "Mobility model not installed.");
    }

    // Test mesh network stack installation
    void TestMeshNetworkInstallation()
    {
        NodeContainer nodes;
        nodes.Create(5);

        MeshHelper mesh;
        mesh.SetStackInstall(true);
        mesh.Install(nodes);

        // Verify that the mesh network stack is installed
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<MeshNetDevice> device = nodes.Get(i)->GetDevice(0)->GetObject<MeshNetDevice>();
            NS_TEST_ASSERT_MSG_NE(device, nullptr, "Mesh stack not installed on node.");
        }
    }

    // Test Wi-Fi device installation
    void TestWaveDeviceInstallation()
    {
        NodeContainer nodes;
        nodes.Create(5);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        wifiMac.SetType("ns3::StaWifiMac");

        NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

        // Ensure devices are installed
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 5, "Failed to install Wi-Fi devices on all nodes.");
    }

    // Test IP address assignment to nodes
    void TestIpAddressAssignment()
    {
        NodeContainer nodes;
        nodes.Create(5);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        wifiMac.SetType("ns3::StaWifiMac");

        NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        // Verify that IP addresses are assigned to the nodes
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ipv4Address ip = interfaces.GetAddress(i);
            NS_TEST_ASSERT_MSG_NE(ip, Ipv4Address("0.0.0.0"), "Failed to assign IP address to node.");
        }
    }

    // Test the setup of UDP Echo Server and Client applications
    void TestUdpApplicationSetup()
    {
        NodeContainer nodes;
        nodes.Create(5);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        wifiMac.SetType("ns3::StaWifiMac");

        NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(nodes.Get(4)); // Last node as server
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(interfaces.GetAddress(4), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(3));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(nodes.Get(0)); // First node as client
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        // Verify that the server and client applications are correctly installed
        NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "Failed to install UDP Echo Server on last node.");
        NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "Failed to install UDP Echo Client on first node.");
    }
};

// Instantiate the test suite
static MeshNetworkTestSuite meshNetworkTestSuite;
