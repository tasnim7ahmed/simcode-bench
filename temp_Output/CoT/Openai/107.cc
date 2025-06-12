#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/packet-sink.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FragmentationIpv6TwoMtu");

void
PacketReceiveCallback(Ptr<const Packet> packet, const Address &addr)
{
  static std::ofstream recvLog("fragmentation-ipv6-two-mtu.tr", std::ios_base::app);
  recvLog << Simulator::Now().GetSeconds() << " RX " << packet->GetSize() << " bytes\n";
}

int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);

  NodeContainer n0, n1, r;
  n0.Create(1);
  n1.Create(1);
  r.Create(1);

  NodeContainer csma1Nodes;
  csma1Nodes.Add(n0.Get(0));
  csma1Nodes.Add(r.Get(0));

  NodeContainer csma2Nodes;
  csma2Nodes.Add(r.Get(0));
  csma2Nodes.Add(n1.Get(0));

  CsmaHelper csma1;
  csma1.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma1.SetChannelAttribute("Delay", StringValue("2ms"));
  csma1.SetDeviceAttribute("Mtu", UintegerValue(5000));

  NetDeviceContainer csma1Devs = csma1.Install(csma1Nodes);

  CsmaHelper csma2;
  csma2.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma2.SetChannelAttribute("Delay", StringValue("2ms"));
  csma2.SetDeviceAttribute("Mtu", UintegerValue(1500));

  NetDeviceContainer csma2Devs = csma2.Install(csma2Nodes);

  InternetStackHelper internetv6;
  internetv6.SetIpv4StackInstall(false);
  internetv6.Install(n0);
  internetv6.Install(r);
  internetv6.Install(n1);

  Ipv6AddressHelper ipv6Addr1;
  ipv6Addr1.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer iface1 = ipv6Addr1.Assign(csma1Devs);
  iface1.SetForwarding(1, true);
  iface1.SetDefaultRouteInAllNodes(1);

  Ipv6AddressHelper ipv6Addr2;
  ipv6Addr2.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer iface2 = ipv6Addr2.Assign(csma2Devs);
  iface2.SetForwarding(0, true);
  iface2.SetDefaultRouteInAllNodes(0);

  // Manually set the default routes for client and server
  Ptr<Ipv6> ipv6_n0 = n0.Get(0)->GetObject<Ipv6>();
  Ptr<Ipv6> ipv6_n1 = n1.Get(0)->GetObject<Ipv6>();
  Ptr<Ipv6> ipv6_r = r.Get(0)->GetObject<Ipv6>();

  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> staticRoutingN0 = ipv6RoutingHelper.GetStaticRouting(ipv6_n0);
  Ptr<Ipv6StaticRouting> staticRoutingN1 = ipv6RoutingHelper.GetStaticRouting(ipv6_n1);

  staticRoutingN0->SetDefaultRoute(iface1.GetAddress(1,1), 1, 0);
  staticRoutingN1->SetDefaultRoute(iface2.GetAddress(0,1), 1, 0);

  // Set up UDP Echo Server on n1
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer(echoPort);
  ApplicationContainer serverApps = echoServer.Install(n1.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // UdpEchoClient on n0, targeting n1 global address
  UdpEchoClientHelper echoClient(iface2.GetAddress(1,1), echoPort);
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(4096));
  ApplicationContainer clientApps = echoClient.Install(n0.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Enable Queue Tracing
  csma1Devs.Get(0)->TraceConnect("MacTx", "fragmentation-ipv6-two-mtu.tr", MakeBoundCallback([](std::string file, Ptr<const Packet> packet) {
      static std::ofstream out(file, std::ios_base::app);
      out << Simulator::Now().GetSeconds() << " TX " << packet->GetSize() << " bytes\n";
    }));

  csma1Devs.Get(1)->TraceConnect("MacRx", "fragmentation-ipv6-two-mtu.tr", MakeBoundCallback([](std::string file, Ptr<const Packet> packet) {
      static std::ofstream out(file, std::ios_base::app);
      out << Simulator::Now().GetSeconds() << " RX " << packet->GetSize() << " bytes\n";
    }));

  csma2Devs.Get(0)->TraceConnect("MacTx", "fragmentation-ipv6-two-mtu.tr", MakeBoundCallback([](std::string file, Ptr<const Packet> packet) {
      static std::ofstream out(file, std::ios_base::app);
      out << Simulator::Now().GetSeconds() << " TX " << packet->GetSize() << " bytes\n";
    }));

  csma2Devs.Get(1)->TraceConnect("MacRx", "fragmentation-ipv6-two-mtu.tr", MakeBoundCallback([](std::string file, Ptr<const Packet> packet) {
      static std::ofstream out(file, std::ios_base::app);
      out << Simulator::Now().GetSeconds() << " RX " << packet->GetSize() << " bytes\n";
    }));

  // Packet Reception tracing at server
  Ptr<Application> app = serverApps.Get(0);
  Ptr<PacketSink> sink = DynamicCast<PacketSink>(app);
  if (sink)
  {
    sink->TraceConnectWithoutContext("Rx", MakeCallback(&PacketReceiveCallback));
  }

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}