#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable application logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes (vehicles)
    NodeContainer vehicles;
    vehicles.Create(3);

    // Install Wi-Fi 802.11p (WAVE) devices
    YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default();
    wavePhy.SetChannel(waveChannel.Create());
    NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
    Wifi80211pHelper waveHelper = Wifi80211pHelper::Default();
    NetDeviceContainer devices = waveHelper.Install(wavePhy, waveMac, vehicles);

    // Mobility: ConstantVelocityMobilityModel
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.SetPositionAllocator(
        "ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(20.0),
        "DeltaY", DoubleValue(0.0),
        "GridWidth", UintegerValue(3),
        "LayoutType", StringValue("RowFirst")
    );
    mobility.Install(vehicles);

    Ptr<ConstantVelocityMobilityModel> mob0 = vehicles.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    Ptr<ConstantVelocityMobilityModel> mob1 = vehicles.Get(1)->GetObject<ConstantVelocityMobilityModel>();
    Ptr<ConstantVelocityMobilityModel> mob2 = vehicles.Get(2)->GetObject<ConstantVelocityMobilityModel>();

    mob0->SetVelocity(Vector(5.0, 0.0, 0.0));   // 5 m/s along X
    mob1->SetVelocity(Vector(10.0, 0.0, 0.0));  // 10 m/s along X
    mob2->SetVelocity(Vector(15.0, 0.0, 0.0));  // 15 m/s along X

    // Internet stack
    InternetStackHelper stack;
    stack.Install(vehicles);

    // Assign IP addresses (192.168.1.0/24)
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Echo Server on last vehicle (node 2)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(vehicles.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Echo Client on first vehicle (node 0), targeting node 2
    UdpEchoClientHelper echoClient(interfaces.GetAddress(2), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = echoClient.Install(vehicles.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}