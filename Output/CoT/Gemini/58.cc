#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/bulk-send-application.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpBbrSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel (LogLevel::LOG_LEVEL_INFO);
  LogComponent::SetLogLevel (LogLevel::LOG_PREFIX_TIME | LogLevel::LOG_PREFIX_NODE);

  NodeContainer nodes;
  nodes.Create (4);

  NodeContainer senderR1 = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer R1R2 = NodeContainer (nodes.Get (1), nodes.Get (2));
  NodeContainer R2receiver = NodeContainer (nodes.Get (2), nodes.Get (3));

  InternetStackHelper stack;
  stack.Install (nodes);

  PointToPointHelper p2pSenderR1;
  p2pSenderR1.SetDeviceAttribute ("DataRate", StringValue ("1000Mbps"));
  p2pSenderR1.SetChannelAttribute ("Delay", StringValue ("5ms"));

  PointToPointHelper p2pR1R2;
  p2pR1R2.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2pR1R2.SetChannelAttribute ("Delay", StringValue ("10ms"));

  PointToPointHelper p2pR2receiver;
  p2pR2receiver.SetDeviceAttribute ("DataRate", StringValue ("1000Mbps"));
  p2pR2receiver.SetChannelAttribute ("Delay", StringValue ("5ms"));

  NetDeviceContainer devicesSenderR1 = p2pSenderR1.Install (senderR1);
  NetDeviceContainer devicesR1R2 = p2pR1R2.Install (R1R2);
  NetDeviceContainer devicesR2receiver = p2pR2receiver.Install (R2receiver);

  TrafficControlHelper tchBottleneck;
  tchBottleneck.SetRootQueueDisc ("ns3::FifoQueueDisc", "MaxSize", StringValue ("1000p"));
  QueueDiscContainer queueDiscs = tchBottleneck.Install (devicesR1R2.Get (1));

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesSenderR1 = address.Assign (devicesSenderR1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesR1R2 = address.Assign (devicesR1R2);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesR2receiver = address.Assign (devicesR2receiver);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Configure TCP BBR
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpBbr"));

  uint16_t port = 50000;
  BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (interfacesR2receiver.GetAddress (1), port));
  source.SetAttribute ("SendSize", UintegerValue (1448));
  source.SetAttribute ("Protocol", EnumValue (TcpHeader::IPV4));

  ApplicationContainer sourceApps = source.Install (nodes.Get (0));
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds (99.0));

  PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (3));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (100.0));

  // Traces

  AsciiTraceHelper ascii;
  p2pR1R2.EnableAsciiAll (ascii.CreateFileStream ("p2p-trace.tr"));
  p2pR1R2.EnablePcapAll ("p2p-trace", false);
  p2pSenderR1.EnablePcapAll ("sender-r1", false);
  p2pR2receiver.EnablePcapAll ("r2-receiver", false);

  // Congestion window trace
  Config::ConnectWithoutContext ("/NodeList/0/ApplicationList/0/$ns3::BulkSendApplication/TxTrace", MakeCallback (&
                                                                                                          [](Time time, SequenceNumber32 seq,
                                                                                                             uint32_t bytesInFlight) {
                                                                                                            std::ofstream cWndStream("congestion_window.dat", std::ios::app);
                                                                                                            cWndStream << time.GetSeconds() << " " << bytesInFlight << std::endl;
                                                                                                            cWndStream.close();
                                                                                                          }));

  // Throughput trace
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Schedule (Seconds (100.0), [&]() {
    monitor->SerializeToXmlFile("flowmon.xml", false, true);
  });

  // Queue size trace
  Simulator::Schedule (Seconds (0.1), [&]() {
      Simulator::ScheduleNow (&
                              [](Ptr<QueueDisc> queue) {
                                std::ofstream queueSizeStream("queue_size.dat", std::ios::app);
                                queueSizeStream << Simulator::Now().GetSeconds() << " " << queue->GetCurrentSize().Get() << std::endl;
                                queueSizeStream.close();
                              }, queueDiscs.Get(0));
    Simulator::Schedule (Seconds (0.1), event);
  });

    Simulator::Schedule (Seconds(10), [](){
        Config::Set ("/NodeList/0/$ns3::TcpL4Protocol/SocketType", StringValue ("ns3::TcpBbr"));
        Config::Set ("/NodeList/0/$ns3::TcpBbr/PacingGain", DoubleValue (0.9));
        Config::Set ("/NodeList/0/$ns3::TcpBbr/CwndGain", DoubleValue (2.0));
        Config::Set ("/NodeList/0/$ns3::TcpBbr/ProbingInterval", TimeValue(Seconds(10.0)));
    });

  Simulator::Run ();

  Simulator::Destroy ();

  return 0;
}