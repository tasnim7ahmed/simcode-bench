#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Adhoc80211sMobileUdpExample");

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Simulation parameters
    double distance = 100.0;
    double speed = 5.0;
    double simTime = 25.0;

    // Create nodes (1 mobile, 1 static)
    NodeContainer nodes;
    nodes.Create(2);

    // Configure Wi-Fi Mesh (802.11s)
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    MeshHelper mesh;
    mesh = MeshHelper::Default();
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
    mesh.SetNumberOfInterfaces(1);

    NetDeviceContainer meshDevices = mesh.Install(phy, nodes);

    // Set up mobility
    MobilityHelper mobility;
    
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));     // Node 0 (mobile) start position
    positionAlloc->Add(Vector(distance, 0.0, 0.0));   // Node 1 (static) position

    mobility.SetPositionAllocator(positionAlloc);

    // Mobile node: straight line (0,0) to (distance,0)
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes.Get(0));
    nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(speed,0,0));
    // Static node: no movement
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes.Get(1));

    // Install TCP/IP stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    // UDP server (on static node)
    uint16_t udpPort = 4000;
    UdpServerHelper server(udpPort);
    ApplicationContainer serverApp = server.Install(nodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    // UDP client (on mobile node)
    uint32_t packetSize = 1024;
    uint32_t maxPackets = 320;
    double interval = 0.05; // seconds

    UdpClientHelper client(interfaces.GetAddress(1), udpPort);
    client.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    client.SetAttribute("Interval", TimeValue(Seconds(interval)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simTime));

    // Enable logging on UDP Server
    Config::Connect("/NodeList/1/ApplicationList/*/$ns3::UdpServer/Rx", 
        MakeCallback([] (Ptr<const Packet> packet, const Address &from)
        {
            NS_LOG_UNCOND("Received " << packet->GetSize() << " bytes at " 
                << Simulator::Now().GetSeconds() << " seconds from " 
                << InetSocketAddress::ConvertFrom(from).GetIpv4());
        }));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}