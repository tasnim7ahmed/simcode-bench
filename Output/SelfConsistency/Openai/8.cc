#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarRingHybridNetwork");

int main(int argc, char *argv[])
{
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    uint32_t nSpokes = 4; // Number of spokes (clients/servers)
    uint32_t nNodes = nSpokes + 1; // central + spokes
    double interval = 2.0; // Interval between each client-server pair start (seconds)
    uint32_t packetCount = 4;
    uint16_t echoPort = 9;
    double packetInterval = 0.5; // Time between packets for each client

    // Create nodes
    NodeContainer starNodes;
    starNodes.Create(nNodes); // nodes: 0 = central, 1...nSpokes = spokes

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Containers for central node and each spoke
    NodeContainer centralNode;
    centralNode.Add(starNodes.Get(0));

    std::vector<NodeContainer> spokeLinks;
    std::vector<NetDeviceContainer> deviceContainers;
    for (uint32_t i = 1; i <= nSpokes; ++i)
    {
        NodeContainer pair(centralNode.Get(0), starNodes.Get(i));
        spokeLinks.push_back(pair);
        deviceContainers.push_back(p2p.Install(pair));
    }

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(starNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaceContainers;

    // IP subnet base: 192.9.39.x
    uint8_t subnetBase[4] = {192, 9, 39, 1};

    for (uint32_t i = 0; i < nSpokes; ++i)
    {
        std::ostringstream subnet;
        subnet << "192.9.39.0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        // Assign a unique IP to each link by adjusting the .x field
        for (uint32_t j = 0; j < 2; ++j)
        {
            subnetBase[3] = 1 + (i * 2) + j;
        }
        Ipv4InterfaceContainer iface = address.Assign(deviceContainers[i]);
        interfaceContainers.push_back(iface);
        // Prepare for the next link (increment base by 2 per two nodes)
        address.NewNetwork();
    }

    // Setup applications: Each node acts as both client and server
    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    for (uint32_t i = 1; i <= nSpokes; ++i)
    {
        double startTime = 1.0 + (i - 1) * interval;

        // 1. Install server on spoke[i]
        UdpEchoServerHelper echoServer(echoPort);
        ApplicationContainer serverApp = echoServer.Install(starNodes.Get(i));
        serverApp.Start(Seconds(startTime));
        serverApp.Stop(Seconds(startTime + interval + 2.0));
        serverApps.Add(serverApp);

        // 2. Install client on central node, sending to spoke[i]
        UdpEchoClientHelper echoClient(interfaceContainers[i-1].GetAddress(1), echoPort);
        echoClient.SetAttribute("MaxPackets", UintegerValue(packetCount));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = echoClient.Install(starNodes.Get(0));
        clientApp.Start(Seconds(startTime + 0.2)); // client starts shortly after server
        clientApp.Stop(Seconds(startTime + interval + 1.5));
        clientApps.Add(clientApp);

        // 3. Install server on central node
        UdpEchoServerHelper echoServerCentral(echoPort + 10);
        ApplicationContainer serverAppCentral = echoServerCentral.Install(starNodes.Get(0));
        serverAppCentral.Start(Seconds(startTime + 0.25));
        serverAppCentral.Stop(Seconds(startTime + interval + 2.0));
        serverApps.Add(serverAppCentral);

        // 4. Install client on spoke[i] to central node
        UdpEchoClientHelper echoClientSpoke(interfaceContainers[i-1].GetAddress(0), echoPort + 10);
        echoClientSpoke.SetAttribute("MaxPackets", UintegerValue(packetCount));
        echoClientSpoke.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
        echoClientSpoke.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientAppSpoke = echoClientSpoke.Install(starNodes.Get(i));
        clientAppSpoke.Start(Seconds(startTime + 0.5)); // starts a bit after previous
        clientAppSpoke.Stop(Seconds(startTime + interval + 1.5));
        clientApps.Add(clientAppSpoke);
    }

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup NetAnim
    AnimationInterface anim("star_ring_hybrid.xml");

    // (Optional) Set positions for anim clarity
    anim.SetConstantPosition(starNodes.Get(0), 50, 50); // central node
    double radius = 40.0;
    for (uint32_t i = 1; i <= nSpokes; ++i)
    {
        double angle = 2 * M_PI * (i - 1) / nSpokes;
        double x = 50 + radius * std::cos(angle);
        double y = 50 + radius * std::sin(angle);
        anim.SetConstantPosition(starNodes.Get(i), x, y);
    }

    Simulator::Stop(Seconds(1.0 + nSpokes * interval + 5.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}