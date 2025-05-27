#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FragmentationIpv6");

int main(int argc, char* argv[]) {
  bool enablePcap = true;

  CommandLine cmd;
  cmd.AddValue("EnablePcap", "Enable PCAP tracing", enablePcap);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::Enable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponent::Enable("UdpEchoServerApplication", LOG_LEVEL_INFO);
  LogComponent::Enable("Ipv6L3Protocol", LOG_LEVEL_ALL);
  LogComponent::Enable("CsmaNetDevice", LOG_LEVEL_ALL);
  LogComponent::Enable("QueueDisc", LOG_LEVEL_ALL);

  // Create nodes
  NS_LOG_INFO("Create nodes.");
  NodeContainer nodes;
  nodes.Create(2);

  NodeContainer router;
  router.Create(1);

  // Create channel
  NS_LOG_INFO("Create channel.");
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer devices1 = csma.Install(NodeContainer(nodes.Get(0), router.Get(0)));
  NetDeviceContainer devices2 = csma.Install(NodeContainer(nodes.Get(1), router.Get(0)));

  // Install Internet Stack
  NS_LOG_INFO("Install Internet Stack.");
  InternetStackHelper internetv6;
  internetv6.Install(nodes);
  internetv6.Install(router);

  // Assign IPv6 addresses
  NS_LOG_INFO("Assign IPv6 Addresses.");
  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer interfaces1 = ipv6.Assign(devices1);

  ipv6.SetBase(Ipv6Address("2001:db8:2::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer interfaces2 = ipv6.Assign(devices2);

  // Configure IPv6 routing
  NS_LOG_INFO("Configure IPv6 Routing.");
  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> staticRouting = ipv6RoutingHelper.GetOrCreateRouting(router.Get(0)->GetObject<Ipv6>());
  staticRouting->AddRoutingTableEntry(Ipv6Address("2001:db8:1::"), 64, interfaces1.GetAddress(1,0));
  staticRouting->AddRoutingTableEntry(Ipv6Address("2001:db8:2::"), 64, interfaces2.GetAddress(1,0));

  // Set default routes on end nodes
  Ptr<Ipv6StaticRouting> staticRouting0 = ipv6RoutingHelper.GetOrCreateRouting(nodes.Get(0)->GetObject<Ipv6>());
  staticRouting0->SetDefaultRoute(interfaces1.GetAddress(1, 0), 0);
  Ptr<Ipv6StaticRouting> staticRouting1 = ipv6RoutingHelper.GetOrCreateRouting(nodes.Get(1)->GetObject<Ipv6>());
  staticRouting1->SetDefaultRoute(interfaces2.GetAddress(1, 0), 0);

  // Create UDP echo client-server application
  NS_LOG_INFO("Create UDP echo client-server application.");
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces2.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(2000)); // Larger than MTU to trigger fragmentation
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Tracing
  NS_LOG_INFO("Tracing.");
  AsciiTraceHelper ascii;
  csma.EnableAsciiAll(ascii.CreateFileStream("fragmentation-ipv6.tr"));

  if (enablePcap)
  {
    csma.EnablePcapAll("fragmentation-ipv6", false);
  }


  // Run simulation
  NS_LOG_INFO("Run Simulation.");
  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  NS_LOG_INFO("Done.");

  return 0;
}