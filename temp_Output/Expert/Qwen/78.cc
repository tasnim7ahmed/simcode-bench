#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/rip-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RipSimpleRouting");

int main(int argc, char *argv[]) {
    bool verbose = false;
    bool printRoutingTables = false;
    bool showPings = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Enable log components", verbose);
    cmd.AddValue("printRoutingTable", "Print routing tables at 30s", printRoutingTables);
    cmd.AddValue("showPings", "Show ping RTTs", showPings);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("RipSimpleRouting", LOG_LEVEL_INFO);
        LogComponentEnable("RipHelper", LOG_LEVEL_ALL);
        LogComponentEnable("Rip", LOG_LEVEL_ALL);
        LogComponentEnable("Ipv4Interface", LOG_LEVEL_ALL);
        LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_ALL);
    }

    Time::SetResolution(Time::NS);
    NodeContainer nodes;
    nodes.Create(6); // SRC, A, B, C, D, DST

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));

    NetDeviceContainer devicesSRCtoA = p2p.Install(nodes.Get(0), nodes.Get(1)); // SRC -> A
    NetDeviceContainer devicesAtoB = p2p.Install(nodes.Get(1), nodes.Get(2)); // A -> B
    NetDeviceContainer devicesAtoC = p2p.Install(nodes.Get(1), nodes.Get(3)); // A -> C
    NetDeviceContainer devicesBtoD = p2p.Install(nodes.Get(2), nodes.Get(4)); // B -> D
    NetDeviceContainer devicesCtoD = p2p.Install(nodes.Get(3), nodes.Get(4)); // C -> D
    NetDeviceContainer devicesDtoDST = p2p.Install(nodes.Get(4), nodes.Get(5)); // D -> DST

    RipHelper rip;
    rip.SetInterfaceMetric(nodes.Get(3), 2, 10); // C to D link cost 10
    rip.SetSplitHorizonStrategy(RipNg::POISON_REVERSE);

    Ipv4ListRoutingHelper listRH;
    listRH.Add(rip, 0);

    InternetStackHelper internet;
    internet.SetRoutingHelper(listRH);
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    ipv4.Assign(devicesSRCtoA);
    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    ipv4.Assign(devicesAtoB);
    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    ipv4.Assign(devicesAtoC);
    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    ipv4.Assign(devicesBtoD);
    ipv4.SetBase("10.0.4.0", "255.255.255.0");
    ipv4.Assign(devicesCtoD);
    ipv4.SetBase("10.0.5.0", "255.255.255.0");
    ipv4.Assign(devicesDtoDST);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    V4PingHelper ping(nodes.Get(5)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal());
    ping.SetAttribute("Verbose", BooleanValue(showPings));
    ApplicationContainer apps = ping.Install(nodes.Get(0));
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(60.0));

    if (printRoutingTables) {
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            Ptr<Ipv4> ipv4Node = nodes.Get(i)->GetObject<Ipv4>();
            std::ostringstream oss;
            oss << "/NodeList/" << i << "/$ns3::Ipv4L3Protocol/RoutingTable";
            Config::ConnectWithoutContext(oss.str(), MakeCallback(&Rip::PrintRoutingTableAt, nodes.Get(i)));
        }
        Simulator::Schedule(Seconds(30.0), &Ipv4GlobalRoutingHelper::PrintRoutingTablesAt, Seconds(30.0));
    }

    // Link failure between B and D at 40s
    Simulator::Schedule(Seconds(40.0), &PointToPointNetDevice::SetLinkEnabled, devicesBtoD.Get(0), false);
    Simulator::Schedule(Seconds(40.0), &PointToPointNetDevice::SetLinkEnabled, devicesBtoD.Get(1), false);
    // Recovery after 44s
    Simulator::Schedule(Seconds(44.0), &PointToPointNetDevice::SetLinkEnabled, devicesBtoD.Get(0), true);
    Simulator::Schedule(Seconds(44.0), &PointToPointNetDevice::SetLinkEnabled, devicesBtoD.Get(1), true);

    Simulator::Stop(Seconds(60.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}