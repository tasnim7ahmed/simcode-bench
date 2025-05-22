#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpUAVNetworkTestSuite : public TestCase
{
public:
    UdpUAVNetworkTestSuite() : TestCase("Test UDP UAV Network") {}
    virtual ~UdpUAVNetworkTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestWifiConfiguration();
        TestMobilitySetup();
        TestRoutingSetup();
        TestIpAddressAssignment();
        TestSocketSetup();
        TestPacketTransmission();
    }

private:
    // Test the creation of UAV nodes and the GCS node
    void TestNodeCreation()
    {
        int numUAVs = 5;
        NodeContainer uavs;
        uavs.Create(numUAVs);

        NodeContainer gcs;
        gcs.Create(1);

        NS_TEST_ASSERT_MSG_EQ(uavs.GetN(), numUAVs, "UAV node creation failed. Expected 5 UAVs.");
        NS_TEST_ASSERT_MSG_EQ(gcs.GetN(), 1, "GCS node creation failed. Expected 1 GCS.");
    }

    // Test Wi-Fi configuration for UAV network (802.11a)
    void TestWifiConfiguration()
    {
        int numUAVs = 5;
        NodeContainer uavs;
        uavs.Create(numUAVs);

        NodeContainer gcs;
        gcs.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211a); // 802.11a for high-speed UAV communication

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer uavDevices = wifi.Install(phy, mac, uavs);
        NetDeviceContainer gcsDevice = wifi.Install(phy, mac, gcs);

        NS_TEST_ASSERT_MSG_EQ(uavDevices.GetN(), numUAVs, "Wi-Fi device configuration failed for UAVs.");
        NS_TEST_ASSERT_MSG_EQ(gcsDevice.GetN(), 1, "Wi-Fi device configuration failed for GCS.");
    }

    // Test mobility setup for UAVs and GCS with 3D mobility model
    void TestMobilitySetup()
    {
        int numUAVs = 5;
        NodeContainer uavs;
        uavs.Create(numUAVs);

        NodeContainer gcs;
        gcs.Create(1);

        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

        // UAVs with different altitudes
        for (int i = 0; i < numUAVs; i++) {
            positionAlloc->Add(Vector(i * 50.0, i * 50.0, 100.0 + i * 10.0));
        }
        positionAlloc->Add(Vector(250.0, 250.0, 0.0)); // GCS at ground level

        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                                  "Speed", StringValue("ns3::UniformRandomVariable[Min=10.0|Max=20.0]"),
                                  "Pause", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                                  "PositionAllocator", PointerValue(positionAlloc));
        mobility.Install(uavs);
        mobility.Install(gcs);

        Ptr<MobilityModel> uavMobility = uavs.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> gcsMobility = gcs.Get(0)->GetObject<MobilityModel>();

        NS_TEST_ASSERT_MSG_NOT_NULL(uavMobility, "Mobility model installation failed for UAVs.");
        NS_TEST_ASSERT_MSG_NOT_NULL(gcsMobility, "Mobility model installation failed for GCS.");
    }

    // Test AODV routing setup
    void TestRoutingSetup()
    {
        int numUAVs = 5;
        NodeContainer uavs;
        uavs.Create(numUAVs);

        NodeContainer gcs;
        gcs.Create(1);

        AodvHelper aodv;
        InternetStackHelper internet;
        internet.SetRoutingHelper(aodv);
        internet.Install(uavs);
        internet.Install(gcs);

        Ptr<Ipv4RoutingProtocol> routingProtocol = uavs.Get(0)->GetObject<Ipv4RoutingProtocol>();
        NS_TEST_ASSERT_MSG_NOT_NULL(routingProtocol, "AODV routing setup failed for UAVs.");
    }

    // Test IP address assignment
    void TestIpAddressAssignment()
    {
        int numUAVs = 5;
        NodeContainer uavs;
        uavs.Create(numUAVs);

        NodeContainer gcs;
        gcs.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211a); // 802.11a for high-speed UAV communication

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer uavDevices = wifi.Install(phy, mac, uavs);
        NetDeviceContainer gcsDevice = wifi.Install(phy, mac, gcs);

        Ipv4AddressHelper address;
        address.SetBase("10.1.4.0", "255.255.255.0");
        Ipv4InterfaceContainer uavInterfaces = address.Assign(uavDevices);
        Ipv4InterfaceContainer gcsInterface = address.Assign(gcsDevice);

        NS_TEST_ASSERT_MSG_EQ(uavInterfaces.GetN(), numUAVs, "IP address assignment failed for UAVs.");
        NS_TEST_ASSERT_MSG_EQ(gcsInterface.GetN(), 1, "IP address assignment failed for GCS.");
    }

    // Test UDP socket setup for UAVs and GCS
    void TestSocketSetup()
    {
        int numUAVs = 5;
        NodeContainer uavs;
        uavs.Create(numUAVs);

        NodeContainer gcs;
        gcs.Create(1);

        uint16_t port = 7070;

        Ptr<Socket> gcsSocket = Socket::CreateSocket(gcs.Get(0), UdpSocketFactory::GetTypeId());
        gcsSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
        gcsSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

        for (int i = 0; i < numUAVs; i++) {
            Ptr<Socket> uavSocket = Socket::CreateSocket(uavs.Get(i), UdpSocketFactory::GetTypeId());
            Simulator::Schedule(Seconds(2.0 + i * 1.0), &SendPacket, uavSocket, gcsInterface.GetAddress(0), port);

            NS_TEST_ASSERT_MSG_NOT_NULL(gcsSocket, "Socket creation failed for GCS.");
            NS_TEST_ASSERT_MSG_NOT_NULL(uavSocket, "Socket creation failed for UAV.");
        }
    }

    // Test packet transmission from UAVs to GCS
    void TestPacketTransmission()
    {
        int numUAVs = 5;
        double simTime = 60.0;
        NodeContainer uavs;
        uavs.Create(numUAVs);

        NodeContainer gcs;
        gcs.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211a); // 802.11a for high-speed UAV communication

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer uavDevices = wifi.Install(phy, mac, uavs);
        NetDeviceContainer gcsDevice = wifi.Install(phy, mac, gcs);

        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

        for (int i = 0; i < numUAVs; i++) {
            positionAlloc->Add(Vector(i * 50.0, i * 50.0, 100.0 + i * 10.0));
        }
        positionAlloc->Add(Vector(250.0, 250.0, 0.0)); // GCS at ground level

        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel");
        mobility.Install(uavs);
        mobility.Install(gcs);

        Ipv4AddressHelper address;
        address.SetBase("10.1.4.0", "255.255.255.0");
        Ipv4InterfaceContainer uavInterfaces = address.Assign(uavDevices);
        Ipv4InterfaceContainer gcsInterface = address.Assign(gcsDevice);

        uint16_t port = 7070;

        Ptr<Socket> gcsSocket = Socket::CreateSocket(gcs.Get(0), UdpSocketFactory::GetTypeId());
        gcsSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
        gcsSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

        for (int i = 0; i < numUAVs; i++) {
            Ptr<Socket> uavSocket = Socket::CreateSocket(uavs.Get(i), UdpSocketFactory::GetTypeId());
            Simulator::Schedule(Seconds(2.0 + i * 1.0), &SendPacket, uavSocket, gcsInterface.GetAddress(0), port);
        }

        Simulator::Stop(Seconds(simTime));
        Simulator::Run();
        Simulator::Destroy();
        
        NS_LOG_INFO("TestPacketTransmission completed.");
    }
};

static UdpUAVNetworkTestSuite udpUAVNetworkTestSuite;

