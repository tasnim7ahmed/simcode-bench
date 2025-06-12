#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-module.h"
#include "ns3/energy-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpWSNExampleTestSuite : public TestCase
{
public:
    UdpWSNExampleTestSuite() : TestCase("Test UDP WSN Example") {}
    virtual ~UdpWSNExampleTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestWifiConfiguration();
        TestMobilitySetup();
        TestEnergyModelSetup();
        TestOlsrRoutingStack();
        TestIpAddressAssignment();
        TestSocketSetup();
        TestPacketTransmission();
    }

private:
    // Test the creation of sensor nodes and sink node
    void TestNodeCreation()
    {
        int numSensors = 20;
        NodeContainer sensors;
        sensors.Create(numSensors);

        NodeContainer sink;
        sink.Create(1);

        NS_TEST_ASSERT_MSG_EQ(sensors.GetN(), numSensors, "Sensor node creation failed. Expected 20 sensors.");
        NS_TEST_ASSERT_MSG_EQ(sink.GetN(), 1, "Sink node creation failed. Expected 1 sink.");
    }

    // Test Wi-Fi configuration (802.11b for low-power sensor communication)
    void TestWifiConfiguration()
    {
        int numSensors = 20;
        NodeContainer sensors;
        sensors.Create(numSensors);

        NodeContainer sink;
        sink.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b); // 802.11b for low-power sensor communication

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer sensorDevices = wifi.Install(phy, mac, sensors);
        NetDeviceContainer sinkDevice = wifi.Install(phy, mac, sink);

        NS_TEST_ASSERT_MSG_EQ(sensorDevices.GetN(), numSensors, "Wi-Fi device configuration failed for sensors.");
        NS_TEST_ASSERT_MSG_EQ(sinkDevice.GetN(), 1, "Wi-Fi device configuration failed for sink.");
    }

    // Test mobility setup (random sensor deployment, sink at the center)
    void TestMobilitySetup()
    {
        int numSensors = 20;
        NodeContainer sensors;
        sensors.Create(numSensors);

        NodeContainer sink;
        sink.Create(1);

        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

        // Random deployment in 100x100m area for sensors
        for (int i = 0; i < numSensors; i++) {
            positionAlloc->Add(Vector(rand() % 100, rand() % 100, 0.0));
        }
        positionAlloc->Add(Vector(50.0, 50.0, 0.0)); // Sink at the center

        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(sensors);
        mobility.Install(sink);

        Ptr<MobilityModel> sensorMobility = sensors.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> sinkMobility = sink.Get(0)->GetObject<MobilityModel>();

        NS_TEST_ASSERT_MSG_NOT_NULL(sensorMobility, "Mobility model installation failed for sensors.");
        NS_TEST_ASSERT_MSG_NOT_NULL(sinkMobility, "Mobility model installation failed for sink.");
    }

    // Test energy model setup for sensors
    void TestEnergyModelSetup()
    {
        int numSensors = 20;
        NodeContainer sensors;
        sensors.Create(numSensors);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b); // 802.11b for low-power sensor communication

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer sensorDevices = wifi.Install(phy, mac, sensors);

        // Install Energy Model for sensors
        BasicEnergySourceHelper energySourceHelper;
        energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(10.0)); // 10 Joules per node
        EnergySourceContainer energySources = energySourceHelper.Install(sensors);

        WifiRadioEnergyModelHelper radioEnergyHelper;
        radioEnergyHelper.Install(sensorDevices, energySources);

        NS_TEST_ASSERT_MSG_EQ(energySources.GetN(), numSensors, "Energy model installation failed for sensors.");
    }

    // Test OLSR routing stack installation
    void TestOlsrRoutingStack()
    {
        int numSensors = 20;
        NodeContainer sensors;
        sensors.Create(numSensors);

        NodeContainer sink;
        sink.Create(1);

        OlsrHelper olsr;
        InternetStackHelper internet;
        internet.SetRoutingHelper(olsr);
        internet.Install(sensors);
        internet.Install(sink);

        Ptr<Ipv4RoutingProtocol> routingProtocol = sensors.Get(0)->GetObject<Ipv4RoutingProtocol>();
        NS_TEST_ASSERT_MSG_NOT_NULL(routingProtocol, "OLSR routing stack installation failed.");
    }

    // Test IP address assignment
    void TestIpAddressAssignment()
    {
        int numSensors = 20;
        NodeContainer sensors;
        sensors.Create(numSensors);

        NodeContainer sink;
        sink.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b); // 802.11b for low-power sensor communication

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer sensorDevices = wifi.Install(phy, mac, sensors);
        NetDeviceContainer sinkDevice = wifi.Install(phy, mac, sink);

        Ipv4AddressHelper address;
        address.SetBase("10.1.3.0", "255.255.255.0");
        Ipv4InterfaceContainer sensorInterfaces = address.Assign(sensorDevices);
        Ipv4InterfaceContainer sinkInterface = address.Assign(sinkDevice);

        NS_TEST_ASSERT_MSG_EQ(sensorInterfaces.GetN(), numSensors, "IP address assignment failed for sensors.");
        NS_TEST_ASSERT_MSG_EQ(sinkInterface.GetN(), 1, "IP address assignment failed for sink.");
    }

    // Test UDP socket setup for sink and sensors
    void TestSocketSetup()
    {
        int numSensors = 20;
        NodeContainer sensors;
        sensors.Create(numSensors);

        NodeContainer sink;
        sink.Create(1);

        uint16_t port = 9090; // UDP port

        Ptr<Socket> sinkSocket = Socket::CreateSocket(sink.Get(0), UdpSocketFactory::GetTypeId());
        sinkSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
        sinkSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

        for (int i = 0; i < numSensors; i++) {
            Ptr<Socket> sensorSocket = Socket::CreateSocket(sensors.Get(i), UdpSocketFactory::GetTypeId());
            Simulator::Schedule(Seconds(2.0 + i * 0.5), &SendPacket, sensorSocket, sinkInterface.GetAddress(0), port);

            NS_TEST_ASSERT_MSG_NOT_NULL(sinkSocket, "Socket creation failed for sink.");
            NS_TEST_ASSERT_MSG_NOT_NULL(sensorSocket, "Socket creation failed for sensor.");
        }
    }

    // Test packet transmission from sensors to sink
    void TestPacketTransmission()
    {
        int numSensors = 20;
        double simTime = 50.0;
        NodeContainer sensors;
        sensors.Create(numSensors);

        NodeContainer sink;
        sink.Create(1);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b); // 802.11b for low-power sensor communication

        WifiMacHelper mac;
        mac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer sensorDevices = wifi.Install(phy, mac, sensors);
        NetDeviceContainer sinkDevice = wifi.Install(phy, mac, sink);

        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

        for (int i = 0; i < numSensors; i++) {
            positionAlloc->Add(Vector(rand() % 100, rand() % 100, 0.0));
        }
        positionAlloc->Add(Vector(50.0, 50.0, 0.0)); // Sink at the center

        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(sensors);
        mobility.Install(sink);

        Ipv4AddressHelper address;
        address.SetBase("10.1.3.0", "255.255.255.0");
        Ipv4InterfaceContainer sensorInterfaces = address.Assign(sensorDevices);
        Ipv4InterfaceContainer sinkInterface = address.Assign(sinkDevice);

        // Setup UDP Server at Sink Node
        Ptr<Socket> sinkSocket = Socket::CreateSocket(sink.Get(0), UdpSocketFactory::GetTypeId());
        sinkSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), 9090));
        sinkSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

        // Setup UDP Clients in Sensor Nodes
        for (int i = 0; i < numSensors; i++) {
            Ptr<Socket> sensorSocket = Socket::CreateSocket(sensors.Get(i), UdpSocketFactory::GetTypeId());
            Simulator::Schedule(Seconds(2.0 + i * 0.5), &SendPacket, sensorSocket, sinkInterface.GetAddress(0), 9090);
        }

        // Run the simulation and verify packet transmission
        Simulator::Stop(Seconds(simTime));
        Simulator::Run();
        Simulator::Destroy();

        // For actual testing, you may want to check packet counts or other behaviors
        NS_LOG_INFO("TestPacketTransmission completed.");
    }
};

static UdpWSNExampleTestSuite udpWSNTestSuite;
