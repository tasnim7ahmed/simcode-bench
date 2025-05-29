#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-radvd-helper.h"
#include "ns3/ipv6-radvd-interface.h"
#include "ns3/ipv6-interface.h"
#include "ns3/ipv6-interface-container.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/icmpv6-echo-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RadvdPingExample");

void
RxTrace (std::string context, Ptr<const Packet> packet, const Address &address)
{
  std::ofstream out ("radvd.tr", std::ios::app);
  out << Simulator::Now ().GetSeconds ()
      << " " << context
      << " Packet received, size: " << packet->GetSize ()
      << std::endl;
}

void
QueueTrace (std::string context, Ptr<const Packet> packet)
{
  std::ofstream out ("radvd.tr", std::ios::app);
  out << Simulator::Now ().GetSeconds ()
      << " " << context
      << " Queue event, size: " << packet->GetSize ()
      << std::endl;
}

int
main (int argc, char *argv[])
{
  // Enable logging if needed
  // LogComponentEnable("RadvdPingExample", LOG_LEVEL_INFO);

  // Create 3 nodes: n0, R (router), n1
  NodeContainer n0, n1, router;
  n0.Create(1);
  n1.Create(1);
  router.Create(1);

  // Create two CSMA channels: n0 <-> R and R <-> n1
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  // n0 <-> R network
  NodeContainer net1;
  net1.Add(n0.Get(0));
  net1.Add(router.Get(0));

  // R <-> n1 network
  NodeContainer net2;
  net2.Add(router.Get(0));
  net2.Add(n1.Get(0));

  NetDeviceContainer ndc_net1 = csma.Install(net1);
  NetDeviceContainer ndc_net2 = csma.Install(net2);

  // Install IPv6 Internet stack
  InternetStackHelper internetv6;
  internetv6.SetIpv4StackInstall(false);
  internetv6.Install(n0);
  internetv6.Install(router);
  internetv6.Install(n1);

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6_1;
  ipv6_1.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer ifc_net1 = ipv6_1.Assign(ndc_net1);
  ifc_net1.SetRouter(1, true); // R is router for net1

  Ipv6AddressHelper ipv6_2;
  ipv6_2.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer ifc_net2 = ipv6_2.Assign(ndc_net2);
  ifc_net2.SetRouter(0, true); // R is router for net2

  // Set all interfaces (except local-loop) as up
  for (uint32_t i = 0; i < ifc_net1.GetN (); i++)
    ifc_net1.SetForwarding(i, true);

  for (uint32_t i = 0; i < ifc_net2.GetN (); i++)
    ifc_net2.SetForwarding(i, true);

  // Configure RADVD (Router Advertisement Daemon) on router
  Ipv6RadvdHelper radvdHelper;
  {
    Ptr<Node> r = router.Get(0);

    // Configure on net1 (ifIndex 0 on router's NetDevices, or use GetInterfaceForDevice)
    uint32_t ifIndex1 = r->GetObject<Ipv6> ()->GetInterfaceForDevice (ndc_net1.Get(1));
    Ptr<Ipv6RadvdInterface> radvdIf1 = CreateObject<Ipv6RadvdInterface> ();
    radvdIf1->SetInterface(ifIndex1);

    // Configure on net2 (ifIndex 1 on router's NetDevices)
    uint32_t ifIndex2 = r->GetObject<Ipv6> ()->GetInterfaceForDevice (ndc_net2.Get(0));
    Ptr<Ipv6RadvdInterface> radvdIf2 = CreateObject<Ipv6RadvdInterface> ();
    radvdIf2->SetInterface(ifIndex2);

    // Add both interfaces to the Radvd
    Ptr<Ipv6Radvd> radvd = CreateObject<Ipv6Radvd> ();
    radvd->AddRadvdInterface(radvdIf1);
    radvd->AddRadvdInterface(radvdIf2);
    r->AggregateObject(radvd);

    radvdHelper.Install(r);
  }

  // Set default routes to both n0 and n1
  Ptr<Ipv6StaticRouting> n0Routing = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (n0.Get(0)->GetObject<Ipv6> ()->GetRoutingProtocol ());
  n0Routing->SetDefaultRoute(ifc_net1.GetAddress(1, 1), ifc_net1.GetInterfaceIndex(0));

  Ptr<Ipv6StaticRouting> n1Routing = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (n1.Get(0)->GetObject<Ipv6> ()->GetRoutingProtocol ());
  n1Routing->SetDefaultRoute(ifc_net2.GetAddress(0, 1), ifc_net2.GetInterfaceIndex(1));

  // Install ICMPv6 Echo application (Ping) on n0 to ping n1's address
  uint16_t echoPort = 9;
  Ipv6Address remoteAddr = ifc_net2.GetAddress(1,1); // n1's address

  Icmpv6EchoHelper echoHelper(remoteAddr);
  echoHelper.SetAttribute("MaxPackets", UintegerValue(5));
  echoHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoHelper.SetAttribute("PacketSize", UintegerValue(56));
  ApplicationContainer pingApps = echoHelper.Install(n0.Get(0));
  pingApps.Start(Seconds(2.0));
  pingApps.Stop(Seconds(15.0));

  // Enable tracing on queue (for both CSMA devices)
  csma.EnablePcapAll("radvd", false);
  // For extra explicit packet/queue event tracing:
  for (uint32_t i = 0; i < ndc_net1.GetN (); ++i)
    {
      std::ostringstream oss;
      oss << "/NodeList/" << net1.Get(i)->GetId ()
          << "/DeviceList/" << ndc_net1.Get(i)->GetIfIndex () << "/$ns3::CsmaNetDevice/TxQueue/Enqueue";
      Config::Connect(oss.str (), MakeCallback(&QueueTrace));
    }
  for (uint32_t i = 0; i < ndc_net2.GetN (); ++i)
    {
      std::ostringstream oss;
      oss << "/NodeList/" << net2.Get(i)->GetId ()
          << "/DeviceList/" << ndc_net2.Get(i)->GetIfIndex () << "/$ns3::CsmaNetDevice/TxQueue/Enqueue";
      Config::Connect(oss.str (), MakeCallback(&QueueTrace));
    }

  // Enable packet reception tracing (at n1)
  Config::Connect("/NodeList/2/ApplicationList/*/$ns3::PacketSink/RxWithAddresses", MakeCallback(&RxTrace));
  Config::Connect("/NodeList/2/DeviceList/0/$ns3::CsmaNetDevice/MacRx", MakeCallback(&RxTrace));
  Config::Connect("/NodeList/2/DeviceList/1/$ns3::CsmaNetDevice/MacRx", MakeCallback(&RxTrace));

  // Enable global ICMPv6 reception trace
  Config::Connect("/NodeList/2/$ns3::Ipv6L3Protocol/Receive", MakeCallback(&RxTrace));
  Config::Connect("/NodeList/0/$ns3::Ipv6L3Protocol/Receive", MakeCallback(&RxTrace));

  // Run simulation
  Simulator::Stop(Seconds(20.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}