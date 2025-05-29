#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/mesh-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // Create two nodes: mobile (0) and static (1)
    NodeContainer nodes;
    nodes.Create(2);

    // Create wifi channel
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    MeshHelper mesh;
    mesh = MeshHelper::Default();
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
    mesh.SetStandard(WIFI_PHY_STANDARD_80211s);

    NetDeviceContainer meshDevices = mesh.Install(wifiPhy, nodes);

    // Mobility: mobile node moves in a straight line, static node stays put
    MobilityHelper mobility;

    // Static node
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // Mobile initial position
    positionAlloc->Add(Vector(50.0, 0.0, 0.0)); // Static node position
    mobility.SetPositionAllocator(positionAlloc);

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes.Get(0));
    Ptr<ConstantVelocityMobilityModel> mob = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    mob->SetVelocity(Vector(2.0, 0.0, 0.0)); // Move at 2 m/s along x axis

    // Static node does not move
    MobilityHelper mobilityStatic;
    mobilityStatic.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityStatic.Install(nodes.Get(1));

    // Internet stack and IP addresses
    InternetStackHelper internetStack;
    internetStack.Install(nodes);
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    // UDP server (static node)
    uint16_t port = 5000;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    // UDP client (mobile node)
    UdpClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(100));
    client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}