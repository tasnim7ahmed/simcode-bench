#include "ns3/test.h"
#include "ns3/wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/socket.h"

using namespace ns3;

class WifiInfraTest : public TestCase {
public:
    WifiInfraTest() : TestCase("Test WiFi Infrastructure Setup") {}
    virtual void DoRun() {
        NodeContainer c;
        c.Create(2);

        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiHelper wifi;
        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("wifi-test");

        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer staDevice = wifi.Install(wifiPhy, wifiMac, c.Get(0));

        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, c.Get(1));
        NetDeviceContainer devices = staDevice;
        devices.Add(apDevice);

        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
        positionAlloc->Add(Vector(0.0, 0.0, 0.0));
        positionAlloc->Add(Vector(5.0, 0.0, 0.0));
        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(c);

        InternetStackHelper internet;
        internet.Install(c);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer i = ipv4.Assign(devices);

        NS_TEST_ASSERT_MSG_EQ(i.GetAddress(0).IsEqual("10.1.1.1"), true, "IP assignment failed");
        NS_TEST_ASSERT_MSG_EQ(i.GetAddress(1).IsEqual("10.1.1.2"), true, "IP assignment failed");
    }
};

class PacketReceptionTest : public TestCase {
public:
    PacketReceptionTest() : TestCase("Test Packet Reception") {}
    virtual void DoRun() {
        NodeContainer nodes;
        nodes.Create(2);

        Ptr<Socket> receiver = Socket::CreateSocket(nodes.Get(0), TypeId::LookupByName("ns3::UdpSocketFactory"));
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 80);
        receiver->Bind(local);
        
        Ptr<Socket> sender = Socket::CreateSocket(nodes.Get(1), TypeId::LookupByName("ns3::UdpSocketFactory"));
        InetSocketAddress remote = InetSocketAddress(Ipv4Address("10.1.1.1"), 80);
        sender->Connect(remote);
        
        sender->Send(Create<Packet>(1000));
        
        Simulator::Run();
        Simulator::Destroy();
        
        NS_TEST_ASSERT_MSG_EQ(receiver->GetRxAvailable(), 1, "Packet was not received correctly");
    }
};

static WifiInfraTest wifiInfraTest;
static PacketReceptionTest packetReceptionTest;
