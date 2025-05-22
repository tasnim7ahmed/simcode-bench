#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

// Test for the creation of vehicle nodes
void TestVehicleNodeCreation()
{
    NodeContainer vehicleNodes;
    vehicleNodes.Create(5); // 5 vehicles

    NS_TEST_ASSERT_MSG_EQ(vehicleNodes.GetN(), 5, "Failed to create 5 vehicle nodes");
}

// Test for vehicle mobility model installation
void TestVehicleMobilityInstallation()
{
    NodeContainer vehicleNodes;
    vehicleNodes.Create(5); // 5 vehicles

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.Install(vehicleNodes);

    Ptr<ConstantVelocityMobilityModel> mobilityModel = vehicleNodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    NS_TEST_ASSERT_MSG_NOT_NULL(mobilityModel, "Mobility model not installed on vehicle nodes");
}

// Test for the velocity setting of vehicles
void TestVehicleVelocitySetting()
{
    NodeContainer vehicleNodes;
    vehicleNodes.Create(5); // 5 vehicles

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.Install(vehicleNodes);

    // Set constant velocity for vehicles (50 m/s)
    for (uint32_t i = 0; i < vehicleNodes.GetN(); ++i)
    {
        Ptr<ConstantVelocityMobilityModel> mobilityModel = vehicleNodes.Get(i)->GetObject<ConstantVelocityMobilityModel>();
        mobilityModel->SetVelocity(Vector(50.0, 0.0, 0.0));
    }

    Ptr<ConstantVelocityMobilityModel> model = vehicleNodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    Vector velocity = model->GetVelocity();
    NS_TEST_ASSERT_MSG_EQ(velocity.x, 50.0, "Failed to set velocity to 50 m/s");
}

// Test for Wave (802.11p) device installation
void TestWaveDeviceInstallation()
{
    NodeContainer vehicleNodes;
    vehicleNodes.Create(5); // 5 vehicles

    YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
    wavePhy.SetChannel(waveChannel.Create());

    NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
    Wifi80211pHelper waveHelper = Wifi80211pHelper::Default();

    NetDeviceContainer vehicleDevices;
    vehicleDevices = waveHelper.Install(wavePhy, waveMac, vehicleNodes);

    NS_TEST_ASSERT_MSG_EQ(vehicleDevices.GetN(), 5, "Failed to install Wave devices on vehicles");
}

// Test for Internet stack installation on vehicles
void TestInternetStackInstallation()
{
    NodeContainer vehicleNodes;
    vehicleNodes.Create(5); // 5 vehicles

    InternetStackHelper internet;
    internet.Install(vehicleNodes);

    Ptr<Ipv4> ipv4 = vehicleNodes.Get(0)->GetObject<Ipv4>();
    NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack not installed on vehicle nodes");
}

// Test for IP address assignment to vehicle devices
void TestIpAddressAssignment()
{
    NodeContainer vehicleNodes;
    vehicleNodes.Create(5); // 5 vehicles

    YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
    wavePhy.SetChannel(waveChannel.Create());

    NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
    Wifi80211pHelper waveHelper = Wifi80211pHelper::Default();

    NetDeviceContainer vehicleDevices;
    vehicleDevices = waveHelper.Install(wavePhy, waveMac, vehicleNodes);

    InternetStackHelper internet;
    internet.Install(vehicleNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer vehicleInterfaces = ipv4.Assign(vehicleDevices);

    NS_TEST_ASSERT_MSG_EQ(vehicleInterfaces.GetN(), 5, "Failed to assign IP addresses to vehicle devices");
}

// Test for BSM application installation
void TestBsmApplicationInstallation()
{
    NodeContainer vehicleNodes;
    vehicleNodes.Create(5); // 5 vehicles

    for (uint32_t i = 0; i < vehicleNodes.GetN(); ++i)
    {
        Ptr<BsmApplication> app = CreateObject<BsmApplication>();
        vehicleNodes.Get(i)->AddApplication(app);
        app->SetStartTime(Seconds(1.0));
        app->SetStopTime(Seconds(10.0));
    }

    Ptr<Application> app = vehicleNodes.Get(0)->GetApplication(0);
    NS_TEST_ASSERT_MSG_NOT_NULL(app, "Failed to install BSM application on vehicle node 0");
}

// Test for NetAnim visualization setup
void TestNetAnimSetup()
{
    NodeContainer vehicleNodes;
    vehicleNodes.Create(5); // 5 vehicles

    AnimationInterface anim("vanet_netanim.xml");
    anim.SetConstantPosition(vehicleNodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(vehicleNodes.Get(1), 50.0, 0.0);
    anim.SetConstantPosition(vehicleNodes.Get(2), 100.0, 0.0);
    anim.SetConstantPosition(vehicleNodes.Get(3), 150.0, 0.0);
    anim.SetConstantPosition(vehicleNodes.Get(4), 200.0, 0.0);

    // Check if the positions are set correctly for visualization
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(vehicleNodes.Get(0)).x, 0.0, "Vehicle 1 position incorrect");
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(vehicleNodes.Get(1)).x, 50.0, "Vehicle 2 position incorrect");
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(vehicleNodes.Get(2)).x, 100.0, "Vehicle 3 position incorrect");
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(vehicleNodes.Get(3)).x, 150.0, "Vehicle 4 position incorrect");
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(vehicleNodes.Get(4)).x, 200.0, "Vehicle 5 position incorrect");
}

int main()
{
    TestVehicleNodeCreation();
    TestVehicleMobilityInstallation();
    TestVehicleVelocitySetting();
    TestWaveDeviceInstallation();
    TestInternetStackInstallation();
    TestIpAddressAssignment();
    TestBsmApplicationInstallation();
    TestNetAnimSetup();

    return 0;
}
