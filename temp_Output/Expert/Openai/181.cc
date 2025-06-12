#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    uint32_t numVehicles = 2;
    double simTime = 10.0; // seconds
    double distance = 100.0; // meters between vehicles
    double nodeSpeed = 15.0; // m/s

    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default();
    wavePhy.SetChannel(waveChannel.Create());

    NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
    Wifi80211pHelper waveHelper = Wifi80211pHelper::Default();

    waveHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode", StringValue("OfdmRate6MbpsBW10MHz"),
                                       "ControlMode", StringValue("OfdmRate6MbpsBW10MHz"));

    NetDeviceContainer deviceContainer = waveHelper.Install(wavePhy, waveMac, vehicles);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(distance),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(numVehicles),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.Install(vehicles);
    // Set velocities
    Ptr<ConstantVelocityMobilityModel> mob0 = vehicles.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    mob0->SetVelocity(Vector(nodeSpeed, 0.0, 0.0));
    Ptr<ConstantVelocityMobilityModel> mob1 = vehicles.Get(1)->GetObject<ConstantVelocityMobilityModel>();
    mob1->SetVelocity(Vector(nodeSpeed, 0.0, 0.0));

    InternetStackHelper internet;
    internet.Install(vehicles);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaces = ipv4.Assign(deviceContainer);

    uint16_t port = 4000;

    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(vehicles.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    UdpClientHelper udpClient(ifaces.GetAddress(1), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(50));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.2)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = udpClient.Install(vehicles.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime - 1.0));

    AnimationInterface anim("vanet-dsrc.xml");
    anim.SetConstantPosition(vehicles.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(vehicles.Get(1), distance, 0.0);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}