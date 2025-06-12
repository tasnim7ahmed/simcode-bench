#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/trace-source-accessor.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpTraceClientServer");

class UdpTraceClientServer {
public:
  static void CreateSimulation(bool useIpv6, bool enableLogging) {
    if (enableLogging) {
      LogComponentEnable("UdpTraceClientServer", LOG_LEVEL_ALL);
      LogComponentEnable("UdpClient", LOG_LEVEL_ALL);
      LogComponentEnable("UdpServer", LOG_LEVEL_ALL);
    }

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.SetIpv4StackInstall(useIpv6 ? false : true);
    stack.SetIpv6StackInstall(useIpv6 ? true : false);
    stack.Install(nodes);

    Ipv4InterfaceContainer ipv4Interfaces;
    Ipv6InterfaceContainer ipv6Interfaces;

    if (!useIpv6) {
      ipv4Interfaces = Ipv4AddressHelper().Assign(devices);
      Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    } else {
      ipv6Interfaces = Ipv6AddressHelper().Assign(devices);
      ipv6Interfaces.SetBase(1, Ipv6Prefix(64));
    }

    uint16_t port = 4000;

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpTraceClientHelper client;
    if (!useIpv6) {
      client = UdpTraceClientHelper(ipv4Interfaces.GetAddress(1), port, "src/ns-3-dev/examples/udp/tracefile.tr");
    } else {
      client = UdpTraceClientHelper(ipv6Interfaces.GetAddress(1, 1), port, "src/ns-3-dev/examples/udp/tracefile.tr");
    }
    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
  }
};

int main(int argc, char *argv[]) {
  bool useIpv6 = false;
  bool enableLogging = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("useIpv6", "Use IPv6 addressing instead of IPv4", useIpv6);
  cmd.AddValue("enableLogging", "Enable logging for simulation components", enableLogging);
  cmd.Parse(argc, argv);

  UdpTraceClientServer::CreateSimulation(useIpv6, enableLogging);
  return 0;
}