#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/dsr-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpVANETExampleTestSuite : public TestCase
{
public:
    UdpVANETExampleTestSuite() : TestCase("Test UDP VANET Example") {}
    virtual ~UdpVANETExampleTestSuite() {}

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
    // Test the creation of vehicle and RSU nodes
    void TestNodeCreation()
    {
        int numVehicles = 10;
        NodeContainer vehicles;
        vehicles.Create(numVehicles);

        NodeContainer rsu;
        rsu.Create(1);

        NS_TEST_ASSERT_MSG_EQ(vehicles.GetN(), numVehicles, "Vehicle node creation failed. Expected 10 vehicles.");
        NS_TEST_ASSERT_MSG_EQ(rsu.GetN(), 1, "RSU node creation failed. Expected 1 RSU.");
    }

    // Test Wi-Fi configuration for VANET (802.11p)
    void TestWifiConfiguration()
    {
        int numVehicles = 10;
        NodeContainer vehicles;
        vehicles.Create(numVehicles);

        NodeContainer rsu;
        rsu.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211p); // 802.11p for VANET communication

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer vehicleDevices = wifi.Install(phy, mac, vehicles);
        NetDeviceContainer rsuDevice = wifi.Install(phy, mac, rsu);

        NS_TEST_ASSERT_MSG_EQ(vehicleDevices.GetN(), numVehicles, "Wi-Fi device configuration failed for vehicles.");
        NS_TEST_ASSERT_MSG_EQ(rsuDevice.GetN(), 1, "Wi-Fi device configuration failed for RSU.");
    }

    // Test mobility setup for vehicles (GaussMarkov model) and RSU (constant position)
    void TestMobilitySetup()
    {
        int numVehicles = 10;
        NodeContainer vehicles;
        vehicles.Create(numVehicles);

        NodeContainer rsu;
        rsu.Create(1);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::GaussMarkovMobilityModel",
                                  "Bounds", BoxValue(Box(0, 500, 0, 500, 0, 0)),
                                  "TimeStep", TimeValue(Seconds(1.0)),
                                  "Alpha", DoubleValue(0.85),
                                  "MeanVelocity", StringValue("ns3::UniformRandomVariable[Min=10.0|Max=20.0]"),
                                  "MeanDirection", StringValue("ns3::UniformRandomVariable[Min=0|Max=360]"),
                                  "MeanPitch", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

        mobility.Install(vehicles);

        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
        positionAlloc->Add(Vector(250.0, 250.0, 0.0)); // RSU fixed at center

        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(rsu);

        Ptr<MobilityModel> vehicleMobility = vehicles.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> rsuMobility = rsu.Get(0)->GetObject<MobilityModel>();

        NS_TEST_ASSERT_MSG_NOT_NULL(vehicleMobility, "Mobility model installation failed for vehicles.");
        NS_TEST_ASSERT_MSG_NOT_NULL(rsuMobility, "Mobility model installation failed for RSU.");
    }

    // Test DSR routing setup
    void TestRoutingSetup()
    {
        int numVehicles = 10;
        NodeContainer vehicles;
        vehicles.Create(numVehicles);

        NodeContainer rsu;
        rsu.Create(1);

        DsrHelper dsr;
        InternetStackHelper internet;
        internet.SetRoutingHelper(dsr);
        internet.Install(vehicles);
        internet.Install(rsu);

        Ptr<Ipv4RoutingProtocol> routingProtocol = vehicles.Get(0)->GetObject<Ipv4RoutingProtocol>();
        NS_TEST_ASSERT_MSG_NOT_NULL(routingProtocol, "DSR routing setup failed for vehicles.");
    }

    // Test IP address assignment for vehicles and RSU
    void TestIpAddressAssignment()
    {
        int numVehicles = 10;
        NodeContainer vehicles;
        vehicles.Create(numVehicles);

        NodeContainer rsu;
        rsu.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211p); // 802.11p for VANET communication

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer vehicleDevices = wifi.Install(phy, mac, vehicles);
        NetDeviceContainer rsuDevice = wifi.Install(phy, mac, rsu);

        Ipv4AddressHelper address;
        address.SetBase("10.1.5.0", "255.255.255.0");
        Ipv4InterfaceContainer vehicleInterfaces = address.Assign(vehicleDevices);
        Ipv4InterfaceContainer rsuInterface = address.Assign(rsuDevice);

        NS_TEST_ASSERT_MSG_EQ(vehicleInterfaces.GetN(), numVehicles, "IP address assignment failed for vehicles.");
        NS_TEST_ASSERT_MSG_EQ(rsuInterface.GetN(), 1, "IP address assignment failed for RSU.");
    }

    // Test UDP socket setup for vehicles and RSU
    void TestSocketSetup()
    {
        int numVehicles = 10;
        NodeContainer vehicles;
        vehicles.Create(numVehicles);

        NodeContainer rsu;
        rsu.Create(1);

        uint16_t port = 8080;

        Ptr<Socket> rsuSocket = Socket::CreateSocket(rsu.Get(0), UdpSocketFactory::GetTypeId());
        rsuSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
        rsuSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

        for (int i = 0; i < numVehicles; i++) {
            Ptr<Socket> vehicleSocket = Socket::CreateSocket(vehicles.Get(i), UdpSocketFactory::GetTypeId());
            Simulator::Schedule(Seconds(2.0 + i * 1.0), &SendPacket, vehicleSocket, rsuInterface.GetAddress(0), port);

            NS_TEST_ASSERT_MSG_NOT_NULL(rsuSocket, "Socket creation failed for RSU.");
            NS_TEST_ASSERT_MSG_NOT_NULL(vehicleSocket, "Socket creation failed for vehicle.");
        }
    }

    // Test packet transmission from vehicles to RSU
    void TestPacketTransmission()
    {
        int numVehicles = 10;
        double simTime = 80.0;
        NodeContainer vehicles;
        vehicles.Create(numVehicles);

        NodeContainer rsu;
        rsu.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211p); // 802.11p for VANET communication

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer vehicleDevices = wifi.Install(phy, mac, vehicles);
        NetDeviceContainer rsuDevice = wifi.Install(phy, mac, rsu);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::GaussMarkovMobilityModel",
                                  "Bounds", BoxValue(Box(0, 500, 0, 500, 0, 0)),
                                  "TimeStep", TimeValue(Seconds(1.0)),
                                  "Alpha", DoubleValue(0.85),
                                  "MeanVelocity", StringValue("ns3::UniformRandomVariable[Min=10.0|Max=20.0]"),
                                  "MeanDirection", StringValue("ns3::UniformRandomVariable[Min=0|Max=360]"),
                                  "MeanPitch", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

        mobility.Install(vehicles);

        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
        positionAlloc->Add(Vector(250.0, 250.0, 0.0)); // RSU fixed at center

        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(rsu);

        Ipv4AddressHelper address;
        address.SetBase("10.1.5.0", "255.255.255.0");
        Ipv4InterfaceContainer vehicleInterfaces = address.Assign(vehicleDevices);
        Ipv4InterfaceContainer rsuInterface = address.Assign(rsuDevice);

        uint16_t port = 8080;

        Ptr<Socket> rsuSocket = Socket::CreateSocket(rsu.Get(0), UdpSocketFactory::GetTypeId());
        rsuSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
        rsuSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

        for (int i = 0; i < numVehicles; i++) {
            Ptr<Socket> vehicleSocket = Socket::CreateSocket(vehicles.Get(i), UdpSocketFactory::GetTypeId());
            Simulator::Schedule(Seconds(2.0 + i * 1.0), &SendPacket, vehicleSocket, rsuInterface.GetAddress(0), port);
        }

        Simulator::Stop(Seconds(simTime));
        Simulator::Run();
        Simulator::Destroy();
        
        NS_LOG_INFO("TestPacketTransmission completed.");
    }
};

static UdpVANETExampleTestSuite udpVANETExampleTestSuite;
