#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/rip-ng.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/command-line.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool verbose = false;
    bool printRoutingTables = false;
    bool enablePing = false;

    CommandLine cmd;
    cmd.AddValue("verbose", "Enable RIPng verbose output", verbose);
    cmd.AddValue("printRoutingTables", "Print routing tables regularly", printRoutingTables);
    cmd.AddValue("enablePing", "Enable ICMP pings", enablePing);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("RipNg", LOG_LEVEL_ALL);
        LogComponentEnable("Ipv4RoutingTable", LOG_LEVEL_ALL);
    }

    NodeContainer srcNode, dstNode, routerA, routerB, routerC, routerD;
    srcNode.Create(1);
    dstNode.Create(1);
    routerA.Create(1);
    routerB.Create(1);
    routerC.Create(1);
    routerD.Create(1);

    InternetStackHelper internet;
    internet.Install(srcNode);
    internet.Install(dstNode);
    internet.Install(routerA);
    internet.Install(routerB);
    internet.Install(routerC);
    internet.Install(routerD);

    RipNgHelper ripNgRouting;
    ripNgRouting.SetSplitHorizon(true);
    ripNgRouting.SetPoisonReverse(true);

    Ipv4GlobalRoutingHelper g;
    g.PopulateRoutingTables();

    NetDeviceContainer srcA = CreateObject<PointToPointHelper>()->Install(srcNode.Get(0), routerA.Get(0));
    NetDeviceContainer aB = CreateObject<PointToPointHelper>()->Install(routerA.Get(0), routerB.Get(0));
    NetDeviceContainer aC = CreateObject<PointToPointHelper>()->Install(routerA.Get(0), routerC.Get(0));
    NetDeviceContainer bD = CreateObject<PointToPointHelper>()->Install(routerB.Get(0), routerD.Get(0));
    NetDeviceContainer cD = CreateObject<PointToPointHelper>()->Install(routerC.Get(0), routerD.Get(0));
    NetDeviceContainer dDst = CreateObject<PointToPointHelper>()->Install(routerD.Get(0), dstNode.Get(0));

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

    ripNgRouting.AddInterface(iAB.GetAddress(0), iAB.GetAddress(1));
    ripNgRouting.AddInterface(iAC.GetAddress(0), iAC.GetAddress(1));
    ripNgRouting.AddInterface(iBD.GetAddress(0), iBD.GetAddress(1));
    ripNgRouting.AddInterface(iCD.GetAddress(0), iCD.GetAddress(1));
    ripNgRouting.AddInterface(iDDst.GetAddress(0), iDDst.GetAddress(1));
    ripNgRouting.AddInterface(iSrcA.GetAddress(0), iSrcA.GetAddress(1));

    Ipv4Address bdAddr1 = iBD.GetAddress(0);
    Ipv4Address bdAddr2 = iBD.GetAddress(1);
    Ipv4Address cdAddr1 = iCD.GetAddress(0);
    Ipv4Address cdAddr2 = iCD.GetAddress(1);

    Simulator::Schedule(Seconds(40.0), [=]() {
        CreateObject<PointToPointHelper>()->DisableLink(routerB.Get(0), routerD.Get(0));
    });

    Simulator::Schedule(Seconds(44.0), [=]() {
         CreateObject<PointToPointHelper>()->EnableLink(routerB.Get(0), routerD.Get(0));
    });

    Config::Set("/NodeList/*/ApplicationList/*/$ns3::RipNg/Cost", IntegerValue(1));
    Config::Set("/NodeList/" + std::to_string(routerC.Get(0)->GetId()) + "/ApplicationList/*/$ns3::RipNg/Interfaces/" + cdAddr1.GetString() + "/Cost", IntegerValue(10));
    Config::Set("/NodeList/" + std::to_string(routerD.Get(0)->GetId()) + "/ApplicationList/*/$ns3::RipNg/Interfaces/" + cdAddr2.GetString() + "/Cost", IntegerValue(10));

    if (enablePing) {
        V4PingHelper ping(iDDst.GetAddress(1));
        ping.SetAttribute("Verbose", BooleanValue(true));
        Simulator::Schedule(Seconds(10), [&ping]() {
            ping.Send();
        });
    }

    if (printRoutingTables) {
        Simulator::Schedule(Seconds(20), [&]() {
            Ptr<Ipv4RoutingTable> routingTable =
                Simulator::GetContext()->GetNode()->GetObject<Ipv4>()->GetRoutingProtocol()->GetRoutingTable();
            routingTable->Print();
            Simulator::Schedule(Seconds(10), EventDeleter(new Event(Simulator::GetContext(), [&]() {
            Ptr<Ipv4RoutingTable> routingTable =
                Simulator::GetContext()->GetNode()->GetObject<Ipv4>()->GetRoutingProtocol()->GetRoutingTable();
                routingTable->Print();
            })));
        });
    }
    
    uint16_t port = 9;
    UdpEchoServerHelper echoServer (port);

    ApplicationContainer serverApps = echoServer.Install (dstNode.Get (0));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (100.0));

    UdpEchoClientHelper echoClient (iDDst.GetAddress(1), port);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
    ApplicationContainer clientApps = echoClient.Install (srcNode.Get (0));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (90.0));


    Simulator::Stop(Seconds(100));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}