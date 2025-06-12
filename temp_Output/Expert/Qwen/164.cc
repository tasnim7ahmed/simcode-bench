#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OspfRoutingSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Connect nodes with point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[7];

    devices[0] = p2p.Install(nodes.Get(0), nodes.Get(1));
    devices[1] = p2p.Install(nodes.Get(0), nodes.Get(2));
    devices[2] = p2p.Install(nodes.Get(1), nodes.Get(3));
    devices[3] = p2p.Install(nodes.Get(2), nodes.Get(3));
    devices[4] = p2p.Install(nodes.Get(1), nodes.Get(4));
    devices[5] = p2p.Install(nodes.Get(2), nodes.Get(4));
    devices[6] = p2p.Install(nodes.Get(3), nodes.Get(4));

    InternetStackHelper stack;
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaces;

    stack.SetRoutingHelper(Ipv4StaticRoutingHelper());  // Will be overridden by OSPF

    stack.Install(nodes);

    for (int i = 0; i < 7; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces.push_back(address.Assign(devices[i]));
    }

    // Enable OSPFv2 on all nodes
    Ipv4RoutingHelper* routingHelper = new OlsrHelper();
    Ipv4ListRoutingHelper listRH;
    listRH.Add(routingHelper, 0);

    InternetStackHelper stackWithOspf;
    stackWithOspf.SetRoutingHelper(listRH);
    stackWithOspf.Install(nodes);

    // Assign IP addresses again with updated routing
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
        for (uint32_t j = 0; j < ipv4->GetNInterfaces(); ++j) {
            if (ipv4->GetAddress(j, 0).GetLocal() != Ipv4Address::GetLoopback()) {
                ipv4->AddAddress(j, Ipv4InterfaceAddress(ipv4->GetAddress(j, 0).GetLocal(), ipv4->GetAddress(j, 0).GetMask()));
            }
        }
    }

    // Set up applications
    UdpEchoServerHelper echoServer(9);

    ApplicationContainer serverApps = echoServer.Install(nodes.Get(4));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces[6].GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Animation setup
    AnimationInterface anim("ospf-routing.xml");
    anim.SetConstantPosition(nodes.Get(0), 0, 0);
    anim.SetConstantPosition(nodes.Get(1), 50, 0);
    anim.SetConstantPosition(nodes.Get(2), 0, 50);
    anim.SetConstantPosition(nodes.Get(3), 50, 50);
    anim.SetConstantPosition(nodes.Get(4), 25, 25);

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}