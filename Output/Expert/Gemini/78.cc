#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/rip.h"
#include "ns3/applications-module.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include "ns3/command-line.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool verbose = false;
    bool printRoutingTables = false;
    bool pingVisibility = false;

    CommandLine cmd;
    cmd.AddValue("verbose", "Enable verbose RIP output", verbose);
    cmd.AddValue("printRoutingTables", "Print routing tables regularly", printRoutingTables);
    cmd.AddValue("pingVisibility", "Enable ping visibility", pingVisibility);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    NodeContainer routerA, routerB, routerC, routerD;
    routerA.Create(1);
    routerB.Create(1);
    routerC.Create(1);
    routerD.Create(1);

    InternetStackHelper internet;
    internet.Install(nodes);
    internet.Install(routerA);
    internet.Install(routerB);
    internet.Install(routerC);
    internet.Install(routerD);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer srcA = p2p.Install(nodes.Get(0), routerA.Get(0));
    NetDeviceContainer aB = p2p.Install(routerA.Get(0), routerB.Get(0));
    NetDeviceContainer aC = p2p.Install(routerA.Get(0), routerC.Get(0));
    NetDeviceContainer bD = p2p.Install(routerB.Get(0), routerD.Get(0));
    NetDeviceContainer cD = p2p.Install(routerC.Get(0), routerD.Get(0));
    NetDeviceContainer dDst = p2p.Install(routerD.Get(0), nodes.Get(1));

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

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Rip rip;
    rip.SetSplitHorizon(true);
    rip.SetSplitHorizonPoisonReverse(true);

    RipNgHelper ripNgHelper;
    ripNgHelper.SetSplitHorizon(true);
    ripNgHelper.SetSplitHorizonPoisonReverse(true);

    ripNgHelper.Install(routerA);
    ripNgHelper.Install(routerB);
    ripNgHelper.Install(routerC);
    ripNgHelper.Install(routerD);

    Ipv4 lAddressA = iAB.GetAddress(0);
    Ipv4 lAddressB = iAB.GetAddress(1);
    Ipv4 lAddressC = iCD.GetAddress(0);
    Ipv4 lAddressD = iCD.GetAddress(1);

    Rip::AddRouteToMetric(lAddressA, 1);
    Rip::AddRouteToMetric(lAddressB, 1);
    Rip::AddRouteToMetric(lAddressC, 10);
    Rip::AddRouteToMetric(lAddressD, 10);

    if (verbose) {
        LogComponentEnable("Rip", LOG_LEVEL_ALL);
        LogComponentEnable("RipNg", LOG_LEVEL_ALL);
    }

    if (printRoutingTables) {
        Simulator::Schedule(Seconds(1), &Rip::PrintRoutingTables, routerA.Get(0));
        Simulator::Schedule(Seconds(1), &Rip::PrintRoutingTables, routerB.Get(0));
        Simulator::Schedule(Seconds(1), &Rip::PrintRoutingTables, routerC.Get(0));
        Simulator::Schedule(Seconds(1), &Rip::PrintRoutingTables, routerD.Get(0));
        Simulator::Schedule(Seconds(10), &Rip::PrintRoutingTables, routerA.Get(0));
        Simulator::Schedule(Seconds(10), &Rip::PrintRoutingTables, routerB.Get(0));
        Simulator::Schedule(Seconds(10), &Rip::PrintRoutingTables, routerC.Get(0));
        Simulator::Schedule(Seconds(10), &Rip::PrintRoutingTables, routerD.Get(0));
        Simulator::Schedule(Seconds(30), &Rip::PrintRoutingTables, routerA.Get(0));
        Simulator::Schedule(Seconds(30), &Rip::PrintRoutingTables, routerB.Get(0));
        Simulator::Schedule(Seconds(30), &Rip::PrintRoutingTables, routerC.Get(0));
        Simulator::Schedule(Seconds(30), &Rip::PrintRoutingTables, routerD.Get(0));
        Simulator::Schedule(Seconds(42), &Rip::PrintRoutingTables, routerA.Get(0));
        Simulator::Schedule(Seconds(42), &Rip::PrintRoutingTables, routerB.Get(0));
        Simulator::Schedule(Seconds(42), &Rip::PrintRoutingTables, routerC.Get(0));
        Simulator::Schedule(Seconds(42), &Rip::PrintRoutingTables, routerD.Get(0));
        Simulator::Schedule(Seconds(46), &Rip::PrintRoutingTables, routerA.Get(0));
        Simulator::Schedule(Seconds(46), &Rip::PrintRoutingTables, routerB.Get(0));
        Simulator::Schedule(Seconds(46), &Rip::PrintRoutingTables, routerC.Get(0));
        Simulator::Schedule(Seconds(46), &Rip::PrintRoutingTables, routerD.Get(0));
        Simulator::Schedule(Seconds(60), &Rip::PrintRoutingTables, routerA.Get(0));
        Simulator::Schedule(Seconds(60), &Rip::PrintRoutingTables, routerB.Get(0));
        Simulator::Schedule(Seconds(60), &Rip::PrintRoutingTables, routerC.Get(0));
        Simulator::Schedule(Seconds(60), &Rip::PrintRoutingTables, routerD.Get(0));
    }

    ApplicationContainer sinkApp;
    V4PingHelper ping(iDDst.GetAddress(1));
    ping.SetAttribute("Verbose", BooleanValue(pingVisibility));
    sinkApp.Add(ping.Install(nodes.Get(0)));
    sinkApp.Start(Seconds(1));
    sinkApp.Stop(Seconds(70));

    Simulator::Schedule(Seconds(40), [&]() {
        std::cout << "Link Failure B-D" << std::endl;
        bD.Get(0)->GetChannel()->Dispose();
        bD.Get(1)->GetChannel()->Dispose();
    });

    Simulator::Schedule(Seconds(44), [&]() {
        std::cout << "Link Recovery B-D" << std::endl;
        NetDeviceContainer bD_new = p2p.Install(routerB.Get(0), routerD.Get(0));
        Ipv4InterfaceContainer iBD_new = ipv4.Assign(bD_new);
    });

    Simulator::Stop(Seconds(70));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}