#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-list-routing-helper.h"
#include "ns3/ping6-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/queue.h"
#include "ns3/drop-tail-queue.h"

using namespace ns3;

void
RxTrace (std::string context, Ptr<const Packet> packet, const Address &address)
{
  NS_LOG_INFO ("RxTrace: Packet received (" << packet->GetSize() << " bytes) at " << Simulator::Now ().GetSeconds () << "s, context: " << context);
}

void
QueueEnqueueTrace (std::string context, Ptr<const Packet> p)
{
  NS_LOG_INFO ("QueueEnqueue: Packet enqueued (" << p->GetSize() << " bytes) at " << Simulator::Now ().GetSeconds () << "s, context: " << context);
}

void
QueueDequeueTrace (std::string context, Ptr<const Packet> p)
{
  NS_LOG_INFO ("QueueDequeue: Packet dequeued (" << p->GetSize() << " bytes) at " << Simulator::Now ().GetSeconds () << "s, context: " << context);
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("LrWpanMac", LOG_LEVEL_INFO);
  LogComponentEnable ("Ping6Application", LOG_LEVEL_INFO);
  LogComponentEnable ("Ipv6L3Protocol", LOG_LEVEL_INFO);
  LogComponentEnableAll (LOG_PREFIX_TIME);

  // Create two nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Create LR-WPAN devices
  LrWpanHelper lrWpanHelper;
  NetDeviceContainer lrwpanDevices = lrWpanHelper.Install (nodes);

  // Set DropTail queue on devices' MAC
  for (uint32_t i = 0; i < lrwpanDevices.GetN (); ++i)
    {
      Ptr<LrWpanNetDevice> dev = DynamicCast<LrWpanNetDevice> (lrwpanDevices.Get (i));
      Ptr<Queue<Packet> > queue = CreateObject<DropTailQueue<Packet> > ();
      dev->GetMac ()->SetAttribute ("TxQueue", PointerValue (queue));
      std::ostringstream oss;
      oss << "/NodeList/" << nodes.Get (i)->GetId () << "/DeviceList/" << dev->GetIfIndex () << "/$ns3::LrWpanNetDevice/Mac/TxQueue";
      Config::Connect (oss.str () + "/Enqueue", MakeCallback (&QueueEnqueueTrace));
      Config::Connect (oss.str () + "/Dequeue", MakeCallback (&QueueDequeueTrace));
    }

  // Assign PAN ID
  lrWpanHelper.AssociateToPan (lrwpanDevices, 0x1234);

  // Install Internet stack (6LoWPAN-over-802.15.4)
  InternetStackHelper internetv6;
  internetv6.SetIpv4StackInstall (false);
  internetv6.Install (nodes);

  SixLowPanHelper sixlowpanHelper;
  NetDeviceContainer sixLowPanDevices = sixlowpanHelper.Install (lrwpanDevices);

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer ifaces = ipv6.Assign (sixLowPanDevices);
  // Enable forwarding for all interfaces
  for (uint32_t i = 0; i < ifaces.GetN (); ++i)
    {
      ifaces.SetForwarding (i, true);
      ifaces.SetDefaultRouteInAllNodes (i);
    }

  // Set up tracing for physical layer receptions (packet receptions)
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      std::ostringstream oss;
      oss << "/NodeList/" << nodes.Get (i)->GetId () << "/DeviceList/0/$ns3::LrWpanNetDevice/Phy/RxEnd";
      Config::Connect (oss.str (), MakeCallback (&RxTrace));
    }

  // Set up Ping6 application: Node 0 pings Node 1
  Ping6Helper ping6;
  ping6.SetLocal (ifaces.GetAddress (0, 1)); // link-local OR
  ping6.SetRemote (ifaces.GetAddress (1, 1)); // link-local of node 1
  ping6.SetAttribute ("Verbose", BooleanValue (true));
  ping6.SetAttribute ("Interval", TimeValue (Seconds (2.0)));
  ping6.SetAttribute ("Size", UintegerValue (32));
  ping6.SetAttribute ("Count", UintegerValue (5));
  ApplicationContainer appPing = ping6.Install (nodes.Get (0));
  appPing.Start (Seconds (1.0));
  appPing.Stop (Seconds (15.0));

  // Tracing queue and receptions to file
  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("wsn-ping6.tr");
  lrWpanHelper.EnableAsciiAll (stream);
  lrWpanHelper.EnablePcapAll ("wsn-ping6");

  // Enable flow monitor to log packet flow details
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();

  monitor->SerializeToXmlFile ("wsn-ping6-flowmon.xml", true, true);

  Simulator::Destroy ();
  return 0;
}