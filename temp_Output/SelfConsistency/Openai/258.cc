#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set simulation time and packet parameters
    double simTime = 10.0;
    uint32_t packetSize = 512;
    uint32_t numPackets = 3;
    double interPacketInterval = 1.0; // seconds

    // Create three nodes (vehicles)
    NodeContainer vehicles;
    vehicles.Create(3);

    // Configure 802.11p PHY and MAC (WAVE)
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    NqosWaveMacHelper wifi80211pMac = NqosWaveMacHelper::Default();
    Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();

    NetDeviceContainer devices = wifi80211p.Install(wifiPhy, wifi80211pMac, vehicles);

    // Assign Mobility: ConstantVelocityMobilityModel
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.Install(vehicles);

    // Set initial velocities along X-axis
    Ptr<ConstantVelocityMobilityModel> mob0 = vehicles.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    Ptr<ConstantVelocityMobilityModel> mob1 = vehicles.Get(1)->GetObject<ConstantVelocityMobilityModel>();
    Ptr<ConstantVelocityMobilityModel> mob2 = vehicles.Get(2)->GetObject<ConstantVelocityMobilityModel>();
    mob0->SetVelocity(Vector(10.0, 0.0, 0.0)); // 10 m/s
    mob1->SetVelocity(Vector(15.0, 0.0, 0.0)); // 15 m/s
    mob2->SetVelocity(Vector(20.0, 0.0, 0.0)); // 20 m/s

    // Install Internet stack and assign IP addresses
    InternetStackHelper internet;
    internet.Install(vehicles);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // UDP Echo Server on last vehicle (vehicle 2)
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);

    ApplicationContainer serverApps = echoServer.Install(vehicles.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    // UDP Echo Client on first vehicle (vehicle 0), targeting vehicle 2
    UdpEchoClientHelper echoClient(interfaces.GetAddress(2), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = echoClient.Install(vehicles.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime - 1));

    // Enable ASCII tracing (optional)
    //AsciiTraceHelper ascii;
    //wifiPhy.EnableAsciiAll(ascii.CreateFileStream("vanet-80211p.tr"));

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}