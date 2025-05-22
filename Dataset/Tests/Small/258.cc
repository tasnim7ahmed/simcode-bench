#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class WaveTestSuite : public TestCase
{
public:
    WaveTestSuite() : TestCase("Test VANET Echo Application Setup") {}
    virtual ~WaveTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestMobilityConfiguration();
        TestWaveDeviceInstallation();
        TestIpAddressAssignment();
        TestApplicationSetup();
    }

private:
    // Test the creation of vehicle nodes
    void TestNodeCreation()
    {
        NodeContainer vehicles;
        vehicles.Create(3); // Create 3 vehicles (nodes)

        // Ensure that the correct number of nodes are created
        NS_TEST_ASSERT_MSG_EQ(vehicles.GetN(), 3, "Failed to create 3 vehicle nodes.");
    }

    // Test the mobility configuration of the vehicles
    void TestMobilityConfiguration()
    {
        NodeContainer vehicles;
        vehicles.Create(3);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
        mobility.Install(vehicles);

        vehicles.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0, 0, 0));
        vehicles.Get(1)->GetObject<MobilityModel>()->SetPosition(Vector(50, 0, 0));
        vehicles.Get(2)->GetObject<MobilityModel>()->SetPosition(Vector(100, 0, 0));

        vehicles.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(10, 0, 0));
        vehicles.Get(1)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(15, 0, 0));
        vehicles.Get(2)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(20, 0, 0));

        // Verify positions and velocities of vehicles
        Ptr<MobilityModel> mobilityModel1 = vehicles.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> mobilityModel2 = vehicles.Get(1)->GetObject<MobilityModel>();
        Ptr<MobilityModel> mobilityModel3 = vehicles.Get(2)->GetObject<MobilityModel>();

        NS_TEST_ASSERT_MSG_EQ(mobilityModel1->GetPosition(), Vector(0, 0, 0), "Failed to set initial position for vehicle 1.");
        NS_TEST_ASSERT_MSG_EQ(mobilityModel2->GetPosition(), Vector(50, 0, 0), "Failed to set initial position for vehicle 2.");
        NS_TEST_ASSERT_MSG_EQ(mobilityModel3->GetPosition(), Vector(100, 0, 0), "Failed to set initial position for vehicle 3.");
        
        NS_TEST_ASSERT_MSG_EQ(vehicles.Get(0)->GetObject<ConstantVelocityMobilityModel>()->GetVelocity(), Vector(10, 0, 0), "Failed to set velocity for vehicle 1.");
        NS_TEST_ASSERT_MSG_EQ(vehicles.Get(1)->GetObject<ConstantVelocityMobilityModel>()->GetVelocity(), Vector(15, 0, 0), "Failed to set velocity for vehicle 2.");
        NS_TEST_ASSERT_MSG_EQ(vehicles.Get(2)->GetObject<ConstantVelocityMobilityModel>()->GetVelocity(), Vector(20, 0, 0), "Failed to set velocity for vehicle 3.");
    }

    // Test the installation of wave devices on the vehicles
    void TestWaveDeviceInstallation()
    {
        NodeContainer vehicles;
        vehicles.Create(3);

        YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default();
        wavePhy.SetChannel(waveChannel.Create());

        NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
        Wifi80211pHelper waveHelper = Wifi80211pHelper::Default();

        NetDeviceContainer waveDevices = waveHelper.Install(wavePhy, waveMac, vehicles);

        // Ensure wave devices are installed
        NS_TEST_ASSERT_MSG_EQ(waveDevices.GetN(), 3, "Failed to install wave devices on vehicles.");
    }

    // Test the assignment of IP addresses to the vehicles
    void TestIpAddressAssignment()
    {
        NodeContainer vehicles;
        vehicles.Create(3);

        YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default();
        wavePhy.SetChannel(waveChannel.Create());

        NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
        Wifi80211pHelper waveHelper = Wifi80211pHelper::Default();

        NetDeviceContainer waveDevices = waveHelper.Install(wavePhy, waveMac, vehicles);

        InternetStackHelper internet;
        internet.Install(vehicles);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(waveDevices);

        // Ensure IP addresses are assigned
        Ipv4Address ipNode1 = interfaces.GetAddress(0);
        Ipv4Address ipNode2 = interfaces.GetAddress(1);
        Ipv4Address ipNode3 = interfaces.GetAddress(2);

        NS_TEST_ASSERT_MSG_NE(ipNode1, Ipv4Address("0.0.0.0"), "Failed to assign IP address to vehicle 1.");
        NS_TEST_ASSERT_MSG_NE(ipNode2, Ipv4Address("0.0.0.0"), "Failed to assign IP address to vehicle 2.");
        NS_TEST_ASSERT_MSG_NE(ipNode3, Ipv4Address("0.0.0.0"), "Failed to assign IP address to vehicle 3.");
    }

    // Test the setup of UDP Echo Server and Client applications
    void TestApplicationSetup()
    {
        NodeContainer vehicles;
        vehicles.Create(3);

        YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default();
        wavePhy.SetChannel(waveChannel.Create());

        NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
        Wifi80211pHelper waveHelper = Wifi80211pHelper::Default();

        NetDeviceContainer waveDevices = waveHelper.Install(wavePhy, waveMac, vehicles);

        InternetStackHelper internet;
        internet.Install(vehicles);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(waveDevices);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(vehicles.Get(2)); // Last vehicle is the server
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(3));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(vehicles.Get(0)); // First vehicle is the client
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        // Ensure that the server and client applications are correctly installed
        NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "Failed to install UDP Echo Server on vehicle 3.");
        NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "Failed to install UDP Echo Client on vehicle 1.");
    }
};

// Instantiate the test suite
static WaveTestSuite waveTestSuite;
