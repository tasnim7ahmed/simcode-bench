#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <cmath>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace ns3;

int main(int argc, char* argv[])
{
    uint32_t numSpokes = 4;
    double clientStartTime = 2.0;
    double packetInterval = 0.5;
    uint32_t numPackets = 4;
    uint16_t echoPort = 9;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numSpokes", "Number of peripheral nodes in the star topology", numSpokes);
    cmd.AddValue("clientStartTime", "Initial start time for the first client application", clientStartTime);
    cmd.AddValue("packetInterval", "Interval between packets sent by UDP clients", packetInterval);
    cmd.AddValue("numPackets", "Number of packets each UDP client will send", numPackets);
    cmd.Parse(argc, argv);

    NodeContainer hubNode;
    hubNode.Create(1);
    NodeContainer spokeNodes;
    spokeNodes.Create(numSpokes);

    InternetStackHelper stack;
    stack.Install(hubNode);
    stack.Install(spokeNodes);

    PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pHelper.SetChannelAttribute("Delay", StringValue("2ms"));

    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.252");

    std::vector<NetDeviceContainer> deviceContainers(numSpokes);
    std::vector<Ipv4InterfaceContainer> interfaceContainers(numSpokes);

    for (uint32_t i = 0; i < numSpokes; ++i)
    {
        deviceContainers[i] = p2pHelper.Install(hubNode.Get(0), spokeNodes.Get(i));
        interfaceContainers[i] = address.Assign(deviceContainers[i]);
        address.NewNetwork();
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps;
    serverApps.Add(echoServer.Install(hubNode.Get(0)));
    for (uint32_t i = 0; i < numSpokes; ++i)
    {
        serverApps.Add(echoServer.Install(spokeNodes.Get(i)));
    }
    serverApps.Start(Seconds(1.0));
    
    double clientDuration = numPackets * packetInterval + 0.1;
    double totalSimTime = clientStartTime;

    for (uint32_t i = 0; i < numSpokes; ++i)
    {
        Ptr<Node> targetNode = spokeNodes.Get((i + 1) % numSpokes);
        Ptr<Ipv4> targetIpv4 = targetNode->GetObject<Ipv4>();
        Ipv4Address targetIp = targetIpv4->GetAddress(1, 0).GetLocal();

        UdpEchoClientHelper echoClient(targetIp, echoPort);
        echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = echoClient.Install(spokeNodes.Get(i));
        clientApps.Start(Seconds(totalSimTime));
        clientApps.Stop(Seconds(totalSimTime + clientDuration));

        totalSimTime += clientDuration + 0.5;
    }
    
    serverApps.Stop(Seconds(totalSimTime + 1.0));

    AnimationInterface anim("star_topology.xml");

    anim.SetConstantPosition(hubNode.Get(0), 0.0, 0.0);

    double radius = 50.0;
    for (uint32_t i = 0; i < numSpokes; ++i)
    {
        double angle = i * 2 * M_PI / numSpokes;
        double x = radius * std::cos(angle);
        double y = radius * std::sin(angle);
        anim.SetConstantPosition(spokeNodes.Get(i), x, y);

        Ptr<Ipv4> spokeIpv4 = spokeNodes.Get(i)->GetObject<Ipv4>();
        std::string ipAddrStr = spokeIpv4->GetAddress(1, 0).GetLocal().GetDottedDecimalString();
        anim.SetNodeDescription(spokeNodes.Get(i), "S" + std::to_string(i) + " (" + ipAddrStr + ")");
    }
    Ptr<Ipv4> hubIpv4 = hubNode.Get(0)->GetObject<Ipv4>();
    std::string hubIpAddrStr = hubIpv4->GetAddress(1, 0).GetLocal().GetDottedDecimalString();
    anim.SetNodeDescription(hubNode.Get(0), "Hub (" + hubIpAddrStr + ")");

    Simulator::Stop(Seconds(totalSimTime + 1.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}