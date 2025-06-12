#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    uint32_t nNodes = 4;
    NodeContainer nodes;
    nodes.Create(nNodes);

    // Create ring links: (0-1), (1-2), (2-3), (3-0)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    std::vector<NodeContainer> nodePairs;
    for (uint32_t i = 0; i < nNodes; ++i)
    {
        NodeContainer pair(nodes.Get(i), nodes.Get((i + 1) % nNodes));
        nodePairs.push_back(pair);
    }

    std::vector<NetDeviceContainer> devicesVec;
    for (auto &pair : nodePairs)
    {
        devicesVec.push_back(p2p.Install(pair));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IPs per link
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfacesVec;
    for (uint32_t i = 0; i < nodePairs.size(); ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfacesVec.push_back(address.Assign(devicesVec[i]));
    }

    // Configure logging of packet transmissions
    Config::SetDefault("ns3::OnOffApplication::PacketSize", UintegerValue(1024));
    Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue("1Mbps"));

    // Install packet sinks on each node, listening on port 8080
    uint16_t port = 8080;
    ApplicationContainer sinkApps;
    for (uint32_t i = 0; i < nNodes; ++i)
    {
        Address sinkAddr(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddr);
        sinkApps.Add(sinkHelper.Install(nodes.Get(i)));
    }
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // Each node sends to its right-hand neighbor in the ring (node i -> node (i+1)%nNodes)
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < nNodes; ++i)
    {
        uint32_t dstNodeIdx = (i + 1) % nNodes;
        // Figure out IP address of dstNode as seen by srcNode (right-hand side interface)
        // Each link: nodePairs[i] connects nodes i, (i+1)%nNodes. So interface 1 on this link is (i+1)%nNodes.
        Ipv4Address dstAddr = interfacesVec[i].GetAddress(1);
        AddressValue remoteAddress(InetSocketAddress(dstAddr, port));
        OnOffHelper onoff("ns3::TcpSocketFactory", Address());
        onoff.SetAttribute("Remote", remoteAddress);
        onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0 + 0.2 * i)));
        onoff.SetAttribute("StopTime", TimeValue(Seconds(9.0)));
        ApplicationContainer app = onoff.Install(nodes.Get(i));
        clientApps.Add(app);
    }

    // Enable packet logging
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("ring-topology.tr"));

    // Also log packet sends on each OnOff application
    for (auto app : clientApps)
    {
        Ptr<Application> onoffApp = app;
        onoffApp->TraceConnectWithoutContext("Tx", MakeCallback(
            [](Ptr<const Packet> pkt) {
                NS_LOG_UNCOND(Simulator::Now().GetSeconds()
                              << "s Tx packet, size: " << pkt->GetSize() << " bytes");
            }
        ));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}