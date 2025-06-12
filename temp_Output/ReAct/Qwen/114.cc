#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/mobility-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WsnPing6Simulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_ALL);
    LogComponentEnable("Ipv6Interface", LOG_LEVEL_ALL);
    LogComponentEnable("WsnPing6Simulation", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    LrWpanHelper lrWpanHelper;
    NetDeviceContainer devices = lrWpanHelper.Install(nodes);

    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.Install(nodes);

    Ipv6AddressHelper ipv6;
    Ipv6Prefix prefix = Ipv6Prefix(64);
    ipv6.SetBase(Ipv6Address("2001:db01::"), prefix);
    Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

    interfaces.SetForwarding(0, true);
    interfaces.SetDefaultRouteInAllNodes();

    Ping6Helper ping6;
    ping6.SetLocal(nodes.Get(0)->GetId(), interfaces.GetAddress(1, 0));
    ping6.SetRemote(interfaces.GetAddress(1, 0));
    ping6.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping6.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    ping6.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    ApplicationContainer apps = ping6.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("wsn-ping6.tr");
    lrWpanHelper.EnableAsciiAll(stream);

    Config::SetDefault("ns3::QueueBase::MaxSize", StringValue("100p"));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}