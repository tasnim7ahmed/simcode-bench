#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t nLeaf = 4;
    double packetInterval = 1.0; // seconds between each client
    double echoInterval = 0.2; // time between client packets
    uint32_t packetSize = 1024;
    uint32_t numPackets = 4;
    uint16_t echoPort = 9;

    CommandLine cmd;
    cmd.AddValue("nLeaf", "Number of leaf nodes", nLeaf);
    cmd.Parse(argc, argv);

    NodeContainer central;
    central.Create(1);

    NodeContainer leaves;
    leaves.Create(nLeaf);

    NodeContainer starNodes;
    starNodes.Add(central.Get(0));
    for (uint32_t i = 0; i < nLeaf; ++i)
        starNodes.Add(leaves.Get(i));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> ndc(nLeaf);
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        NodeContainer pair(central.Get(0), leaves.Get(i));
        ndc[i] = p2p.Install(pair);
    }

    InternetStackHelper stack;
    stack.Install(starNodes);

    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> intf(nLeaf);
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        std::ostringstream subnet;
        subnet << "192.9.39." << 1 + i * 8 << "/29";
        address.SetBase(subnet.str().c_str(), "255.255.255.248");
        intf[i] = address.Assign(ndc[i]);
    }

    double appStart = 1.0;
    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        Ptr<Node> clientNode = leaves.Get(i);
        Ptr<Node> serverNode = central.Get(0);
        Ipv4Address serverAddress = intf[i].GetAddress(0);

        UdpEchoServerHelper echoServer(echoPort);
        ApplicationContainer serverApp = echoServer.Install(serverNode);
        serverApp.Start(Seconds(appStart));
        serverApp.Stop(Seconds(appStart + numPackets * echoInterval + 5));

        UdpEchoClientHelper echoClient(serverAddress, echoPort);
        echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(echoInterval)));
        echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer clientApp = echoClient.Install(clientNode);
        clientApp.Start(Seconds(appStart));
        clientApp.Stop(Seconds(appStart + numPackets * echoInterval + 1));

        serverApps.Add(serverApp);
        clientApps.Add(clientApp);

        appStart += packetInterval;
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    AnimationInterface anim("star-ring-mix.xml");
    anim.SetMaxPktsPerTraceFile(5000000);

    anim.SetConstantPosition(central.Get(0), 50, 50);
    double radius = 35.0;
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        double angle = 2 * M_PI * i / nLeaf;
        double x = 50 + radius * std::cos(angle);
        double y = 50 + radius * std::sin(angle);
        anim.SetConstantPosition(leaves.Get(i), x, y);
    }

    Simulator::Stop(Seconds(appStart + 10));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}