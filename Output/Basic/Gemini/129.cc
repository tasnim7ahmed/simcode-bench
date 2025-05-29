#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/dhcp-helper.h"
#include "ns3/tap-bridge-module.h"
#include "ns3/csma-module.h"
#include "ns3/bridge-module.h"
#include "ns3/vlan-tag.h"
#include "ns3/packet.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VlanDhcpRouting");

int main(int argc, char *argv[]) {
  bool tracing = true;
  uint32_t numHostsPerVlan = 2;

  CommandLine cmd;
  cmd.AddValue("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue("numHostsPerVlan", "Number of hosts per VLAN", numHostsPerVlan);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetFilter("UdpClient", LOG_LEVEL_INFO);
  LogComponent::SetFilter("UdpServer", LOG_LEVEL_INFO);

  NodeContainer router, switchNode;
  router.Create(1);
  switchNode.Create(1);

  NodeContainer vlan1Hosts, vlan2Hosts, vlan3Hosts;
  vlan1Hosts.Create(numHostsPerVlan);
  vlan2Hosts.Create(numHostsPerVlan);
  vlan3Hosts.Create(numHostsPerVlan);

  InternetStackHelper stack;
  stack.Install(router);
  stack.Install(vlan1Hosts);
  stack.Install(vlan2Hosts);
  stack.Install(vlan3Hosts);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  NetDeviceContainer routerVlan1Devs = csma.Install(NodeContainer(router.Get(0), vlan1Hosts));
  NetDeviceContainer routerVlan2Devs = csma.Install(NodeContainer(router.Get(0), vlan2Hosts));
  NetDeviceContainer routerVlan3Devs = csma.Install(NodeContainer(router.Get(0), vlan3Hosts));

  NetDeviceContainer switchRouterDevs = csma.Install(NodeContainer(router.Get(0), switchNode.Get(0)));

  VlanTag tag1, tag2, tag3;
  tag1.vlanId = 10;
  tag2.vlanId = 20;
  tag3.vlanId = 30;

  NetDeviceContainer vlan1Tagged, vlan2Tagged, vlan3Tagged;
  for (uint32_t i = 1; i < routerVlan1Devs.GetN(); ++i) {
    vlan1Tagged.Add(VlanTag::Install(routerVlan1Devs.Get(i), tag1));
  }
  for (uint32_t i = 1; i < routerVlan2Devs.GetN(); ++i) {
    vlan2Tagged.Add(VlanTag::Install(routerVlan2Devs.Get(i), tag2));
  }
  for (uint32_t i = 1; i < routerVlan3Devs.GetN(); ++i) {
    vlan3Tagged.Add(VlanTag::Install(routerVlan3Devs.Get(i), tag3));
  }

  Ipv4AddressHelper address;
  address.SetBase("10.1.10.0", "255.255.255.0");
  Ipv4InterfaceContainer vlan1Interfaces = address.Assign(routerVlan1Devs);
  address.SetBase("10.1.20.0", "255.255.255.0");
  Ipv4InterfaceContainer vlan2Interfaces = address.Assign(routerVlan2Devs);
  address.SetBase("10.1.30.0", "255.255.255.0");
  Ipv4InterfaceContainer vlan3Interfaces = address.Assign(routerVlan3Devs);

  address.SetBase("10.1.40.0", "255.255.255.0");
  Ipv4InterfaceContainer switchRouterInterfaces = address.Assign(switchRouterDevs);

  Ipv4Address routerVlan1Address = vlan1Interfaces.GetAddress(0);
  Ipv4Address routerVlan2Address = vlan2Interfaces.GetAddress(0);
  Ipv4Address routerVlan3Address = vlan3Interfaces.GetAddress(0);
  Ipv4Address routerSwitchAddress = switchRouterInterfaces.GetAddress(0);

  DhcpHelper dhcpHelper;

  dhcpHelper.EnableAsciiTraces("/dhcp-trace", routerVlan1Devs.Get(0));

  Ptr<Node> dhcpServerNode = router.Get(0);

  dhcpHelper.AddAddressPool(Ipv4Address("10.1.10.10"), Ipv4Address("10.1.10.20"), Ipv4Mask("255.255.255.0"));
  dhcpHelper.AddAddressPool(Ipv4Address("10.1.20.10"), Ipv4Address("10.1.20.20"), Ipv4Mask("255.255.255.0"));
  dhcpHelper.AddAddressPool(Ipv4Address("10.1.30.10"), Ipv4Address("10.1.30.20"), Ipv4Mask("255.255.255.0"));

  dhcpHelper.Install(routerVlan1Devs.Get(0), dhcpServerNode);
  dhcpHelper.Install(routerVlan2Devs.Get(0), dhcpServerNode);
  dhcpHelper.Install(routerVlan3Devs.Get(0), dhcpServerNode);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(vlan1Hosts.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(vlan1Interfaces.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(vlan2Hosts.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  if (tracing) {
    csma.EnablePcap("vlan-dhcp-routing", routerVlan1Devs.Get(0), true);
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}