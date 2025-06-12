#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/bbr-congestion-ops.h"
#include <fstream>

using namespace ns3;

static void
CwndChange (std::string context, uint32_t oldCwnd, uint32_t newCwnd)
{
  std::cout << Simulator::Now ().GetSeconds () << " " << context << " Old CWnd: " << oldCwnd
            << " New CWnd: " << newCwnd << std::endl;
}

static void
ThroughputMonitor (std::string context, Time oldTime, Time newTime, DataRate oldRxRate, DataRate newRxRate)
{
  std::cout << Simulator::Now ().GetSeconds () << " " << context << " Old RxRate: " << oldRxRate
            << " New RxRate: " << newRxRate << std::endl;
}

static void
QueueSizeMonitor (std::string context, Time time, uint32_t oldSize, uint32_t newSize)
{
  std::cout << Simulator::Now ().GetSeconds () << " " << context << " Old Queue Size: " << oldSize
            << " New Queue Size: " << newSize << std::endl;
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("TcpBbr", LOG_LEVEL_ALL);
  LogComponentEnable ("BulkSendApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (4);

  NodeContainer senderRouter;
  senderRouter.Add (nodes.Get (0));
  senderRouter.Add (nodes.Get (1));

  NodeContainer receiverRouter;
  receiverRouter.Add (nodes.Get (2));
  receiverRouter.Add (nodes.Get (3));

  NodeContainer routers;
  routers.Add (nodes.Get (1));
  routers.Add (nodes.Get (2));

  InternetStackHelper stack;
  stack.Install (nodes);

  PointToPointHelper pointToPointSenderRouter;
  pointToPointSenderRouter.SetDeviceAttribute ("DataRate", StringValue ("1000Mbps"));
  pointToPointSenderRouter.SetChannelAttribute ("Delay", StringValue ("5ms"));

  PointToPointHelper pointToPointReceiverRouter;
  pointToPointReceiverRouter.SetDeviceAttribute ("DataRate", StringValue ("1000Mbps"));
  pointToPointReceiverRouter.SetChannelAttribute ("Delay", StringValue ("5ms"));

  PointToPointHelper pointToPointRouters;
  pointToPointRouters.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPointRouters.SetChannelAttribute ("Delay", StringValue ("10ms"));
  TrafficControlHelper tch;
  uint16_t queueDiscId = tch.Install (routers);


  NetDeviceContainer senderRouterDevices;
  senderRouterDevices = pointToPointSenderRouter.Install (senderRouter);

  NetDeviceContainer receiverRouterDevices;
  receiverRouterDevices = pointToPointReceiverRouter.Install (receiverRouter);

  NetDeviceContainer routerDevices;
  routerDevices = pointToPointRouters.Install (routers);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer senderRouterInterfaces = address.Assign (senderRouterDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer routerInterfaces = address.Assign (routerDevices);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer receiverRouterInterfaces = address.Assign (receiverRouterDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 5000;
  BulkSendHelper source ("ns3::TcpBbr", InetSocketAddress (receiverRouterInterfaces.GetAddress (1), port));
  source.SetAttribute ("SendSize", UintegerValue (1448));
  source.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer sourceApps = source.Install (nodes.Get (0));
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds (99.0));

  PacketSinkHelper sink ("ns3::TcpBbr", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (3));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (100.0));

  Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::BulkSendApplication/CongestionWindow",
                   MakeCallback (&CwndChange));

  Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::PacketSink/RxRate",
                   MakeCallback (&ThroughputMonitor));

  Config::Connect ("/NodeList/1/DeviceList/1/$ns3::PointToPointNetDevice/TxQueue/QueueSize",
                   MakeCallback (&QueueSizeMonitor));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Schedule (Seconds(10.0), [](){
        Config::SetGlobal("TcpBbr::PacingGain", DoubleValue(2.89));
  });

  Simulator::Schedule (Seconds(20.0), [](){
        Config::SetGlobal("TcpBbr::PacingGain", DoubleValue(1.0));
  });

   Simulator::Schedule (Seconds(30.0), [](){
        Config::SetGlobal("TcpBbr::PacingGain", DoubleValue(2.89));
  });

  Simulator::Schedule (Seconds(40.0), [](){
        Config::SetGlobal("TcpBbr::PacingGain", DoubleValue(1.0));
  });

  Simulator::Schedule (Seconds(50.0), [](){
        Config::SetGlobal("TcpBbr::PacingGain", DoubleValue(2.89));
  });

  Simulator::Schedule (Seconds(60.0), [](){
        Config::SetGlobal("TcpBbr::PacingGain", DoubleValue(1.0));
  });

  Simulator::Schedule (Seconds(70.0), [](){
        Config::SetGlobal("TcpBbr::PacingGain", DoubleValue(2.89));
  });

  Simulator::Schedule (Seconds(80.0), [](){
        Config::SetGlobal("TcpBbr::PacingGain", DoubleValue(1.0));
  });

  Simulator::Schedule (Seconds(90.0), [](){
        Config::SetGlobal("TcpBbr::PacingGain", DoubleValue(2.89));
  });

  pointToPointSenderRouter.EnablePcapAll ("bbr-sender-router");
  pointToPointReceiverRouter.EnablePcapAll ("bbr-receiver-router");
  pointToPointRouters.EnablePcapAll ("bbr-routers");

  Simulator::Stop (Seconds (100));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
    std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
	  std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
	  std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
	  std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000  << " Mbps\n";
    }

  monitor->SerializeToXmlFile("bbr-flowmon.xml", true, true);

  Simulator::Destroy ();

  return 0;
}