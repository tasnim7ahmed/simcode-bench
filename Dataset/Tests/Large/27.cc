#include "ns3/test.h"
#include "ns3/simulator.h"
#include "ns3/node-container.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/on-off-helper.h"

using namespace ns3;

class WifiThroughputTest : public TestCase {
public:
    WifiThroughputTest() : TestCase("Verify Wi-Fi throughput") {}

    void DoRun() override {
        NodeContainer wifiStaNode, wifiApNode;
        wifiStaNode.Create(1);
        wifiApNode.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy;
        phy.SetChannel(channel.Create());
        
        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n);
        
        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-80211n");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer staDevice = wifi.Install(phy, mac, wifiStaNode);
        
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);
        
        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
        positionAlloc->Add(Vector(0.0, 0.0, 0.0));
        positionAlloc->Add(Vector(1.0, 0.0, 0.0));
        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiApNode);
        mobility.Install(wifiStaNode);
        
        InternetStackHelper stack;
        stack.Install(wifiApNode);
        stack.Install(wifiStaNode);
        
        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer staNodeInterface = address.Assign(staDevice);
        Ipv4InterfaceContainer apNodeInterface = address.Assign(apDevice);
        
        uint16_t port = 9;
        UdpServerHelper server(port);
        ApplicationContainer serverApp = server.Install(wifiStaNode.Get(0));
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(10.0));

        UdpClientHelper client(staNodeInterface.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(4294967295U));
        client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        client.SetAttribute("PacketSize", UintegerValue(1472));
        ApplicationContainer clientApp = client.Install(wifiApNode.Get(0));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(10.0));

        Simulator::Stop(Seconds(11.0));
        Simulator::Run();

        uint32_t rxBytes = 1472 * DynamicCast<UdpServer>(serverApp.Get(0))->GetReceived();
        double throughput = (rxBytes * 8) / 10e6;
        NS_TEST_ASSERT_MSG_GT(throughput, 0, "Throughput should be greater than 0");
        
        Simulator::Destroy();
    }
};

static WifiThroughputTest g_wifiThroughputTest;

class MobilityModelTest : public TestCase {
public:
    MobilityModelTest() : TestCase("Verify Mobility Model") {}

    void DoRun() override {
        NodeContainer nodes;
        nodes.Create(2);
        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
        positionAlloc->Add(Vector(0.0, 0.0, 0.0));
        positionAlloc->Add(Vector(1.0, 0.0, 0.0));
        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(nodes);

        Ptr<MobilityModel> model1 = nodes.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> model2 = nodes.Get(1)->GetObject<MobilityModel>();

        NS_TEST_ASSERT_MSG_EQ_TOL(model1->GetPosition().x, 0.0, 0.01, "Node 1 should be at x=0.0");
        NS_TEST_ASSERT_MSG_EQ_TOL(model2->GetPosition().x, 1.0, 0.01, "Node 2 should be at x=1.0");
    }
};

static MobilityModelTest g_mobilityModelTest;