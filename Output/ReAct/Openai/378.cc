#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(3);

    // Mobility: place nodes linearly
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // Node 0
    positionAlloc->Add(Vector(50.0, 0.0, 0.0)); // Node 1
    positionAlloc->Add(Vector(100.0, 0.0, 0.0)); // Node 2
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Mesh configuration
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    MeshHelper mesh;
    mesh = MeshHelper::Default();
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
    mesh.SetRemoteStationManager("ns3::ConstantRateWifiManager", 
                                 "DataMode", StringValue("HtMcs0"), 
                                 "ControlMode", StringValue("HtMcs0"));
    // Install mesh devices
    NetDeviceContainer meshDevices = mesh.Install(wifiPhy, nodes);

    // Internet stack
    InternetStackHelper internetStack;
    internetStack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    // UDP Sink on Node 2 (acts as the server)
    uint16_t udpPort = 9000;
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApps = udpServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Client on Node 0 targeting Node 2 (must traverse Node 1)
    UdpClientHelper udpClient(interfaces.GetAddress(2), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = udpClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable routing so forwarding works on Node 1
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}