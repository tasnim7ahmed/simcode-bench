#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    uint32_t nSpokes = 4;
    double startTime = 1.0;
    double simTime = 12.0;
    double interval = 0.5; // Interval between packets in EchoClient

    CommandLine cmd;
    cmd.AddValue("nSpokes", "Number of spoke nodes", nSpokes);
    cmd.Parse(argc, argv);

    NodeContainer hub;
    hub.Create(1);
    NodeContainer spokes;
    spokes.Create(nSpokes);

    // Create the star links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> deviceContainers;
    std::vector<NodeContainer> pairContainers;

    for (uint32_t i = 0; i < nSpokes; ++i)
    {
        NodeContainer pair(hub.Get(0), spokes.Get(i));
        pairContainers.push_back(pair);
        NetDeviceContainer link = p2p.Install(pair);
        deviceContainers.push_back(link);
    }

    // Install stack
    InternetStackHelper internet;
    internet.Install(hub);
    internet.Install(spokes);

    // Assign IPs
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaces;
    for (uint32_t i = 0; i < nSpokes; ++i)
    {
        std::ostringstream subnet;
        subnet << "192.9.39." << (i * 2) << "/31";
        address.SetBase(subnet.str().c_str(), "255.255.255.254");
        interfaces.push_back(address.Assign(deviceContainers[i]));
    }

    // Set netanim positions
    AnimationInterface anim("star_ring_ns3.xml");
    anim.SetConstantPosition(hub.Get(0), 50, 50);
    double radius = 30.0;
    for (uint32_t i = 0; i < nSpokes; ++i)
    {
        double angle = (2 * M_PI * i) / nSpokes;
        double x = 50 + radius * std::cos(angle);
        double y = 50 + radius * std::sin(angle);
        anim.SetConstantPosition(spokes.Get(i), x, y);
    }

    uint16_t port = 8000;

    for (uint32_t i = 0; i < nSpokes; ++i)
    {
        // Each spoke node is a server
        UdpEchoServerHelper echoServer(port + i);
        ApplicationContainer serverApps;

        // Server always installed on the destination node (spoke)
        serverApps = echoServer.Install(spokes.Get(i));
        serverApps.Start(Seconds(0.5 + startTime * i));
        serverApps.Stop(Seconds(simTime));

        // Client: one at a time, each sends to the next spoke node (looped); acts as a "ring"
        uint32_t clientIdx = i;
        uint32_t serverIdx = (i + 1) % nSpokes;
        UdpEchoClientHelper echoClient(interfaces[serverIdx].GetAddress(1), port + serverIdx);
        echoClient.SetAttribute("MaxPackets", UintegerValue(4));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
        echoClient.SetAttribute("PacketSize", UintegerValue(64));
        ApplicationContainer clientApps = echoClient.Install(spokes.Get(clientIdx));
        double clientStart = startTime + i * 2.0; // Stagger activities: one pair at a time
        clientApps.Start(Seconds(clientStart));
        clientApps.Stop(Seconds(clientStart + (4 * interval) + 1.0));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}