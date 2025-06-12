#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DctcpSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel (LogLevel::LOG_LEVEL_INFO);
  LogComponent::SetDefaultLogComponentEnable (true);

  // Create nodes
  NodeContainer senders, receivers, switches;
  senders.Create (40);
  receivers.Create (40);
  switches.Create (2);

  NodeContainer r1 = NodeContainer (switches.Get (1));

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (senders);
  stack.Install (receivers);
  stack.Install (switches);

  // PointToPointHelper for 10 Gbps links (S-T1, T1-T2)
  PointToPointHelper p2p_10gbps;
  p2p_10gbps.DataRate ("10Gbps");
  p2p_10gbps.Delay ("10us");
  p2p_10gbps.SetQueue ("ns3::RedQueueDisc",
                        "LinkBandwidth", StringValue("10Gbps"),
                        "LinkDelay", StringValue("10us"),
                        "MeanPktSize", UintegerValue(1000));

  // PointToPointHelper for 1 Gbps link (T2-R1)
  PointToPointHelper p2p_1gbps;
  p2p_1gbps.DataRate ("1Gbps");
  p2p_1gbps.Delay ("10us");
    p2p_1gbps.SetQueue ("ns3::RedQueueDisc",
                        "LinkBandwidth", StringValue("1Gbps"),
                        "LinkDelay", StringValue("10us"),
                        "MeanPktSize", UintegerValue(1000));


  // Create links
  NetDeviceContainer sender_t1_devices, t1_t2_devices, t2_receiver_devices;
  for (uint32_t i = 0; i < 30; ++i)
    {
      NetDeviceContainer link = p2p_10gbps.Install (senders.Get (i), switches.Get (0));
      sender_t1_devices.Add (link.Get (0));
    }

  for (uint32_t i = 30; i < 40; ++i)
    {
      NetDeviceContainer link = p2p_1gbps.Install (senders.Get (i), switches.Get (1));
      sender_t1_devices.Add (link.Get (0));
    }

  t1_t2_devices = p2p_10gbps.Install (switches.Get (0), switches.Get (1));

  for (uint32_t i = 0; i < 20; ++i)
  {
     NetDeviceContainer link = p2p_1gbps.Install(switches.Get(1), receivers.Get(i));
     t2_receiver_devices.Add(link.Get(1));
  }

  for (uint32_t i = 20; i < 40; ++i)
  {
     NetDeviceContainer link = p2p_10gbps.Install(switches.Get(0), receivers.Get(i));
     t2_receiver_devices.Add(link.Get(1));
  }


  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer sender_t1_interfaces = address.Assign (sender_t1_devices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer t1_t2_interfaces = address.Assign (t1_t2_devices);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer t2_receiver_interfaces = address.Assign (t2_receiver_devices);


  Ipv4AddressHelper sender_address, receiver_address;
  Ipv4InterfaceContainer sender_interfaces, receiver_interfaces;

  for(uint32_t i=0; i<40; ++i)
  {
    sender_address.SetBase ("10.2." + std::to_string(i) + ".0", "255.255.255.0");
    NetDeviceContainer sender_dev, receiver_dev;
    sender_dev.Add(sender_t1_devices.Get(i));
    receiver_dev.Add(t2_receiver_devices.Get(i));
    sender_interfaces = sender_address.Assign(sender_dev);
    receiver_interfaces = sender_address.Assign(receiver_dev);
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Create Applications (BulkSendApplication and Sink)
  uint16_t port = 50000;
  BulkSendHelper source ("ns3::TcpSocketFactory",
                           InetSocketAddress (t2_receiver_interfaces.GetAddress (0), port));
  source.SetAttribute ("MaxBytes", UintegerValue (0));

  ApplicationContainer sourceApps, sinkApps;

  for (uint32_t i = 0; i < 40; ++i)
  {
    BulkSendHelper temp_source ("ns3::TcpSocketFactory",
                           InetSocketAddress (t2_receiver_interfaces.GetAddress (i), port));
    temp_source.SetAttribute ("MaxBytes", UintegerValue (0));

    sourceApps.Add (temp_source.Install (senders.Get (i)));

    PacketSinkHelper sink ("ns3::TcpSocketFactory",
                             InetSocketAddress (Ipv4Address::GetAny (), port));
    sinkApps.Add (sink.Install (receivers.Get (i)));
  }

  for (uint32_t i = 0; i < 40; ++i)
  {
    sourceApps.Get(i)->SetStartTime (Seconds (1.0 * i / 40));
    sourceApps.Get(i)->SetStopTime (Seconds (5.0));
    sinkApps.Get(i)->SetStartTime (Seconds (0.0));
    sinkApps.Get(i)->SetStopTime (Seconds (5.0));
  }

  // FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Run simulation
  Simulator::Stop (Seconds (5.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  double totalThroughput = 0.0;
  double fairnessNumerator = 0.0;
  double fairnessDenominator = 0.0;
  int flowCount = 0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if (t.sourceAddress == sender_t1_interfaces.GetAddress (0))
        {
          double throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000000;
          std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ") : Throughput: " << throughput << " Mbps" << std::endl;
          totalThroughput += throughput;
          fairnessNumerator += throughput;
          fairnessDenominator += throughput * throughput;
          flowCount++;
        }
    }

  double jainFairness = (fairnessNumerator * fairnessNumerator) / (flowCount * fairnessDenominator);
  std::cout << "Aggregate Throughput: " << totalThroughput << " Mbps" << std::endl;
  std::cout << "Jain's Fairness Index: " << jainFairness << std::endl;

  monitor->SerializeToXmlFile("dctcp.flowmon", true, true);

  Simulator::Destroy ();

  return 0;
}