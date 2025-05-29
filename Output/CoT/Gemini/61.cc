#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/queue.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SeriesNetwork");

int main (int argc, char *argv[])
{
  bool enablePcap = true;

  CommandLine cmd;
  cmd.AddValue ("EnablePcap", "Enable PCAP Tracing", enablePcap);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetAttribute ("TcpL4Protocol", "SocketType", StringValue ("ns3::TcpNewReno"));

  NodeContainer nodes;
  nodes.Create (4);

  PointToPointHelper p2p01;
  p2p01.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p01.SetChannelAttribute ("Delay", StringValue ("1ms"));
  p2p01.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(20));

  PointToPointHelper p2p12;
  p2p12.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  p2p12.SetChannelAttribute ("Delay", StringValue ("10ms"));
  p2p12.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(20));

  PointToPointHelper p2p23;
  p2p23.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p23.SetChannelAttribute ("Delay", StringValue ("1ms"));
  p2p23.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(20));

  NetDeviceContainer devices01 = p2p01.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices12 = p2p12.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer devices23 = p2p23.Install (nodes.Get (2), nodes.Get (3));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = address.Assign (devices01);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign (devices12);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces23 = address.Assign (devices23);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;

  BulkSendHelper source ("ns3::TcpSocketFactory",
                         InetSocketAddress (interfaces23.GetAddress (1,0), port));
  source.SetAttribute ("SendSize", UintegerValue (1400));
  source.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer sourceApps = source.Install (nodes.Get (0));
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds (10.0));

  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (3));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

  // Tracing
  if (enablePcap)
  {
    p2p01.EnablePcapAll ("results/pcap/series-network-01");
    p2p12.EnablePcapAll ("results/pcap/series-network-12");
    p2p23.EnablePcapAll ("results/pcap/series-network-23");
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (10.0));

  //Cwnd Tracing
  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("results/cwndTraces/tcp-cwnd.dat");
  Config::ConnectWithoutContext ("/NodeList/*/ApplicationList/*/$ns3::BulkSendApplication/Socket[0]/TcpRxBuffer/BytesReceived", MakeBoundCallback (&WriteTcpCwnd, stream));

  //Queue Length Tracing
  std::ofstream queueSizeFile("results/queue-size.dat");
  if (queueSizeFile.is_open()) {
      Simulator::Schedule(Seconds(0.1), [&]() {
          Ptr<Queue<Packet>> queue01 = StaticCast<PointToPointNetDevice>(devices01.Get(0))->GetQueue();
          Ptr<Queue<Packet>> queue12 = StaticCast<PointToPointNetDevice>(devices12.Get(0))->GetQueue();
          Ptr<Queue<Packet>> queue23 = StaticCast<PointToPointNetDevice>(devices23.Get(0))->GetQueue();

          if (queue01 && queue12 && queue23) {
              queueSizeFile << Simulator::Now().GetSeconds() << " "
                           << queue01->GetNPackets() << " "
                           << queue12->GetNPackets() << " "
                           << queue23->GetNPackets() << std::endl;
          }
          Simulator::Schedule(Seconds(0.1), [&]() { Simulator::Stop(); });
      });
  }
  else {
      std::cerr << "Error opening queue-size.dat for writing." << std::endl;
  }

  // Queue Drop Tracing
  AsciiTraceHelper asciiTraceHelperQueueDrop;
  Ptr<OutputStreamWrapper> streamQueueDrop = asciiTraceHelperQueueDrop.CreateFileStream ("results/queueTraces/queue-drop.dat");

  Config::ConnectWithoutContext ("/NodeList/*/DeviceList/*/TxQueue/Drop", MakeBoundCallback (&WriteQueueDrop, streamQueueDrop));

  //Queue Stats
  TrafficControlHelper tch;
  tch.EnableQueueStats ("results/queueStats.txt", Seconds (0.5));

  // Config details
  std::ofstream configFile("results/config.txt");
    if (configFile.is_open()) {
        configFile << "Simulation Configuration:" << std::endl;
        configFile << "Nodes: n0, n1, n2, n3" << std::endl;
        configFile << "Link n0-n1: 10Mbps, 1ms delay" << std::endl;
        configFile << "Link n1-n2: 1Mbps, 10ms delay" << std::endl;
        configFile << "Link n2-n3: 10Mbps, 1ms delay" << std::endl;
        configFile << "TCP Flow: n0 -> n3" << std::endl;
        configFile << "Simulation Time: 10 seconds" << std::endl;
        configFile.close();
    } else {
        std::cerr << "Error opening config.txt for writing." << std::endl;
    }

  Simulator::Run ();

  monitor->SerializeToXmlFile("results/flowmon.xml", false, true);

  Simulator::Destroy ();
  queueSizeFile.close();
  return 0;
}

static void
WriteTcpCwnd (Ptr<OutputStreamWrapper> stream, uint32_t bytesReceived)
{
  *stream << Simulator::Now ().GetSeconds () << " " << bytesReceived << std::endl;
}

static void
WriteQueueDrop (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> p)
{
  *stream << Simulator::Now ().GetSeconds () << " Drop" << std::endl;
}