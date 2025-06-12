#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OspfSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(5);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[7];

    devices[0] = p2p.Install(NodeContainer(nodes.Get(0), nodes.Get(1)));
    devices[1] = p2p.Install(NodeContainer(nodes.Get(0), nodes.Get(2)));
    devices[2] = p2p.Install(NodeContainer(nodes.Get(1), nodes.Get(3)));
    devices[3] = p2p.Install(NodeContainer(nodes.Get(2), nodes.Get(3)));
    devices[4] = p2p.Install(NodeContainer(nodes.Get(1), nodes.Get(4)));
    devices[5] = p2p.Install(NodeContainer(nodes.Get(2), nodes.Get(4)));
    devices[6] = p2p.Install(NodeContainer(nodes.Get(3), nodes.Get(4)));

    InternetStackHelper stack;
    Ipv4RoutingHelper routing = Ipv4RoutingHelper::Create("ns3::OlsrRoutingProtocol");
    stack.SetRoutingHelper(routing);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[7];

    for (uint32_t i = 0; i < 7; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    uint16_t port = 9;

    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(4));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces[6].GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AnimationInterface anim("ospf_simulation.xml");
    anim.SetConstantPosition(nodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(nodes.Get(1), 50.0, 50.0);
    anim.SetConstantPosition(nodes.Get(2), -50.0, 50.0);
    anim.SetConstantPosition(nodes.Get(3), 50.0, 100.0);
    anim.SetConstantPosition(nodes.Get(4), -50.0, 100.0);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}