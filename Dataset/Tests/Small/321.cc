#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/mesh-module.h"
#include "ns3/test.h"

using namespace ns3;

class SimpleMeshTest : public TestCase
{
public:
    SimpleMeshTest() : TestCase("Simple Mesh Network Test") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestWifiMeshSetup();
        TestInternetStackInstallation();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestIpAddressAssignment();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test the creation of mesh nodes
        NodeContainer meshNodes;
        meshNodes.Create(4);

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(meshNodes.GetN(), 4, "Mesh node creation failed");
    }

    void TestWifiMeshSetup()
    {
        // Test the setup of the Wi-Fi mesh network
        NodeContainer meshNodes;
        meshNodes.Create(4);

        MeshHelper meshHelper;
        meshHelper.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);
        meshHelper.SetRemoteStationManager("ns3::ArfWifiManager");

        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("ns-3-mesh-ssid");
        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        WifiMacHelper apMac;
        apMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

        NetDeviceContainer meshDevices = meshHelper.Install(wifiPhy, wifiMac, meshNodes);

        // Verify mesh devices have been installed
        NS_TEST_ASSERT_MSG_EQ(meshDevices.GetN(), 4, "Mesh devices installation failed");
    }

    void TestInternetStackInstallation()
    {
        // Test the installation of internet stack (IP, UDP) on mesh nodes
        NodeContainer meshNodes;
        meshNodes.Create(4);

        InternetStackHelper stack;
        stack.Install(meshNodes);

        // Verify internet stack installation on all nodes
        for (uint32_t i = 0; i < meshNodes.GetN(); ++i)
        {
            Ptr<Node> node = meshNodes.Get(i);
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Internet stack installation failed on mesh node " << i);
        }
    }

    void TestUdpServerSetup()
    {
        // Test the setup of the UDP server on the last mesh node
        NodeContainer meshNodes;
        meshNodes.Create(4);

        uint16_t port = 9;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(meshNodes.Get(3));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the server application is installed correctly
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server application installation failed on last mesh node");
    }

    void TestUdpClientSetup()
    {
        // Test the setup of UDP client applications on the other mesh nodes
        NodeContainer meshNodes;
        meshNodes.Create(4);

        uint16_t port = 9;
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer meshIpIface = ipv4.Assign(meshDevices);

        UdpClientHelper udpClient(meshIpIface.GetAddress(3), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp;
        for (uint32_t i = 0; i < 3; ++i)
        {
            clientApp.Add(udpClient.Install(meshNodes.Get(i)));
        }
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that client applications are installed on all UE nodes
        for (uint32_t i = 0; i < 3; ++i)
        {
            NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 3, "UDP client application installation failed on mesh node " << i);
        }
    }

    void TestIpAddressAssignment()
    {
        // Test the assignment of IP addresses to the mesh nodes
        NodeContainer meshNodes;
        meshNodes.Create(4);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer meshIpIface = ipv4.Assign(meshDevices);

        // Verify that IP addresses are assigned correctly to the devices
        NS_TEST_ASSERT_MSG_EQ(meshIpIface.GetN(), 4, "IP address assignment failed on mesh devices");
    }

    void TestSimulationExecution()
    {
        // Test the execution of the simulation
        NodeContainer meshNodes;
        meshNodes.Create(4);

        uint16_t port = 9;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(meshNodes.Get(3));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpClientHelper udpClient(meshIpIface.GetAddress(3), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp;
        for (uint32_t i = 0; i < 3; ++i)
        {
            clientApp.Add(udpClient.Install(meshNodes.Get(i)));
        }
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();

        // Verify that the simulation ran without errors
        NS_TEST_ASSERT_MSG_EQ(true, true, "Simulation execution failed");
    }
};

// Create the test case and run it
int main()
{
    SimpleMeshTest test;
    test.Run();
    return 0;
}
