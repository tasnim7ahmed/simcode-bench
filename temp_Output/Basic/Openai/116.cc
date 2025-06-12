#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/animation-interface.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  // LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Set up WiFi (802.11b/g/n) in ad hoc mode
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211g);

  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  // Mobility model: RandomWalk2dMobilityModel within a box
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", TimeValue (Seconds (0.5)),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=2.0]"),
                             "Bounds", RectangleValue (Rectangle (0, 100, 0, 100)));
  mobility.Install (nodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IPv4 addresses
  Ipv4AddressHelper address;
  address.SetBase ("192.9.39.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // UDP Echo Servers
  uint16_t port1 = 9;
  uint16_t port2 = 10;
  UdpEchoServerHelper echoServer1 (port1);
  UdpEchoServerHelper echoServer2 (port2);

  ApplicationContainer serverApps1 = echoServer1.Install (nodes.Get (0));
  ApplicationContainer serverApps2 = echoServer2.Install (nodes.Get (1));
  serverApps1.Start (Seconds (1.0));
  serverApps1.Stop (Seconds (20.0));
  serverApps2.Start (Seconds (1.0));
  serverApps2.Stop (Seconds (20.0));

  // UDP Echo Clients (bi-directional)
  UdpEchoClientHelper echoClient1 (interfaces.GetAddress (1), port2);
  echoClient1.SetAttribute ("MaxPackets", UintegerValue (16));
  echoClient1.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient1.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientApps1 = echoClient1.Install (nodes.Get (0));
  clientApps1.Start (Seconds (2.0));
  clientApps1.Stop (Seconds (18.0));

  UdpEchoClientHelper echoClient2 (interfaces.GetAddress (0), port1);
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (16));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientApps2 = echoClient2.Install (nodes.Get (1));
  clientApps2.Start (Seconds (2.5));
  clientApps2.Stop (Seconds (18.5));

  // Enable Flow Monitor
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll ();

  // Enable animation
  AnimationInterface anim ("adhoc-bidir.xml");
  anim.SetConstantPosition (nodes.Get (0), 20, 50);
  anim.SetConstantPosition (nodes.Get (1), 80, 50);

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();

  // Flow monitor analysis
  flowmon->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats ();

  double totalTxPackets = 0;
  double totalRxPackets = 0;
  double totalDelaySum = 0;
  double totalRxBytes = 0;
  double simulationTime = 20.0 - 2.0; // On time duration
  uint32_t totalFlows = 0;

  std::cout << "Flow statistics:\n";
  for (auto iter = stats.begin (); iter != stats.end (); ++iter)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
      std::cout << "Flow " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << iter->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << iter->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << iter->second.lostPackets << "\n";
      std::cout << "  Throughput: "
                << iter->second.rxBytes * 8.0 / (simulationTime * 1000.0)
                << " Kbps\n";
      std::cout << "  Mean Delay: ";
      if (iter->second.rxPackets > 0)
        std::cout << (iter->second.delaySum.GetSeconds () / iter->second.rxPackets) << " s\n";
      else
        std::cout << "N/A\n";

      totalTxPackets += iter->second.txPackets;
      totalRxPackets += iter->second.rxPackets;
      totalRxBytes += iter->second.rxBytes;
      totalDelaySum += iter->second.delaySum.GetSeconds ();
      totalFlows++;
    }

  double pdr = (totalTxPackets > 0) ? (totalRxPackets / totalTxPackets) * 100.0 : 0.0;
  double avgDelay = (totalRxPackets > 0) ? (totalDelaySum / totalRxPackets) : 0.0;
  double throughput = (totalRxBytes * 8.0) / (simulationTime * 1000.0); // Kbps

  std::cout << "\nAggregate performance metrics:\n";
  std::cout << "  Packet Delivery Ratio: " << pdr << " %\n";
  std::cout << "  Average End-to-End Delay: " << avgDelay << " s\n";
  std::cout << "  Aggregate Throughput: " << throughput << " Kbps\n";

  Simulator::Destroy ();
  return 0;
}