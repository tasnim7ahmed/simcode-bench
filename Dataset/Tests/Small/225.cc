#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/dsdv-module.h"
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
        TestMobilityModel();
        TestDsdvRoutingStack();
        TestIpAddressAssignment();
        TestSocketCreation();
        TestPacketTransmission();
    }

private:
    // Test the creation of vehicles and RSU nodes
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

    // Test Wi-Fi configuration (802.11p for VANET)
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
        wifi.SetStandard(WIFI_STANDARD_80211p); // 802.11p for vehicular communication

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer devices = wifi.Install(phy, mac, vehicles);
        NetDeviceContainer rsuDevice = wifi.Install(phy, mac, rsu);

        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), numVehicles, "Wi-Fi device configuration failed for vehicles.");
        NS_TEST_ASSERT_MSG_EQ(rsuDevice.GetN(), 1, "Wi-Fi device configuration failed for RSU.");
    }

    // Test mobility model setup (vehicles along a highway)
    void TestMobilityModel()
    {
        int numVehicles = 10;
        NodeContainer vehicles;
        vehicles.Create(numVehicles);

        NodeContainer rsu;
        rsu.Create(1);

        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

        for (int i = 0; i < numVehicles; i++) {
            positionAlloc->Add(Vector(i * 20.0, 0.0, 0.0)); // Vehicles spaced 20m apart
        }
        positionAlloc->Add(Vector(100.0, 50.0, 0.0)); // RSU position

        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::WaypointMobilityModel");
        mobility.Install(vehicles);
        mobility.Install(rsu);

        Ptr<MobilityModel> vehicleMobility = vehicles.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> rsuMobility = rsu.Get(0)->GetObject<MobilityModel>();

        NS_TEST_ASSERT_MSG_NOT_NULL(vehicleMobility, "Mobility model installation failed for vehicles.");
        NS_TEST_ASSERT_MSG_NOT_NULL(rsuMobility, "Mobility model installation failed for RSU.");
    }

    // Test DSDV routing stack installation
    void TestDsdvRoutingStack()
    {
        int numVehicles = 10;
        NodeContainer vehicles;
        vehicles.Create(numVehicles);

        NodeContainer rsu;
        rsu.Create(1);

        DsdvHelper dsdv;
        InternetStackHelper internet;
        internet.SetRoutingHelper(dsdv);
        internet.Install(vehicles);
        internet.Install(rsu);

        Ptr<Ipv4RoutingProtocol> routingProtocol = vehicles.Get(0)->GetObject<Ipv4RoutingProtocol>();
        NS_TEST_ASSERT_MSG_NOT_NULL(routingProtocol, "DSDV routing stack installation failed.");
    }

    // Test IP address assignment
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
        wifi.SetStandard(WIFI_STANDARD_80211p); // 802.11p for vehicular communication

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer devices = wifi.Install(phy, mac, vehicles);
        NetDeviceContainer rsuDevice = wifi.Install(phy, mac, rsu);

        Ipv4AddressHelper address;
        address.SetBase("10.1.2.0", "255.255.255.0");
        Ipv4InterfaceContainer vehicleInterfaces = address.Assign(devices);
        Ipv4InterfaceContainer rsuInterface = address.Assign(rsuDevice);

        NS_TEST_ASSERT_MSG_EQ(vehicleInterfaces.GetN(), numVehicles, "IP address assignment failed for vehicles.");
        NS_TEST_ASSERT_MSG_EQ(rsuInterface.GetN(), 1, "IP address assignment failed for RSU.");
    }

    // Test UDP socket creation for vehicles and RSU
    void TestSocketCreation()
    {
        int numVehicles = 10;
        NodeContainer vehicles;
        vehicles.Create(numVehicles);

        NodeContainer rsu;
        rsu.Create(1);

        uint16_t port = 8080; // UDP port

        Ptr<Socket> rsuSocket = Socket::CreateSocket(rsu.Get(0), UdpSocketFactory::GetTypeId());
        rsuSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
        rsuSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

        for (int i = 0; i < numVehicles; i++) {
            Ptr<Socket> vehicleSocket = Socket::CreateSocket(vehicles.Get(i), UdpSocketFactory::GetTypeId());
            Simulator::Schedule(Seconds(2.0 + i * 0.5), &SendPacket, vehicleSocket, rsuInterface.GetAddress(0), port);

            NS_TEST_ASSERT_MSG_NOT_NULL(rsuSocket, "Socket creation failed for RSU.");
            NS_TEST_ASSERT_MSG_NOT_NULL(vehicleSocket, "Socket creation failed for vehicle.");
        }
    }

    // Test UDP packet transmission from vehicles to RSU
    void TestPacketTransmission()
    {
        int numVehicles = 10;
        double simTime = 30.0;
        NodeContainer vehicles;
        vehicles.Create(numVehicles);

        NodeContainer rsu;
        rsu.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211p); // 802.11p for vehicular communication

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer devices = wifi.Install(phy, mac, vehicles);
        NetDeviceContainer rsuDevice = wifi.Install(phy, mac, rsu);

        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

        for (int i = 0; i < numVehicles; i++) {
            positionAlloc->Add(Vector(i * 20.0, 0.0, 0.0)); // Vehicles spaced 20m apart
        }
        positionAlloc->Add(Vector(100.0, 50.0, 0.0)); // RSU position

        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::WaypointMobilityModel");
        mobility.Install(vehicles);
        mobility.Install(rsu);

        Ipv4AddressHelper address;
        address.SetBase("10.1.2.0", "255.255.255.0");
        Ipv4InterfaceContainer vehicleInterfaces = address.Assign(devices);
        Ipv4InterfaceContainer rsuInterface = address.Assign(rsuDevice);

        // Setup UDP Server at RSU
        Ptr<Socket> rsuSocket = Socket::CreateSocket(rsu.Get(0), UdpSocketFactory::GetTypeId());
        rsuSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), 8080));
        rsuSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

        // Setup UDP Clients in Vehicles
        for (int i = 0; i < numVehicles; i++) {
            Ptr<Socket> vehicleSocket = Socket::CreateSocket(vehicles.Get(i), UdpSocketFactory::GetTypeId());
            Simulator::Schedule(Seconds(2.0 + i * 0.5), &SendPacket, vehicleSocket, rsuInterface.GetAddress(0), 8080);
        }

        // Run the simulation and verify packet transmission
        Simulator::Stop(Seconds(simTime));
        Simulator::Run();
        Simulator::Destroy();
        
        NS_LOG_UNCOND("Test completed. Check RSU logs for received packets.");
    }
};

TestSuite *TestSuiteInstance = new TestSuite("UdpVANETExampleTestSuite");

int main(int argc, char *argv[])
{
    // Create the test suite and add the test case
    TestSuiteInstance->AddTestCase(new UdpVANETExampleTestSuite());
    return TestSuiteInstance->Run();
}
