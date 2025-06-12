#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/test.h"

using namespace ns3;

class WifiUdpExampleTest : public TestCase
{
public:
    WifiUdpExampleTest() : TestCase("Test Wifi UDP Example") {}

    virtual void DoRun() override
    {
        TestNodeCreation();
        TestWifiDeviceInstallation();
        TestMobilityInstallation();
        TestIpv4AddressAssignment();
        TestUdpServerInstallation();
        TestUdpClientInstallation();
        TestSimulationExecution();
    }

private:
    void TestNodeCreation()
    {
        // Test creation of 1 AP and 3 STAs
        NodeContainer wifiNodes;
        wifiNodes.Create(4); // 1 AP and 3 STAs

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(wifiNodes.GetN(), 4, "Node creation failed, incorrect number of nodes");
    }

    void TestWifiDeviceInstallation()
    {
        // Test the installation of Wi-Fi devices (AP and STAs)
        NodeContainer wifiNodes;
        wifiNodes.Create(4); // 1 AP and 3 STAs

        WifiHelper wifi;
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("ns3-ssid");

        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(1, 2, 3));

        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(0));

        // Verify the devices are installed
        NS_TEST_ASSERT_MSG_EQ(staDevices.GetN(), 3, "Wi-Fi STA device installation failed");
        NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "Wi-Fi AP device installation failed");
    }

    void TestMobilityInstallation()
    {
        // Test mobility model installation
        NodeContainer wifiNodes;
        wifiNodes.Create(4); // 1 AP and 3 STAs

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiNodes.Get(0)); // AP position
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel");
        mobility.Install(wifiNodes.Get(1, 2, 3)); // STA positions

        // Verify mobility model installation (basic check on the first node)
        Ptr<MobilityModel> mobilityModel = wifiNodes.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_EQ(mobilityModel != nullptr, true, "Mobility model installation failed for AP");
    }

    void TestIpv4AddressAssignment()
    {
        // Test IP address assignment
        NodeContainer wifiNodes;
        wifiNodes.Create(4); // 1 AP and 3 STAs

        WifiHelper wifi;
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("ns3-ssid");

        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(1, 2, 3));

        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(0));

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(staDevices);
        Ipv4InterfaceContainer apInterface = ipv4.Assign(apDevice);

        // Verify that IPs are assigned to devices
        Ipv4Address apAddress = apInterface.GetAddress(0);
        NS_TEST_ASSERT_MSG_EQ(apAddress.IsValid(), true, "IP address assignment failed for AP");

        Ipv4Address staAddress = interfaces.GetAddress(0);
        NS_TEST_ASSERT_MSG_EQ(staAddress.IsValid(), true, "IP address assignment failed for STA");
    }

    void TestUdpServerInstallation()
    {
        // Test UDP server installation on AP
        NodeContainer wifiNodes;
        wifiNodes.Create(4); // 1 AP and 3 STAs

        uint16_t port = 8080;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(wifiNodes.Get(0)); // AP as server
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the server application is installed
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server application installation failed");
    }

    void TestUdpClientInstallation()
    {
        // Test UDP client installation on STA
        NodeContainer wifiNodes;
        wifiNodes.Create(4); // 1 AP and 3 STAs

        uint16_t port = 8080;
        WifiHelper wifi;
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("ns3-ssid");

        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(1, 2, 3));

        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(0));

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(staDevices);
        Ipv4InterfaceContainer apInterface = ipv4.Assign(apDevice);

        UdpClientHelper udpClient(apInterface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(wifiNodes.Get(1)); // STA as client
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the client application is installed
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client application installation failed");
    }

    void TestSimulationExecution()
    {
        // Test if the simulation runs without errors
        NodeContainer wifiNodes;
        wifiNodes.Create(4); // 1 AP and 3 STAs

        WifiHelper wifi;
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("ns3-ssid");

        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(1, 2, 3));

        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(0));

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(staDevices);
        Ipv4InterfaceContainer apInterface = ipv4.Assign(apDevice);

        uint16_t port = 8080;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(wifiNodes.Get(0)); // AP as server
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpClientHelper udpClient(apInterface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = udpClient.Install(wifiNodes.Get(1)); // STA as client
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();
    }
};

// Create the test case and run it
int main()
{
    WifiUdpExampleTest test;
    test.Run();
    return 0;
}
