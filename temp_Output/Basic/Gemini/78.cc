#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/rip.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/command-line.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool verbose = false;
    bool printRoutingTables = false;
    bool pingVisibility = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Tell application to log if true.", verbose);
    cmd.AddValue("printRoutingTables", "Print routing tables if true.", printRoutingTables);
    cmd.AddValue("pingVisibility", "Show pings", pingVisibility);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("Rip", LOG_LEVEL_ALL);
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    NodeContainer srcNode, dstNode;
    srcNode.Create(1);
    dstNode.Create(1);

    NodeContainer routerA, routerB, routerC, routerD;
    routerA.Create(1);
    routerB.Create(1);
    routerC.Create(1);
    routerD.Create(1);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer srcA = p2p.Install(srcNode.Get(0), routerA.Get(0));
    NetDeviceContainer aB = p2p.Install(routerA.Get(0), routerB.Get(0));
    NetDeviceContainer aC = p2p.Install(routerA.Get(0), routerC.Get(0));
    NetDeviceContainer bD = p2p.Install(routerB.Get(0), routerD.Get(0));
    NetDeviceContainer cD = p2p.Install(routerC.Get(0), routerD.Get(0));
    NetDeviceContainer dDst = p2p.Install(routerD.Get(0), dstNode.Get(0));

    InternetStackHelper internet;
    internet.Install(srcNode);
    internet.Install(dstNode);
    internet.Install(routerA);
    internet.Install(routerB);
    internet.Install(routerC);
    internet.Install(routerD);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iSrcA = ipv4.Assign(srcA);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iAB = ipv4.Assign(aB);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer iAC = ipv4.Assign(aC);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer iBD = ipv4.Assign(bD);

    ipv4.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer iCD = ipv4.Assign(cD);

    ipv4.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer iDDst = ipv4.Assign(dDst);

    Ipv4Address sinkAddress = iDDst.GetAddress(1);

    Rip ripRouting;
    ripRouting.SetSplitHorizon(true);
    ripRouting.SetPoisonReverse(true);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    ripRouting.ExcludeInterface(routerA.Get(0), 1);
    ripRouting.ExcludeInterface(routerB.Get(0), 1);
    ripRouting.ExcludeInterface(routerC.Get(0), 1);
    ripRouting.ExcludeInterface(routerD.Get(0), 1);

    ApplicationContainer sinkApp = UdpEchoServerHelper(9).Install(dstNode.Get(0));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(100.0));

    UdpEchoClientHelper client(sinkAddress, 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(srcNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(99.0));

    Simulator::Schedule(Seconds(40.0), [&]() {
        p2p.SetDeviceAttribute("DataRate", StringValue("0kbps"));
        p2p.SetChannelAttribute("Delay", StringValue("1s"));
        NetDeviceContainer temp = p2p.Install(routerB.Get(0), routerD.Get(0));
    });

    Simulator::Schedule(Seconds(44.0), [&]() {
        p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));
        NetDeviceContainer temp = p2p.Install(routerB.Get(0), routerD.Get(0));
    });

    if (printRoutingTables) {
        Simulator::Schedule(Seconds(10.0), [&]() {
            Ipv4RoutingHelper::PrintRoutingTableAllAt(Seconds(0));
        });
        Simulator::Schedule(Seconds(30.0), [&]() {
            Ipv4RoutingHelper::PrintRoutingTableAllAt(Seconds(0));
        });
        Simulator::Schedule(Seconds(50.0), [&]() {
            Ipv4RoutingHelper::PrintRoutingTableAllAt(Seconds(0));
        });
        Simulator::Schedule(Seconds(70.0), [&]() {
            Ipv4RoutingHelper::PrintRoutingTableAllAt(Seconds(0));
        });
    }

    if (pingVisibility) {
        Packet::EnablePrinting();
        PointToPointHelper::EnablePcapAll("rip-simple");
    }

    Simulator::Stop(Seconds(100.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}