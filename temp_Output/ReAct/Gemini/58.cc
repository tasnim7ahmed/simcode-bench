#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-bbr.h"
#include "ns3/queue.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpBbrSimulation");

static void
CwndChange (std::string context, uint32_t oldCwnd, uint32_t newCwnd)
{
  std::cout << Simulator::Now ().GetSeconds () << " " << context << " Old CWnd: " << oldCwnd
            << " New CWnd: " << newCwnd << std::endl;
}

static void
QueueSizeChange (std::string context, uint32_t oldSize, uint32_t newSize)
{
  std::cout << Simulator::Now ().GetSeconds () << " " << context << " Old Size: " << oldSize
            << " New Size: " << newSize << std::endl;
}

static void
ThroughputChange (std::string context, Time oldTime, Time newTime, uint32_t byteCount)
{
    std::cout << Simulator::Now().GetSeconds() << " " << context << " Throughput: " << ((double)byteCount * 8) / (newTime.GetSeconds() - oldTime.GetSeconds()) / 1000000 << " Mbps" << std::endl;
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  LogComponent::SetFilter (
    "TcpBbrSimulation",
    Level::LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (4);

  NodeContainer senderR1 = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer r1R2 = NodeContainer (nodes.Get (1), nodes.Get (2));
  NodeContainer r2Receiver = NodeContainer (nodes.Get (2), nodes.Get (3));

  InternetStackHelper stack;
  stack.Install (nodes);

  PointToPointHelper p2pSenderR1;
  p2pSenderR1.SetDeviceAttribute ("DataRate", StringValue ("1000Mbps"));
  p2pSenderR1.SetChannelAttribute ("Delay", StringValue ("5ms"));
  NetDeviceContainer devicesSenderR1 = p2pSenderR1.Install (senderR1);

  PointToPointHelper p2pR1R2;
  p2pR1R2.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2pR1R2.SetChannelAttribute ("Delay", StringValue ("10ms"));
  NetDeviceContainer devicesR1R2 = p2pR1R2.Install (r1R2);

  PointToPointHelper p2pR2Receiver;
  p2pR2Receiver.SetDeviceAttribute ("DataRate", StringValue ("1000Mbps"));
  p2pR2Receiver.SetChannelAttribute ("Delay", StringValue ("5ms"));
  NetDeviceContainer devicesR2Receiver = p2pR2Receiver.Install (r2Receiver);

  AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesSenderR1 = address.Assign (devicesSenderR1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesR1R2 = address.Assign (devicesR1R2);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesR2Receiver = address.Assign (devicesR2Receiver);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t sinkPort = 50000;
  Address sinkAddress (InetSocketAddress (interfacesR2Receiver.GetAddress (1), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (3));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (100.0));

  TypeId tid = TypeId::LookupByName ("ns3::TcpBbr");
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (tid));

  uint32_t maxBytes = 0;
  V4TcpCongestionAvoidance bbr;
  bbr.SetProbeRttInterval(Seconds(10));
  bbr.SetProbeRttCwnd(4);

  BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (interfacesR2Receiver.GetAddress (1), sinkPort));
  source.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  ApplicationContainer sourceApps = source.Install (nodes.Get (0));
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds (100.0));

  AsciiTraceHelper ascii;
  p2pSenderR1.EnablePcapAll ("tcp-bbr-simulation");
  p2pR1R2.EnablePcapAll ("tcp-bbr-simulation");
  p2pR2Receiver.EnablePcapAll ("tcp-bbr-simulation");

  Config::Connect ("/ChannelList/0/TxQueue/Size", MakeCallback (&QueueSizeChange));
  Config::Connect ("/ChannelList/1/TxQueue/Size", MakeCallback (&QueueSizeChange));
  Config::Connect ("/ChannelList/2/TxQueue/Size", MakeCallback (&QueueSizeChange));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Schedule (Seconds (1.1), [&]() {
    std::ofstream cwndStream("cwnd.txt");
    Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeBoundCallback (&CwndChange, &cwndStream));

    TimeValue oldTime;
    uint32_t byteCount = 0;

    Simulator::ScheduleNow ([&](){
      oldTime = Simulator::Now();
    });

    Simulator::Schedule (Seconds(2), [&](){
      Simulator::ScheduleEvery(Seconds(1), [&](){
        Ptr<Application> app = sourceApps.Get(0);
        Ptr<BulkSendApplication> bulk = DynamicCast<BulkSendApplication>(app);
        Time newTime = Simulator::Now();
        byteCount = bulk->GetBytesTransmitted();
        ThroughputChange("Sender", oldTime, newTime, byteCount);
        oldTime = newTime;
      });
    });
  });

  Simulator::Stop (Seconds (100.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Flow Duration: " << i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds() << " s\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000  << " Mbps\n";
    }

  monitor->SerializeToXmlFile("tcp-bbr-simulation.flowmon", true, true);

  Simulator::Destroy ();

  return 0;
}