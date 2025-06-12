#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CountToInfinitySimulation");

void TearDownLink(Ptr<Node> node1, Ptr<Node> node2)
{
    // Remove the NetDevices and Channels between node1 and node2
    for (uint32_t i = 0; i < node1->GetNDevices(); ++i)
    {
        Ptr<NetDevice> dev1 = node1->GetDevice(i);
        Ptr<Channel> channel1 = dev1->GetChannel();
        if (channel1)
        {
            for (uint32_t j = 0; j < node2->GetNDevices(); ++j)
            {
                Ptr<NetDevice> dev2 = node2->GetDevice(j);
                Ptr<Channel> channel2 = dev2->GetChannel();
                if (channel1 == channel2)
                {
                    dev1->SetAttribute("ReceiveEnable", BooleanValue(false));
                    dev2->SetAttribute("ReceiveEnable", BooleanValue(false));
                    NS_LOG_INFO("Tearing down link between node " 
                                << node1->GetId() << " and node " << node2->GetId());
                }
            }
        }
    }
}

int main(int argc, char *argv[])
{
    LogComponentEnable("Rip", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    // Label nodes for clarity
    Ptr<Node> A = nodes.Get(0);
    Ptr<Node> B = nodes.Get(1);
    Ptr<Node> C = nodes.Get(2);

    // Create p2p links: A-B, B-C, A-C (triangle)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // NetDevices/Containers for each link
    NetDeviceContainer ab = p2p.Install(A, B); // link1: A<->B
    NetDeviceContainer bc = p2p.Install(B, C); // link2: B<->C
    NetDeviceContainer ca = p2p.Install(C, A); // link3: C<->A

    // Install Internet stack with RIP (Distance Vector)
    InternetStackHelper stack;
    RipHelper rip;
    rip.SetInterfaceExclusions(ab.Get(0), ab.Get(1)); // No exclusions: all use RIP
    rip.SetInterfaceExclusions(bc.Get(0), bc.Get(1));
    rip.SetInterfaceExclusions(ca.Get(0), ca.Get(1));
    stack.SetRoutingHelper(rip);
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer abIf = address.Assign(ab);

    address.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bcIf = address.Assign(bc);

    address.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer caIf = address.Assign(ca);

    // Enable routing table logging
    RipHelper::EnableRoutingTableLogging(Seconds(0), Seconds(2.0), nodes);

    // Wait a few seconds for convergence, then break B-C link at t=10
    Simulator::Schedule(Seconds(10.0), &TearDownLink, B, C);

    // Optionally, ping from A to C to trigger activity (not strictly necessary)
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", 
                      InetSocketAddress(bcIf.GetAddress(1), port));
    onoff.SetConstantRate(DataRate("100kbps"));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(3.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(30.0)));
    ApplicationContainer app = onoff.Install(A);

    PacketSinkHelper sink("ns3::UdpSocketFactory",
                          InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer appSink = sink.Install(C);
    appSink.Start(Seconds(2.0));
    appSink.Stop(Seconds(30.0));

    Simulator::Stop(Seconds(35.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}