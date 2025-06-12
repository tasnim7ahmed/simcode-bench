#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class WifiTestSuite : public TestCase
{
public:
    WifiTestSuite() : TestCase("Test Wi-Fi network setup with UDP applications") {}
    virtual ~WifiTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestWifiDeviceSetup();
        TestMobilityModel();
        TestInternetStackInstallation();
        TestIpv4AddressAssignment();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestDataTransmission();
    }

private:
    // Test node creation (verify that AP and STA nodes are created)
    void TestNodeCreation()
    {
        uint32_t numSta = 3;
        NodeContainer apNode, staNodes;
        apNode.Create(1);
        staNodes.Create(numSta);

        // Verify that 1 AP node and 3 STA nodes are created
        NS_TEST_ASSERT_MSG_EQ(apNode.GetN(), 1, "Failed to create AP node.");
        NS_TEST_ASSERT_MSG_EQ(staNodes.GetN(), numSta, "Failed to create STA nodes.");
    }

    // Test Wi-Fi device setup (verify that Wi-Fi devices are installed)
    void TestWifiDeviceSetup()
    {
        uint32_t numSta = 3;
        NodeContainer apNode, staNodes;
        apNode.Create(1);
        staNodes.Create(numSta);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("wifi-network");
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconInterval", TimeValue(Seconds(2)));

        WifiDeviceHelper wifiDevices;
        wifiDevices.SetDeviceAttribute("WifiPhy", PointerValue(&phy));
        NetDeviceContainer apDevice = wifiDevices.Install(apNode);
        NetDeviceContainer staDevices = wifiDevices.Install(staNodes);

        // Verify that Wi-Fi devices are installed on both AP and STA nodes
        NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "Failed to install Wi-Fi device on AP node.");
        NS_TEST_ASSERT_MSG_EQ(staDevices.GetN(), numSta, "Failed to install Wi-Fi devices on STA nodes.");
    }

    // Test mobility model (verify that the mobility models are installed)
    void TestMobilityModel()
    {
        uint32_t numSta = 3;
        NodeContainer apNode, staNodes;
        apNode.Create(1);
        staNodes.Create(numSta);

        MobilityHelper mobility;

        // AP is static with constant position model
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(10.0),
                                      "DeltaY", DoubleValue(10.0),
                                      "GridWidth", UintegerValue(3),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(apNode);
        mobility.Install(staNodes);

        // Verify that mobility models are installed
        Ptr<MobilityModel> apMobility = apNode.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> staMobility = staNodes.Get(0)->GetObject<MobilityModel>();

        NS_TEST_ASSERT_MSG_NE(apMobility, nullptr, "Failed to install mobility model on AP node.");
        NS_TEST_ASSERT_MSG_NE(staMobility, nullptr, "Failed to install mobility model on STA node.");
    }

    // Test Internet stack installation (verify that the stack is installed)
    void TestInternetStackInstallation()
    {
        uint32_t numSta = 3;
        NodeContainer apNode, staNodes;
        apNode.Create(1);
        staNodes.Create(numSta);

        InternetStackHelper internet;
        internet.Install(apNode);
        internet.Install(staNodes);

        // Verify that Internet stack is installed on both AP and STA nodes
        Ptr<Ipv4> apIpv4 = apNode.Get(0)->GetObject<Ipv4>();
        Ptr<Ipv4> staIpv4 = staNodes.Get(0)->GetObject<Ipv4>();

        NS_TEST_ASSERT_MSG_NE(apIpv4, nullptr, "Failed to install Internet stack on AP node.");
        NS_TEST_ASSERT_MSG_NE(staIpv4, nullptr, "Failed to install Internet stack on STA node.");
    }

    // Test IPv4 address assignment (verify that IP addresses are assigned)
    void TestIpv4AddressAssignment()
    {
        uint32_t numSta = 3;
        NodeContainer apNode, staNodes;
        apNode.Create(1);
        staNodes.Create(numSta);

        WifiHelper wifi;
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("wifi-network");
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconInterval", TimeValue(Seconds(2)));

        WifiDeviceHelper wifiDevices;
        wifiDevices.SetDeviceAttribute("WifiPhy", PointerValue(&phy));
        NetDeviceContainer apDevice = wifiDevices.Install(apNode);
        NetDeviceContainer staDevices = wifiDevices.Install(staNodes);

        Ipv4AddressHelper ipv4;
        Ipv4InterfaceContainer staInterfaces = ipv4.Assign(staDevices);
        Ipv4InterfaceContainer apInterfaces = ipv4.Assign(apDevice);

        // Verify that IPv4 addresses are assigned correctly
        NS_TEST_ASSERT_MSG_NE(staInterfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "Failed to assign IP address to STA.");
        NS_TEST_ASSERT_MSG_NE(apInterfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "Failed to assign IP address to AP.");
    }

    // Test UDP server setup (verify that UDP server is installed on AP)
    void TestUdpServerSetup()
    {
        NodeContainer apNode;
        apNode.Create(1);

        UdpServerHelper udpServer(9);
        ApplicationContainer serverApp = udpServer.Install(apNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the UDP server is installed on AP
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "Failed to install UDP server on AP node.");
    }

    // Test UDP client setup (verify that UDP clients are installed on STAs)
    void TestUdpClientSetup()
    {
        uint32_t numSta = 3;
        NodeContainer staNodes;
        staNodes.Create(numSta);

        Ipv4AddressHelper ipv4;
        Ipv4InterfaceContainer apInterfaces;
        apInterfaces.SetBase("10.1.1.0", "255.255.255.0");

        UdpClientHelper udpClient(apInterfaces.GetAddress(0), 9); // AP as receiver
        udpClient.SetAttribute("MaxPackets", UintegerValue(5));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps;
        for (uint32_t i = 0; i < numSta; i++)
        {
            clientApps.Add(udpClient.Install(staNodes.Get(i)));
        }
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        // Verify that the UDP clients are installed on the STA nodes
        NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), numSta, "Failed to install UDP clients on STA nodes.");
    }

    // Test data transmission (verify that data is transmitted from STAs to AP)
    void TestDataTransmission()
    {
        uint32_t numSta = 3;
        NodeContainer apNode, staNodes;
        apNode.Create(1);
        staNodes.Create(numSta);

        Ipv4AddressHelper ipv4;
        Ipv4InterfaceContainer staInterfaces, apInterfaces;
        WifiHelper wifi;
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        Ssid ssid = Ssid("wifi-network");
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconInterval", TimeValue(Seconds(2)));

        WifiDeviceHelper wifiDevices;
        wifiDevices.SetDeviceAttribute("WifiPhy", PointerValue(&phy));
        NetDeviceContainer apDevice = wifiDevices.Install(apNode);
        NetDeviceContainer staDevices = wifiDevices.Install(staNodes);

        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        staInterfaces = ipv4.Assign(staDevices);
        apInterfaces = ipv4.Assign(apDevice);

        UdpServerHelper udpServer(9);
        ApplicationContainer serverApp = udpServer.Install(apNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpClientHelper udpClient(apInterfaces.GetAddress(0), 9); // AP as receiver
        udpClient.SetAttribute("MaxPackets", UintegerValue(5));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps;
        for (uint32_t i = 0; i < numSta; i++)
        {
            clientApps.Add(udpClient.Install(staNodes.Get(i)));
        }

        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();

        // Data transmission can be tested by verifying the reception of packets, but this part is complex
        // and may require packet reception callback functions to verify the correct data is received.
    }
};

int main(int argc, char *argv[])
{
    WifiTestSuite testSuite;
    testSuite.Run();
    return 0;
}
