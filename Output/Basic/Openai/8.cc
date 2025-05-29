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
    uint32_t nSpokes = 5; // number of leaf nodes
    uint32_t pktSize = 1024;
    double interval = 1.0; // Seconds between packet bursts per client
    double simTime = nSpokes * interval + 4.0;

    // Command line parameters
    CommandLine cmd;
    cmd.AddValue("nSpokes", "Number of leaf nodes", nSpokes);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer hubNode;
    hubNode.Create(1);
    NodeContainer spokeNodes;
    spokeNodes.Create(nSpokes);

    // Create point-to-point links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Installing net devices and assigning addresses
    std::vector<NetDeviceContainer> deviceContainers(nSpokes);
    InternetStackHelper stack;
    stack.Install(hubNode);
    stack.Install(spokeNodes);

    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaceContainers(nSpokes);

    // We'll create subnets: 192.9.39.x/29 for each link to allow room.
    for (uint32_t i = 0; i < nSpokes; ++i)
    {
        deviceContainers[i] = pointToPoint.Install(hubNode.Get(0), spokeNodes.Get(i));

        std::ostringstream subnet;
        subnet << "192.9.39." << (i*8) << "/29";
        address.SetBase(subnet.str().c_str(), "255.255.255.248");

        interfaceContainers[i] = address.Assign(deviceContainers[i]);
    }

    // For each spoke, configure it as a "server", and the others act as clients in order
    uint16_t portBase = 8000;

    ApplicationContainer serverApps, clientApps;

    for (uint32_t i = 0; i < nSpokes; ++i)
    {
        // On this spoke, set up a UDP echo server
        UdpEchoServerHelper echoServer(portBase + i);
        ApplicationContainer serverApp = echoServer.Install(spokeNodes.Get(i));
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(simTime));
        serverApps.Add(serverApp);
    }

    // For each client-server pair, only one client sends packets at a time
    double appStart = 1.0;
    double appGap = interval;

    for (uint32_t i = 0; i < nSpokes; ++i)
    {
        uint32_t clientId = i;
        uint32_t serverId = (i + 1) % nSpokes;

        // The client is spoke clientId, the server is spoke serverId.
        Ipv4Address serverAddr = interfaceContainers[serverId].GetAddress(1);

        UdpEchoClientHelper echoClient(serverAddr, portBase + serverId);
        echoClient.SetAttribute("MaxPackets", UintegerValue(4));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.25)));
        echoClient.SetAttribute("PacketSize", UintegerValue(pktSize));
        echoClient.SetAttribute("StartTime", TimeValue(Seconds(appStart + appGap * i)));
        echoClient.SetAttribute("StopTime", TimeValue(Seconds(appStart + appGap * i + 2)));
        ApplicationContainer clientApp = echoClient.Install(spokeNodes.Get(clientId));
        clientApps.Add(clientApp);
    }

    // NetAnim setup
    AnimationInterface anim("star-ring-anim.xml");

    // Set positions: place hub at center, spokes around a circle
    anim.SetConstantPosition(hubNode.Get(0), 50.0, 50.0);
    double radius = 30.0;
    for (uint32_t i = 0; i < nSpokes; ++i)
    {
        double angle = 2 * M_PI * i / nSpokes;
        double x = 50.0 + radius * std::cos(angle);
        double y = 50.0 + radius * std::sin(angle);
        anim.SetConstantPosition(spokeNodes.Get(i), x, y);
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}