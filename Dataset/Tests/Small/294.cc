#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/test.h"

using namespace ns3;

class WifiMeshUdpExampleTest : public TestCase
{
public:
    WifiMeshUdpExampleTest() : TestCase("Test Wi-Fi Mesh UDP Example") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestWifiMeshNetworkSetup();
        TestMobilityInstallation();
        TestIpv4AddressAssignment();
        TestUdpServerInstallation();
        TestUdpClientInstallation();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test creation of 5 Wi-Fi nodes
        NodeContainer wifiNodes;
        wifiNodes.Create(5);

        // Verify that 5 nodes are created
        NS_TEST_ASSERT_MSG_EQ(wifiNodes.GetN(), 5, "Node creation failed, expected 5 nodes");
    }

    void TestWifiMeshNetworkSetup()
    {
        // Test Wi-Fi Mesh network setup
        NodeContainer wifiNodes;
        wifiNodes.Create(5);

        WifiMeshHelper wifiMesh;
        wifiMesh.SetRemoteStationManager("ns3::AarfWifiManager");

        MeshHelper meshHelper;
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("ns3-mesh-ssid");
        wifiMac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer meshDevices = wifiMesh.Install(wifiPhy, wifiMac, wifiNodes);

        // Verify that 5 devices are installed
        NS_TEST_ASSERT_MSG_EQ(meshDevices.GetN(), 5, "Wi-Fi Mesh network setup failed, expected 5 devices");
    }

    void TestMobilityInstallation()
    {
        // Test the installation of the mobility model
        NodeContainer wifiNodes;
        wifiNodes.Create(5);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel");
        mobility.Install(wifiNodes);

        // Verify mobility model installation (just a basic check on number of installed models)
        Ptr<MobilityModel> mobilityModel = wifiNodes.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_EQ(mobilityModel != nullptr, true, "Mobility model installation failed on node 0");
    }

    void TestIpv4AddressAssignment()
    {
        // Test IP address assignment to the devices
        NodeContainer wifiNodes;
        wifiNodes.Create(5);

        WifiMeshHelper wifiMesh;
        wifiMesh.SetRemoteStationManager("ns3::AarfWifiManager");

        MeshHelper meshHelper;
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("ns3-mesh-ssid");
        wifiMac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer meshDevices = wifiMesh.Install(wifiPhy, wifiMac, wifiNodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(meshDevices);

        // Verify that IP addresses are assigned to all nodes
        Ipv4Address address = interfaces.GetAddress(0);
        NS_TEST_ASSERT_MSG_EQ(address.IsValid(), true, "IP address assignment failed for node 0");
    }

    void TestUdpServerInstallation()
    {
        // Test UDP server installation on the last node
        NodeContainer wifiNodes;
        wifiNodes.Create(5);

        uint16_t port = 8080;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(wifiNodes.Get(4)); // Last node
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the server application is installed
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server application installation failed");
    }

    void TestUdpClientInstallation()
    {
        // Test UDP client installation on the first node
        NodeContainer wifiNodes;
        wifiNodes.Create(5);

        uint16_t port = 8080;
        WifiMeshHelper wifiMesh;
        wifiMesh.SetRemoteStationManager("ns3::AarfWifiManager");

        MeshHelper meshHelper;
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("ns3-mesh-ssid");
        wifiMac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer meshDevices = wifiMesh.Install(wifiPhy, wifiMac, wifiNodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(meshDevices);

        UdpClientHelper udpClient(interfaces.GetAddress(4), port); // Sending to last node
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(wifiNodes.Get(0)); // First node
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the client application is installed
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client application installation failed");
    }

    void TestSimulationExecution()
    {
        // Test if the simulation runs without errors
        NodeContainer wifiNodes;
        wifiNodes.Create(5);

        WifiMeshHelper wifiMesh;
        wifiMesh.SetRemoteStationManager("ns3::AarfWifiManager");

        MeshHelper meshHelper;
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("ns3-mesh-ssid");
        wifiMac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer meshDevices = wifiMesh.Install(wifiPhy, wifiMac, wifiNodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(meshDevices);

        uint16_t port = 8080;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(wifiNodes.Get(4)); // Last node
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpClientHelper udpClient(interfaces.GetAddress(4), port); // Sending to last node
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(wifiNodes.Get(0)); // First node
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();

        // Check if simulation completes successfully
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext()->GetTotalSimTime().GetSeconds() > 0, true, "Simulation did not run correctly");
    }
};

int main(int argc, char *argv[])
{
    // Run the unit tests
    Ptr<WifiMeshUdpExampleTest> test = CreateObject<WifiMeshUdpExampleTest>();
    test->Run();

    return 0;
}
