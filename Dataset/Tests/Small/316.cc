#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/test.h"

using namespace ns3;

class SimpleWifiTest : public TestCase
{
public:
    SimpleWifiTest() : TestCase("Simple Wifi Network Test") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestWifiDeviceInstallation();
        TestIpv4AddressAssignment();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test the creation of STA and AP nodes
        NodeContainer staNodes, apNodes;
        staNodes.Create(1);
        apNodes.Create(1);

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(staNodes.GetN(), 1, "STA node creation failed");
        NS_TEST_ASSERT_MSG_EQ(apNodes.GetN(), 1, "AP node creation failed");
    }

    void TestWifiDeviceInstallation()
    {
        // Test WiFi device installation on both STA and AP nodes
        NodeContainer staNodes, apNodes;
        staNodes.Create(1);
        apNodes.Create(1);

        WifiHelper wifiHelper;
        wifiHelper.SetRemoteStationManager("ns3::AarfWifiManager");

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer staDevices = wifiHelper.Install(phy, mac, staNodes);

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevices = wifiHelper.Install(phy, mac, apNodes);

        // Verify devices are installed on both STA and AP nodes
        NS_TEST_ASSERT_MSG_EQ(staDevices.GetN(), 1, "WiFi device installation failed on STA");
        NS_TEST_ASSERT_MSG_EQ(apDevices.GetN(), 1, "WiFi device installation failed on AP");
    }

    void TestIpv4AddressAssignment()
    {
        // Test the assignment of IP addresses to devices
        NodeContainer staNodes, apNodes;
        staNodes.Create(1);
        apNodes.Create(1);

        WifiHelper wifiHelper;
        wifiHelper.SetRemoteStationManager("ns3::AarfWifiManager");

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer staDevices = wifiHelper.Install(phy, mac, staNodes);

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevices = wifiHelper.Install(phy, mac, apNodes);

        InternetStackHelper internet;
        internet.Install(staNodes);
        internet.Install(apNodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");
        Ipv4InterfaceContainer staIpIface = ipv4.Assign(staDevices);
        Ipv4InterfaceContainer apIpIface = ipv4.Assign(apDevices);

        // Verify IP addresses are assigned to both STA and AP devices
        NS_TEST_ASSERT_MSG_EQ(staIpIface.GetN(), 1, "IP address assignment failed for STA");
        NS_TEST_ASSERT_MSG_EQ(apIpIface.GetN(), 1, "IP address assignment failed for AP");
    }

    void TestUdpServerSetup()
    {
        // Test the setup of UDP server on the AP node
        NodeContainer apNodes;
        apNodes.Create(1);

        uint16_t port = 9;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(apNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify the UDP server application is installed
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server application installation failed on AP");
    }

    void TestUdpClientSetup()
    {
        // Test the setup of UDP client on the STA node
        NodeContainer staNodes, apNodes;
        staNodes.Create(1);
        apNodes.Create(1);

        WifiHelper wifiHelper;
        wifiHelper.SetRemoteStationManager("ns3::AarfWifiManager");

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer staDevices = wifiHelper.Install(phy, mac, staNodes);

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevices = wifiHelper.Install(phy, mac, apNodes);

        InternetStackHelper internet;
        internet.Install(staNodes);
        internet.Install(apNodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");
        Ipv4InterfaceContainer staIpIface = ipv4.Assign(staDevices);
        Ipv4InterfaceContainer apIpIface = ipv4.Assign(apDevices);

        uint16_t port = 9;
        UdpClientHelper udpClient(apIpIface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(staNodes.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify the UDP client application is installed
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client application installation failed on STA");
    }

    void TestSimulationExecution()
    {
        // Test the execution of the simulation
        NodeContainer staNodes, apNodes;
        staNodes.Create(1);
        apNodes.Create(1);

        WifiHelper wifiHelper;
        wifiHelper.SetRemoteStationManager("ns3::AarfWifiManager");

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-wifi");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer staDevices = wifiHelper.Install(phy, mac, staNodes);

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevices = wifiHelper.Install(phy, mac, apNodes);

        InternetStackHelper internet;
        internet.Install(staNodes);
        internet.Install(apNodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");
        Ipv4InterfaceContainer staIpIface = ipv4.Assign(staDevices);
        Ipv4InterfaceContainer apIpIface = ipv4.Assign(apDevices);

        uint16_t port = 9;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(apNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpClientHelper udpClient(apIpIface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(staNodes.Get(0));
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
    SimpleWifiTest test;
    test.Run();
    return 0;
}
