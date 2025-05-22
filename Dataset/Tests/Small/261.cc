#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class WaveTestSuite : public TestCase
{
public:
    WaveTestSuite() : TestCase("Test Wave Network Setup for Vehicles") {}
    virtual ~WaveTestSuite() {}

    void DoRun() override
    {
        TestVehicleCreation();
        TestMobilityModel();
        TestWaveDeviceInstallation();
        TestIpAddressAssignment();
        TestUdpApplicationSetup();
    }

private:
    // Test the creation of vehicles (nodes)
    void TestVehicleCreation()
    {
        NodeContainer vehicles;
        vehicles.Create(3); // Create 3 vehicles

        // Ensure the correct number of vehicles are created
        NS_TEST_ASSERT_MSG_EQ(vehicles.GetN(), 3, "Failed to create 3 vehicles.");
    }

    // Test the mobility configuration of the vehicles (velocity)
    void TestMobilityModel()
    {
        NodeContainer vehicles;
        vehicles.Create(3);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
        mobility.Install(vehicles);

        Ptr<ConstantVelocityMobilityModel> mobility1 = vehicles.Get(0)->GetObject<ConstantVelocityMobilityModel>();
        Ptr<ConstantVelocityMobilityModel> mobility2 = vehicles.Get(1)->GetObject<ConstantVelocityMobilityModel>();
        Ptr<ConstantVelocityMobilityModel> mobility3 = vehicles.Get(2)->GetObject<ConstantVelocityMobilityModel>();

        mobility1->SetVelocity(Vector(20, 0, 0)); // Vehicle 1 moves at 20 m/s
        mobility2->SetVelocity(Vector(15, 0, 0)); // Vehicle 2 moves at 15 m/s
        mobility3->SetVelocity(Vector(25, 0, 0)); // Vehicle 3 moves at 25 m/s

        // Verify that the velocities are set correctly
        NS_TEST_ASSERT_MSG_EQ(mobility1->GetVelocity(), Vector(20, 0, 0), "Failed to set velocity for vehicle 1.");
        NS_TEST_ASSERT_MSG_EQ(mobility2->GetVelocity(), Vector(15, 0, 0), "Failed to set velocity for vehicle 2.");
        NS_TEST_ASSERT_MSG_EQ(mobility3->GetVelocity(), Vector(25, 0, 0), "Failed to set velocity for vehicle 3.");
    }

    // Test the installation of Wave devices on the vehicles
    void TestWaveDeviceInstallation()
    {
        NodeContainer vehicles;
        vehicles.Create(3);

        YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default();
        wavePhy.SetChannel(waveChannel.Create());

        NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
        WaveHelper wave;
        NetDeviceContainer devices = wave.Install(wavePhy, waveMac, vehicles);

        // Ensure Wave devices are installed on all vehicles
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 3, "Failed to install Wave devices on all vehicles.");
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
        WaveHelper wave;
        NetDeviceContainer devices = wave.Install(wavePhy, waveMac, vehicles);

        InternetStackHelper internet;
        internet.Install(vehicles);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        // Verify that IP addresses are assigned to the vehicles
        Ipv4Address ipVehicle1 = interfaces.GetAddress(0);
        Ipv4Address ipVehicle2 = interfaces.GetAddress(1);
        Ipv4Address ipVehicle3 = interfaces.GetAddress(2);

        NS_TEST_ASSERT_MSG_NE(ipVehicle1, Ipv4Address("0.0.0.0"), "Failed to assign IP address to vehicle 1.");
        NS_TEST_ASSERT_MSG_NE(ipVehicle2, Ipv4Address("0.0.0.0"), "Failed to assign IP address to vehicle 2.");
        NS_TEST_ASSERT_MSG_NE(ipVehicle3, Ipv4Address("0.0.0.0"), "Failed to assign IP address to vehicle 3.");
    }

    // Test the setup of UDP Echo Server and Client applications
    void TestUdpApplicationSetup()
    {
        NodeContainer vehicles;
        vehicles.Create(3);

        YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default();
        wavePhy.SetChannel(waveChannel.Create());

        NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
        WaveHelper wave;
        NetDeviceContainer devices = wave.Install(wavePhy, waveMac, vehicles);

        InternetStackHelper internet;
        internet.Install(vehicles);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        UdpEchoServerHelper echoServer(9);
        Applicat
