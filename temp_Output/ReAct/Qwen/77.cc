#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ripng-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RipNgSplitHorizon");

int main(int argc, char *argv[]) {
    bool printRoutingTables = false;
    bool splitHorizon = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("printRoutingTable", "Print routing tables at 3 seconds", printRoutingTables);
    cmd.AddValue("splitHorizon", "Enable Split Horizon in RIPng", splitHorizon);
    cmd.Parse(argc, argv);

    if (splitHorizon) {
        Config::SetDefault("ns3::Ripng::SplitHorizon", EnumValue(Ripng::SPLIT_HORIZON));
    }

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(6); // SRC, A, B, C, D, DST

    Names::Add("SRC", nodes.Get(0));
    Names::Add("A", nodes.Get(1));
    Names::Add("B", nodes.Get(2));
    Names::Add("C", nodes.Get(3));
    Names::Add("D", nodes.Get(4));
    Names::Add("DST", nodes.Get(5));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devSRC_A = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devA_B = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devA_C = p2p.Install(nodes.Get(1), nodes.Get(3));
    NetDeviceContainer devB_C = p2p.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer devB_D = p2p.Install(nodes.Get(2), nodes.Get(4));
    NetDeviceContainer devC_D = p2p.Install(nodes.Get(3), nodes.Get(4));
    NetDeviceContainer devD_DST = p2p.Install(nodes.Get(4), nodes.Get(5));

    InternetStackHelper internetv6;
    RipngHelper ripng;
    ripng.ExcludeInterface(nodes.Get(0), 1); // exclude loopback
    ripng.ExcludeInterface(nodes.Get(5), 1);
    internetv6.SetRoutingHelper(ripng);
    internetv6.Install(NodeContainer::GetGlobal());

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifSRC_A = ipv6.Assign(devSRC_A);
    Ipv6InterfaceContainer ifA_B = ipv6.Assign(devA_B);
    Ipv6InterfaceContainer ifA_C = ipv6.Assign(devA_C);
    Ipv6InterfaceContainer ifB_C = ipv6.Assign(devB_C);
    Ipv6InterfaceContainer ifB_D = ipv6.Assign(devB_D);
    Ipv6InterfaceContainer ifC_D = ipv6.Assign(devC_D);
    Ipv6InterfaceContainer ifD_DST = ipv6.Assign(devD_DST);

    ifSRC_A.SetForwarding(0, true);
    ifSRC_A.SetDefaultRouteInAllNodes(true);

    Ipv6GlobalRoutingHelper::PopulateRoutingTables();

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(5));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(50.0));

    UdpEchoClientHelper echoClient(ifDST.GetAddress(0, 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(3.0));
    clientApps.Stop(Seconds(50.0));

    p2p.EnablePcapAll("ripng-split-horizon", true, true);
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("ripng-split-horizon.tr"));

    Simulator::Schedule(Seconds(40.0), &PointToPointNetDevice::SetLinkStatus, devB_D.Get(0), false);
    Simulator::Schedule(Seconds(44.0), &PointToPointNetDevice::SetLinkStatus, devB_D.Get(0), true);

    if (printRoutingTables) {
        Simulator::Schedule(Seconds(3.0), &Ipv6RoutingHelper::PrintRoutingTableAt, &ripng, Seconds(3.0), nodes.Get(1), std::cout);
        Simulator::Schedule(Seconds(3.0), &Ipv6RoutingHelper::PrintRoutingTableAt, &ripng, Seconds(3.0), nodes.Get(4), std::cout);
    }

    Simulator::Stop(Seconds(50.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}