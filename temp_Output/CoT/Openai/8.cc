#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t nSpokes = 5;
    uint32_t packetSize = 1024;
    uint32_t nPackets = 4;
    double interval = 1.0;
    double simTime = (nPackets * interval + 1.0) * nSpokes;

    Config::SetDefault("ns3::UdpEchoClient::MaxPackets", UintegerValue(nPackets));
    Config::SetDefault("ns3::UdpEchoClient::Interval", TimeValue(Seconds(interval)));
    Config::SetDefault("ns3::UdpEchoClient::PacketSize", UintegerValue(packetSize));

    // Create nodes
    NodeContainer hubNode;
    hubNode.Create(1);
    NodeContainer spokeNodes;
    spokeNodes.Create(nSpokes);

    // Set up point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Keep track of all nodes for mobility/NetAnim
    NodeContainer allNodes;
    allNodes.Add(hubNode);
    allNodes.Add(spokeNodes);

    // Install devices and channels
    NetDeviceContainer hubDevices;
    NetDeviceContainer spokeDevices[nSpokes];
    for (uint32_t i = 0; i < nSpokes; ++i)
    {
        NetDeviceContainer link = p2p.Install(hubNode.Get(0), spokeNodes.Get(i));
        hubDevices.Add(link.Get(0));
        spokeDevices[i].Add(link.Get(1));
    }

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::ostringstream subnet;
    subnet << "192.9.39.0";
    address.SetBase(subnet.str().c_str(), "255.255.255.0");
    Ipv4InterfaceContainer hubInterfaces;
    Ipv4InterfaceContainer spokeInterfaces;
    uint32_t base = 1;

    std::vector<Ipv4InterfaceContainer> interfaces(nSpokes);
    for (uint32_t i = 0; i < nSpokes; ++i)
    {
        std::ostringstream ipa, ipb;
        ipa << "192.9.39." << base++;
        ipb << "192.9.39." << base++;
        Ipv4Address if1(ipa.str().c_str());
        Ipv4Address if2(ipb.str().c_str());
        Ipv4InterfaceContainer iface = address.Assign(NetDeviceContainer(hubDevices.Get(i), spokeDevices[i].Get(0)));
        hubInterfaces.Add(iface.Get(0));
        spokeInterfaces.Add(iface.Get(1));
        address.NewNetwork(); // To ensure unique assignment per link
    }

    // Set mobility for NetAnim visualization
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    positionAlloc->Add(Vector(50.0, 50.0, 0.0)); // central hub
    for (uint32_t i = 0; i < nSpokes; ++i)
    {
        double angle = (2 * M_PI * i) / nSpokes;
        double x = 50.0 + 30.0 * std::cos(angle);
        double y = 50.0 + 30.0 * std::sin(angle);
        positionAlloc->Add(Vector(x, y, 0.0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    // Install UDP echo servers and clients in scheduled sequence
    uint16_t echoPort = 9;
    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    for (uint32_t i = 0; i < nSpokes; ++i)
    {
        // Each spoke acts as server
        UdpEchoServerHelper echoServer(echoPort);
        ApplicationContainer serverApp = echoServer.Install(spokeNodes.Get(i));
        serverApp.Start(Seconds(i * (nPackets*interval + 1.0)));
        serverApp.Stop(Seconds((i + 1) * (nPackets*interval + 1.0)));
        serverApps.Add(serverApp);

        // Central hub acts as client
        UdpEchoClientHelper echoClient(spokeInterfaces.GetAddress(i), echoPort);
        echoClient.SetAttribute("MaxPackets", UintegerValue(nPackets));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
        echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer clientApp = echoClient.Install(hubNode.Get(0));
        clientApp.Start(Seconds(i * (nPackets*interval + 1.0) + 0.5));
        clientApp.Stop(Seconds((i+1) * (nPackets*interval + 1.0)));
        clientApps.Add(clientApp);

        // Now reverse: hub is server, spoke is client, alternate after all spokes finish
    }

    // After hub-to-spoke is done, do spoke-to-hub (client-server reverse)
    double offset = nSpokes * (nPackets*interval + 1.0) + 1.0;
    for (uint32_t i = 0; i < nSpokes; ++i)
    {
        // Hub as server
        UdpEchoServerHelper echoServer(echoPort + 1);
        ApplicationContainer serverApp = echoServer.Install(hubNode.Get(0));
        serverApp.Start(Seconds(offset + i * (nPackets*interval + 1.0)));
        serverApp.Stop(Seconds(offset + (i + 1) * (nPackets*interval + 1.0)));
        serverApps.Add(serverApp);

        // Spoke as client
        UdpEchoClientHelper echoClient(hubInterfaces.GetAddress(i), echoPort + 1);
        echoClient.SetAttribute("MaxPackets", UintegerValue(nPackets));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
        echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer clientApp = echoClient.Install(spokeNodes.Get(i));
        clientApp.Start(Seconds(offset + i * (nPackets*interval + 1.0) + 0.5));
        clientApp.Stop(Seconds(offset + (i+1) * (nPackets*interval + 1.0)));
        clientApps.Add(clientApp);
    }

    // Enable NetAnim output
    AnimationInterface anim("star_ring_netanim.xml");

    Simulator::Stop(Seconds(offset + nSpokes * (nPackets*interval + 1.0) + 2.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}