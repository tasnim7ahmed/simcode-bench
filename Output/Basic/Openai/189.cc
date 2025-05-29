#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CsmaTcpExample");

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("CsmaTcpExample", LOG_LEVEL_INFO);

  // Create 4 nodes: 0-2 are clients, 3 is the server
  NodeContainer nodes;
  nodes.Create (4);

  // Set up CSMA channel with some delay and error rate
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  csma.SetDeviceAttribute ("Mtu", UintegerValue (1500));

  // Install devices
  NetDeviceContainer devices = csma.Install (nodes);

  // Add packet loss model to the CSMA channel
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  em->SetAttribute ("ErrorRate", DoubleValue (0.01)); // 1% packet loss
  for (uint32_t i = 0; i < devices.GetN (); ++i)
    {
      devices.Get (i)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
    }

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Create a single TCP server on node 3
  uint16_t port = 8080;
  Address serverAddress (InetSocketAddress (interfaces.GetAddress (3), port));
  PacketSinkHelper pktSinkHelper ("ns3::TcpSocketFactory", serverAddress);
  ApplicationContainer sinkApp = pktSinkHelper.Install (nodes.Get (3));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (15.0));

  // Install TCP clients on nodes 0, 1, 2 targeting node 3
  OnOffHelper client ("ns3::TcpSocketFactory", serverAddress);
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  client.SetAttribute ("DataRate", StringValue ("10Mbps"));
  client.SetAttribute ("MaxBytes", UintegerValue (0)); // unlimited
  client.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  client.SetAttribute ("StopTime", TimeValue (Seconds (14.0)));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < 3; ++i)
    {
      clientApps.Add (client.Install (nodes.Get (i)));
    }

  // Enable pcap tracing (optional)
  csma.EnablePcapAll ("csma-tcp");

  // Set up FlowMonitor
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll ();

  // Set up NetAnim
  AnimationInterface anim ("csma-tcp.xml");
  anim.SetConstantPosition (nodes.Get (0), 10, 30);
  anim.SetConstantPosition (nodes.Get (1), 40, 30);
  anim.SetConstantPosition (nodes.Get (2), 70, 30);
  anim.SetConstantPosition (nodes.Get (3), 40, 60);

  Simulator::Stop (Seconds (15.0));
  Simulator::Run ();

  // Print FlowMonitor statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  std::cout << "Flow statistics:" << std::endl;
  for (auto const& i : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i.first);
      std::cout << "Flow ID: " << i.first << ", Src Addr: " << t.sourceAddress
                << ", Dst Addr: " << t.destinationAddress << std::endl;
      std::cout << "  Tx Packets: " << i.second.txPackets << std::endl;
      std::cout << "  Rx Packets: " << i.second.rxPackets << std::endl;
      std::cout << "  Lost Packets: " << i.second.lostPackets << std::endl;
      std::cout << "  Throughput: " << (i.second.rxBytes * 8.0 / (i.second.timeLastRxPacket.GetSeconds () - i.second.timeFirstTxPacket.GetSeconds ()) / 1e6)
                << " Mbps" << std::endl;
      std::cout << "  Mean Delay: " << (i.second.delaySum.GetSeconds () / i.second.rxPackets)
                << " s" << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}