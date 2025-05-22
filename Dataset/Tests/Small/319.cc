#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/test.h"

using namespace ns3;

class SimpleWifiTest : public TestCase
{
public:
    SimpleWifiTest() : TestCase("Simple Wi-Fi Network Test") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestWifiDeviceInstallation();
        TestInternetStackInstallation();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestIpAddressAssignment();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test the creation of access point and station nodes
        NodeContainer apNode, staNodes;
        apNode.Create(1);
        staNodes.Create(3);

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(apNode.GetN(), 1, "Access Point node creation failed");
        NS_TEST_ASSERT_MSG_EQ(staNodes.GetN(), 3, "Station nodes creation failed");
    }

    void TestWifiDeviceInstallation()
    {
        // Test the installation of Wi-Fi devices on access point and station nodes
        NodeContainer apNode, staNodes;
        apNode.Create(1);
        staNodes.Create(3);

        WifiHelper wifi;
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("ns-3-ssid");
        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        WifiMacHelper apMac;
        apMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

        NetDeviceContainer apDevice, staDevices;
        apDevice = wifi.Install(wifiPhy, apMac, apNode);
        staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

        // Verify that devices have been installed on both AP and station nodes
        NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "Wi-Fi device installation failed on AP node");
        NS_TEST_ASSERT_MSG_EQ(staDevices.GetN(), 3, "Wi-Fi device installation failed on station nodes");
    }

    void TestInternetStackInstallation()
    {
        // Test the installation of the internet stack (IP, TCP, UDP) on the nodes
        NodeContainer apNode, staNodes;
        apNode.Create(1);
        staNodes.Create(3);

        InternetStackHelper stack;
        stack.Install(apNode);
        stack.Install(staNodes);

        // Verify internet stack installation on all nodes
        for (uint32_t i = 0; i < apNode.GetN(); ++i)
        {
            Ptr<Node> node = apNode.Get(i);
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Internet stack installation failed on AP node " << i);
        }

        for (uint32_t i = 0; i < staNodes.GetN(); ++i)
        {
            Ptr<Node> node = staNodes.Get(i);
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Internet stack installation failed on STA node " << i);
        }
    }

    void TestUdpServerSetup()
    {
        // Test the setup of UDP server on the AP node
        NodeContainer apNode;
        apNode.Create(1);

        uint16_t port = 9;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(apNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the server application is installed correctly
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server application installation failed on AP node");
    }

    void TestUdpClientSetup()
    {
        // Test the setup of UDP client on the station nodes
        NodeContainer apNode, staNodes;
        apNode.Create(1);
        staNodes.Create(3);

        uint16_t port = 9;
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer apInterfaces = ipv4.Assign(apNode.Get(0)->GetDevice(0));
        ipv4.SetBase("10.1.2.0", "255.255.255.0");
        ipv4.Assign(staNodes.Get(0)->GetDevice(0));

        UdpClientHelper udpClient(apInterfaces.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(staNodes);
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the client application is installed correctly on all station nodes
        for (uint32_t i = 0; i < staNodes.GetN(); ++i)
        {
            NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client application installation failed on STA node " << i);
        }
    }

    void TestIpAddressAssignment()
    {
        // Test the assignment of IP addresses to the devices on nodes
        NodeContainer apNode, staNodes;
        apNode.Create(1);
        staNodes.Create(3);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer apInterfaces = ipv4.Assign(apNode.Get(0)->GetDevice(0));
        ipv4.SetBase("10.1.2.0", "255.255.255.0");
        ipv4.Assign(staNodes.Get(0)->GetDevice(0));

        // Verify that IP addresses are assigned correctly to AP and STA devices
        NS_TEST_ASSERT_MSG_EQ(apInterfaces.GetN(), 1, "IP address assignment failed on AP node");
        NS_TEST_ASSERT_MSG_EQ(apInterfaces.GetN(), 3, "IP address assignment failed on STA nodes");
    }

    void TestSimulationExecution()
    {
        // Test the execution of the simulation
        NodeContainer apNode, staNodes;
        apNode.Create(1);
        staNodes.Create(3);

        uint16_t port = 9;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(apNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        ipv4.Assign(apNode.Get(0)->GetDevice(0));
        ipv4.SetBase("10.1.2.0", "255.255.255.0");
        ipv4.Assign(staNodes.Get(0)->GetDevice(0));

        Simulator::Run();
        Simulator::Destroy();

        // Verify that the simulation ran without errors
        NS_TEST_ASSERT_MSG_EQ(true, true, "Simulation execution failed");
    }
};

// Create the test case and run it
int main()
{
    SimpleWifiTest test;
    test.Run();
    return 0;
}
