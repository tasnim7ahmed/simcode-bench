#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/radvd-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/packet-sink-helper.h"
#include <fstream>

using namespace ns3;

static void
PacketReceiveCallback(std::string context, Ptr<const Packet> packet, const Address &address)
{
  static std::ofstream outFile("radvd.tr", std::ios::app);
  outFile << Simulator::Now().GetSeconds()
          << " " << context
          << " Packet received, size=" << packet->GetSize() << std::endl;
}

static void
QueueTraceCallbackCsma(std::string context, Ptr<const QueueDisc> queue)
{
  static std::ofstream outFile("radvd.tr", std::ios::app);
  outFile << Simulator::Now().GetSeconds()
          << " Queue (" << context << ") packets: " << queue->GetNPackets () << std::endl;
}

int
main(int argc, char *argv[])
{
  LogComponentEnable("Ping6Application", LOG_LEVEL_INFO);

  // Create nodes: n0, router (R), n1
  NodeContainer n0, n1, router;
  n0.Create(1);
  n1.Create(1);
  router.Create(1);

  // Create CSMA helpers for two subnets
  CsmaHelper csma0;
  csma0.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  csma0.SetChannelAttribute("Delay", TimeValue(NanoSeconds(5000)));

  CsmaHelper csma1;
  csma1.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  csma1.SetChannelAttribute("Delay", TimeValue(NanoSeconds(5000)));

  // Each subnet: CSMA between R and n0/n1
  NodeContainer net0(router.Get(0), n0.Get(0));
  NodeContainer net1(router.Get(0), n1.Get(0));

  NetDeviceContainer dev0 = csma0.Install(net0);
  NetDeviceContainer dev1 = csma1.Install(net1);

  // Install Internet stack
  InternetStackHelper internetv6;
  internetv6.SetIpv4StackInstall(false);
  internetv6.Install(n0);
  internetv6.Install(n1);
  internetv6.Install(router);

  // Assign IPv6 addresses (each router interface gets one prefix)
  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer ifc0 = ipv6.Assign(dev0);
  ifc0.SetRouter(0, true); // Router interface as router
  ifc0.SetRouter(1, false);

  ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer ifc1 = ipv6.Assign(dev1);
  ifc1.SetRouter(0, true);
  ifc1.SetRouter(1, false);

  // By default, NS-3 disables global addresses. Let them go TENTATIVE->PREFERRED immediately.
  ifc0.SetForwarding(0, true);
  ifc0.SetDefaultRouteInAllNodes(0);

  ifc1.SetForwarding(0, true);
  ifc1.SetDefaultRouteInAllNodes(0);

  // RADVD (Router Advertisement Daemon)
  RadvdHelper radvdHelper;
  RadvdInterface routerIf0;
  routerIf0.SetInterfaceIndex(router.Get(0)->GetObject<Ipv6>()->GetInterfaceForDevice(dev0.Get(0)));
  routerIf0.SetPrefix(Ipv6Address("2001:1::"), 64);

  RadvdInterface routerIf1;
  routerIf1.SetInterfaceIndex(router.Get(0)->GetObject<Ipv6>()->GetInterfaceForDevice(dev1.Get(0)));
  routerIf1.SetPrefix(Ipv6Address("2001:2::"), 64);

  RadvdHelper::RadvdInterfaceContainer iFaceC;
  iFaceC.push_back(routerIf0);
  iFaceC.push_back(routerIf1);

  ApplicationContainer radvdApps = radvdHelper.Install(router.Get(0), iFaceC);

  radvdApps.Start(Seconds(1.0));
  radvdApps.Stop (Seconds(15.0));

  // Ping6 from n0 to n1's global address
  Ipv6Address dst = ifc1.GetAddress(1,1); // n1 global address in 2001:2::/64

  uint32_t packetSize = 56;
  uint32_t maxPackets = 5;
  Time interPacketInterval = Seconds(1.0);
  Ping6Helper ping6(dst);
  ping6.SetAttribute("MaxPackets", UintegerValue(maxPackets));
  ping6.SetAttribute("Interval", TimeValue(interPacketInterval));
  ping6.SetAttribute("PacketSize", UintegerValue(packetSize));
  ping6.SetAttribute("Verbose", BooleanValue(true));
  ApplicationContainer pingApps = ping6.Install(n0.Get(0));
  pingApps.Start(Seconds(2.0));
  pingApps.Stop(Seconds(15.0));

  // Connect tracing for queue and reception
  Ptr<NetDevice> n0Dev = dev0.Get(1);
  Ptr<NetDevice> n1Dev = dev1.Get(1);

  // Queue tracing (only applies if queue disc is present, so use NetDevice queue)
  Ptr<Queue<Packet>> q0 = DynamicCast<CsmaNetDevice>(dev0.Get(0))->GetQueue();
  Ptr<Queue<Packet>> q1 = DynamicCast<CsmaNetDevice>(dev1.Get(0))->GetQueue();

  if (!Config::LookupMatches("/NodeList/*/DeviceList/*/TxQueue/")) {
    // Connect manually if path does not match (for newer ns-3)
    if (q0)
    {
      q0->TraceConnectWithoutContext("PacketsInQueue", MakeCallback([](uint32_t oldVal, uint32_t newVal){
        static std::ofstream outFile("radvd.tr", std::ios::app);
        outFile << Simulator::Now().GetSeconds()
          << " Queue (dev0-router): New=" << newVal << std::endl;
      }));
    }
    if (q1)
    {
      q1->TraceConnectWithoutContext("PacketsInQueue", MakeCallback([](uint32_t oldVal, uint32_t newVal){
        static std::ofstream outFile("radvd.tr", std::ios::app);
        outFile << Simulator::Now().GetSeconds()
          << " Queue (dev1-router): New=" << newVal << std::endl;
      }));
    }
  }

  // Packet rx tracing
  n1Dev->TraceConnect("MacRx", "n1-MacRx",
      MakeBoundCallback(&PacketReceiveCallback, std::string("n1")));

  Ptr<Ipv6L3Protocol> ipv6L3N1 = n1.Get(0)->GetObject<Ipv6L3Protocol>();
  if (ipv6L3N1)
  {
    ipv6L3N1->TraceConnect("Rx", "n1-Ipv6Rx",
        MakeBoundCallback(&PacketReceiveCallback, std::string("n1-Ipv6Rx")));
  }

  Simulator::Stop(Seconds(16.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}