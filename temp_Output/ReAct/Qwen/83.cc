#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-interface-container.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpCsmaSimulation");

class UdpCsmaSimulation
{
public:
  UdpCsmaSimulation();
  void Setup(bool useIpv6, bool enableLogging);
  void Run();

private:
  NodeContainer nodes;
  CsmaHelper csma;
  InternetStackHelper internet;
  Ipv4AddressHelper ipv4AddrHelper;
  Ipv6AddressHelper ipv6AddrHelper;
  Ipv4InterfaceContainer ipv4Interfaces;
  Ipv6InterfaceContainer ipv6Interfaces;
  UdpEchoServerHelper serverApp;
  UdpEchoClientHelper clientApp;
  ApplicationContainer serverApps;
  ApplicationContainer clientApps;
};

UdpCsmaSimulation::UdpCsmaSimulation()
  : csma(),
    internet(),
    ipv4AddrHelper("10.1.1.0", "255.255.255.0"),
    ipv6AddrHelper("2001:db01::", Ipv6Prefix(64)),
    serverApp(9),
    clientApp(Ipv4Address(), 9)
{
}

void
UdpCsmaSimulation::Setup(bool useIpv6, bool enableLogging)
{
  if (enableLogging)
  {
    LogComponentEnable("UdpCsmaSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_ALL);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_ALL);
  }

  nodes.Create(2);

  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(6560)));

  NetDeviceContainer devices = csma.Install(nodes);

  if (useIpv6)
  {
    internet.SetIpv4StackEnabled(false);
    internet.SetIpv6StackEnabled(true);
    internet.Install(nodes);

    ipv6Interfaces = ipv6AddrHelper.Assign(devices);
    ipv6AddrHelper.NewNetwork();
    ipv6Interfaces.SetForwarding(1, true);
    ipv6Interfaces.SetDefaultRouteInAllNodes(1);

    serverApp = UdpEchoServerHelper(9);
    clientApp = UdpEchoClientHelper(ipv6Interfaces.GetAddress(1, 1), 9);
  }
  else
  {
    internet.SetIpv4StackEnabled(true);
    internet.SetIpv6StackEnabled(false);
    internet.Install(nodes);

    ipv4Interfaces = ipv4AddrHelper.Assign(devices);

    serverApp = UdpEchoServerHelper(9);
    clientApp = UdpEchoClientHelper(ipv4Interfaces.GetAddress(1), 9);
  }

  serverApps = serverApp.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  clientApp.SetAttribute("MaxPackets", UintegerValue(320));
  clientApp.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
  clientApp.SetAttribute("PacketSize", UintegerValue(1024));

  clientApps = clientApp.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));
}

void
UdpCsmaSimulation::Run()
{
  NS_LOG_INFO("Starting simulation.");
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  NS_LOG_INFO("Simulation finished.");
}

int
main(int argc, char *argv[])
{
  bool useIpv6 = false;
  bool enableLogging = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("useIpv6", "Use IPv6 addressing instead of IPv4", useIpv6);
  cmd.AddValue("enableLogging", "Enable detailed logging", enableLogging);
  cmd.Parse(argc, argv);

  UdpCsmaSimulation sim;
  sim.Setup(useIpv6, enableLogging);
  sim.Run();

  return 0;
}