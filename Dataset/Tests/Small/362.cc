#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for WifiSimpleExample
class WifiSimpleTest : public TestCase
{
public:
    WifiSimpleTest() : TestCase("Test for WifiSimpleExample") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestWifiSetup();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestApplicationSetup();
        TestSimulationExecution();
    }

private:
    uint32_t nNodes = 2;
    NodeContainer wifiNodes;
    NetDeviceContainer staDevice;
    NetDeviceContainer apDevice;
    Ipv4InterfaceContainer interfaces;

    // Test for node creation
    void TestNodeCreation()
    {
        wifiNodes.Create(nNodes);
        
        NS_TEST_ASSERT_MSG_EQ(wifiNodes.GetN(), nNodes, "Failed to create the correct number of nodes");
    }

    // Test for Wi-Fi PHY and MAC layer setup
    void TestWifiSetup()
    {
        WifiHelper wifi;
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        YansWifiChannelHelper channel;
        channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
        channel.AddPropagationLoss("ns3::FriisPropagationLossModel");

        YansWifiPhyHelper phy;
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        mac.SetType("ns3::StaWifiMac");

        staDevice = wifi.Install(phy, mac, wifiNodes.Get(1));

        mac.SetType("ns3::ApWifiMac");
        apDevice = wifi.Install(phy, mac, wifiNodes.Get(0));

        // Verify devices are installed correctly
        NS_TEST_ASSERT_MSG_EQ(staDevice.GetN(), 1, "Failed to install STA device");
        NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "Failed to install AP device");
    }

    // Test for Internet stack installation
    void TestInternetStackInstallation()
    {
        InternetStackHelper stack;
        stack.Install(wifiNodes);

        // Verify that the Internet stack is installed on the nodes
        Ptr<Ipv4> ipv4 = wifiNodes.Get(0)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on node 0");

        ipv4 = wifiNodes.Get(1)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on node 1");
    }

    // Test for IP address assignment
    void TestIpAddressAssignment()
    {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfaces = address.Assign(staDevice);
        address.Assign(apDevice);

        // Verify that the IP addresses are assigned correctly
        NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to STA");
    }

    // Test for application setup (UDP server and client)
    void TestApplicationSetup()
    {
        uint16_t port = 9;

        // Set up UDP server on AP (node 0)
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(wifiNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Set up UDP client on STA (node 1)
        UdpClientHelper udpClient(interfaces.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = udpClient.Install(wifiNodes.Get(1));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(9.0));

        // Verify that the server and client applications are set up
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP server application not installed on node 0");
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP client application not installed on node 1");
    }

    // Test for simulation execution
    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error");
    }
};

// Register the test
static WifiSimpleTest wifiSimpleTestInstance;
