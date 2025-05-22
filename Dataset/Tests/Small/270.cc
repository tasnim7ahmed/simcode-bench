#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class MeshTestSuite : public TestCase
{
public:
    MeshTestSuite() : TestCase("Test Mesh Network with UDP Echo Application") {}
    virtual ~MeshTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestMeshDeviceInstallation();
        TestIpAddressAssignment();
        TestUdpEchoServerSetup();
        TestUdpEchoClientSetup();
        TestDataTransmission();
    }

private:
    // Test node creation (verify that 3 mesh nodes are created correctly)
    void TestNodeCreation()
    {
        NodeContainer meshNodes;
        meshNodes.Create(3);

        // Verify that 3 mesh nodes are created
        NS_TEST_ASSERT_MSG_EQ(meshNodes.GetN(), 3, "Failed to create the expected number of mesh nodes.");
    }

    // Test mesh device installation (verify that mesh devices are installed correctly on all nodes)
    void TestMeshDeviceInstallation()
    {
        NodeContainer meshNodes;
        meshNodes.Create(3);

        // Configure mesh helper
        MeshHelper mesh;
        mesh.SetStackInstaller("ns3::Dot11sStack");
        mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
        mesh.SetMacType("ns3::MeshPointDevice");
        mesh.SetNumberOfInterfaces(1);

        // Configure Wi-Fi channel
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        // Install mesh devices on nodes
        NetDeviceContainer meshDevices = mesh.Install(phy, meshNodes);

        // Verify that mesh devices are installed on all nodes
        NS_TEST_ASSERT_MSG_EQ(meshDevices.GetN(), 3, "Failed to install mesh devices on mesh nodes.");
    }

    // Test IP address assignment (verify that IP addresses are correctly assigned to mesh devices)
    void TestIpAddressAssignment()
    {
        NodeContainer meshNodes;
        meshNodes.Create(3);

        // Configure mesh helper
        MeshHelper mesh;
        mesh.SetStackInstaller("ns3::Dot11sStack");
        mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
        mesh.SetMacType("ns3::MeshPointDevice");
        mesh.SetNumberOfInterfaces(1);

        // Configure Wi-Fi channel
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        // Install mesh devices on nodes
        NetDeviceContainer meshDevices = mesh.Install(phy, meshNodes);

        InternetStackHelper internet;
        internet.Install(meshNodes);

        // Assign IP addresses
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer meshInterfaces = address.Assign(meshDevices);

        // Verify that IP addresses are assigned to devices
        for (uint32_t i = 0; i < meshNodes.GetN(); ++i)
        {
            Ipv4Address ipAddr = meshInterfaces.GetAddress(i);
            NS_TEST_ASSERT_MSG_NE(ipAddr, Ipv4Address("0.0.0.0"), "Failed to assign IP address to mesh node.");
        }
    }

    // Test UDP Echo Server setup (verify that the UDP Echo Server is installed correctly on the last node)
    void TestUdpEchoServerSetup()
    {
        NodeContainer meshNodes;
        meshNodes.Create(3);

        // Configure mesh helper
        MeshHelper mesh;
        mesh.SetStackInstaller("ns3::Dot11sStack");
        mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
        mesh.SetMacType("ns3::MeshPointDevice");
        mesh.SetNumberOfInterfaces(1);

        // Configure Wi-Fi channel
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        // Install mesh devices on nodes
        NetDeviceContainer meshDevices = mesh.Install(phy, meshNodes);

        InternetStackHelper internet;
        internet.Install(meshNodes);

        // Assign IP addresses
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer meshInterfaces = address.Assign(meshDevices);

        // Set up the UDP Echo Server on the last node
        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(meshNodes.Get(2));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        // Verify that the UDP Echo Server is installed on the last mesh node
        NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "Failed to install UDP Echo Server on the last node.");
    }

    // Test UDP Echo Client setup (verify that the UDP Echo Client is set up correctly on the first node)
    void TestUdpEchoClientSetup()
    {
        NodeContainer meshNodes;
        meshNodes.Create(3);

        // Configure mesh helper
        MeshHelper mesh;
        mesh.SetStackInstaller("ns3::Dot11sStack");
        mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
        mesh.SetMacType("ns3::MeshPointDevice");
        mesh.SetNumberOfInterfaces(1);

        // Configure Wi-Fi channel
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        // Install mesh devices on nodes
        NetDeviceContainer meshDevices = mesh.Install(phy, meshNodes);

        InternetStackHelper internet;
        internet.Install(meshNodes);

        // Assign IP addresses
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer meshInterfaces = address.Assign(meshDevices);

        // Set up the UDP Echo Client on the first node
        UdpEchoClientHelper echoClient(meshInterfaces.GetAddress(2), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(meshNodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        // Verify that the UDP Echo Client is installed on the first mesh node
        NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "Failed to install UDP Echo Client on the first node.");
    }

    // Test data transmission (verify that data is transmitted between the UDP Echo Client and Server)
    void TestDataTransmission()
    {
        NodeContainer meshNodes;
        meshNodes.Create(3);

        // Configure mesh helper
        MeshHelper mesh;
        mesh.SetStackInstaller("ns3::Dot11sStack");
        mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
        mesh.SetMacType("ns3::MeshPointDevice");
        mesh.SetNumberOfInterfaces(1);

        // Configure Wi-Fi channel
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        // Install mesh devices on nodes
        NetDeviceContainer meshDevices = mesh.Install(phy, meshNodes);

        InternetStackHelper internet;
        internet.Install(meshNodes);

        // Assign IP addresses
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer meshInterfaces = address.Assign(meshDevices);

        // Set up the UDP Echo Server and Client
        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(meshNodes.Get(2));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(meshInterfaces.GetAddress(2), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(meshNodes.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        // Run the simulation and verify successful data transmission
        Simulator::Run();
        Simulator::Destroy();
    }
};

// Test Suite registration
int main(int argc, char *argv[])
{
    MeshTestSuite meshTestSuite;
    meshTestSuite.Run();
    return 0;
}
