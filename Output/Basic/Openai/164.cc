#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OspfNetAnimExample");

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Number of nodes in the backbone
    uint32_t nNodes = 5;

    NodeContainer nodes;
    nodes.Create(nNodes);

    // Install OSPF routing stack
    InternetStackHelper internet;
    Ipv4RoutingHelperOSPFFactory ospfHelperFactory;
    Ptr<Ipv4RoutingHelper> ospfHelper = ospfHelperFactory.Create();
    internet.SetRoutingHelper(*ospfHelper);
    internet.Install(nodes);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> deviceContainers;
    std::vector<Ipv4InterfaceContainer> interfaceContainers;

    Ipv4AddressHelper ipv4;
    uint32_t networkBase = 1;

    // Interconnect in a line topology plus add a "shortcut" between first and last to show OSPF path computation
    for (uint32_t i = 0; i < nNodes - 1; ++i)
    {
        NetDeviceContainer nd = p2p.Install(NodeContainer(nodes.Get(i), nodes.Get(i + 1)));
        deviceContainers.push_back(nd);
        std::ostringstream subnet;
        subnet << "10." << networkBase << ".0.0";
        ipv4.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.0");
        interfaceContainers.push_back(ipv4.Assign(nd));
        ++networkBase;
    }
    // Shortcut link: node 0 <-> node nNodes-1
    NetDeviceContainer shortcut = p2p.Install(NodeContainer(nodes.Get(0), nodes.Get(nNodes - 1)));
    deviceContainers.push_back(shortcut);
    std::ostringstream subnet;
    subnet << "10." << networkBase << ".0.0";
    ipv4.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.0");
    interfaceContainers.push_back(ipv4.Assign(shortcut));

    // Assign NetAnim positions for visualization
    AnimationInterface anim("ospf-netanim.xml");
    for (uint32_t i = 0; i < nNodes; ++i)
    {
        anim.SetConstantPosition(nodes.Get(i), 100 + i * 100, 200);
    }
    // Draw the shortcut clearly below the main line
    anim.SetConstantPosition(nodes.Get(0), 100, 200);
    anim.SetConstantPosition(nodes.Get(nNodes-1), 100 + (nNodes-1)*100, 200);

    // Install applications: UDP traffic from node 0 to node nNodes-1
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory",
                      Address(InetSocketAddress(interfaceContainers.back().GetAddress(1), port)));
    onoff.SetConstantRate(DataRate("500kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(18.0)));
    onoff.Install(nodes.Get(0));

    PacketSinkHelper sink("ns3::UdpSocketFactory",
                          Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(nNodes - 1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(20.0));

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}