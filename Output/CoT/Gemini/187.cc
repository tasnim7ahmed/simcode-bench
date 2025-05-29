#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FatTreeSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Define parameters for the Fat-Tree topology
  int k = 4; // k-ary Fat-Tree (k=4 for a 2-tier Fat-Tree)
  int num_pods = k;
  int num_switches_per_pod = k / 2 + 1;
  int num_hosts_per_pod = k / 2;

  // Create nodes
  NodeContainer coreSwitches;
  for (int i = 0; i < (k / 2) * (k / 2); ++i) {
    coreSwitches.Create (1);
  }

  NodeContainer aggregationSwitches[num_pods];
  NodeContainer edgeSwitches[num_pods];
  NodeContainer hosts[num_pods][num_hosts_per_pod];
  for (int i = 0; i < num_pods; ++i) {
    aggregationSwitches[i].Create (k / 2);
    edgeSwitches[i].Create (k / 2);
    for (int j = 0; j < num_hosts_per_pod; ++j) {
      hosts[i][j].Create (1);
    }
  }

  // Install Internet Stack
  InternetStackHelper stack;
  for (int i = 0; i < (k / 2) * (k / 2); ++i) {
      stack.Install(coreSwitches.Get(i));
  }
  for (int i = 0; i < num_pods; ++i) {
    stack.Install (aggregationSwitches[i]);
    stack.Install (edgeSwitches[i]);
    for (int j = 0; j < num_hosts_per_pod; ++j) {
      stack.Install (hosts[i][j]);
    }
  }

  // Create point-to-point links
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("100us"));

  // Connect core switches to aggregation switches
  int core_index = 0;
  for (int i = 0; i < num_pods; ++i) {
    for (int j = 0; j < k / 2; ++j) {
      for (int m = 0; m < k/2; ++m){
        NetDeviceContainer link = pointToPoint.Install (aggregationSwitches[i].Get(j), coreSwitches.Get(core_index));
        core_index++;
      }
      core_index -= k/2;
    }
    core_index += k/2;
  }
  core_index = 0;

  // Connect aggregation switches to edge switches
  for (int i = 0; i < num_pods; ++i) {
    for (int j = 0; j < k / 2; ++j) {
      NetDeviceContainer link = pointToPoint.Install (aggregationSwitches[i].Get (j), edgeSwitches[i].Get (j));
    }
  }

  // Connect edge switches to hosts
  for (int i = 0; i < num_pods; ++i) {
    for (int j = 0; j < k / 2; ++j) {
      NetDeviceContainer link = pointToPoint.Install (edgeSwitches[i].Get (j), hosts[i][j].Get (0));
    }
  }

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.0.0", "255.255.0.0");

  Ipv4InterfaceContainer interfaces[num_pods][num_hosts_per_pod];
  for (int i = 0; i < num_pods; ++i) {
      for(int j = 0; j < num_hosts_per_pod; ++j){
          interfaces[i][j] = address.Assign (hosts[i][j].GetDevices ());
          address.NewNetwork ();
      }
  }

  Ipv4AddressHelper switchAddress;
  switchAddress.SetBase("10.100.0.0", "255.255.0.0");

  NetDeviceContainer switchDevices;
  for(int i=0; i<(k/2)*(k/2); ++i){
      switchDevices = coreSwitches.Get(i)->GetDevice(0);
      switchAddress.Assign(switchDevices);
      switchAddress.NewNetwork();
  }

  for (int i = 0; i < num_pods; i++) {
      for(int j=0; j< k/2; ++j){
          switchDevices = aggregationSwitches[i].Get(j)->GetDevice(0);
          switchAddress.Assign(switchDevices);
          switchAddress.NewNetwork();

          switchDevices = edgeSwitches[i].Get(j)->GetDevice(0);
          switchAddress.Assign(switchDevices);
          switchAddress.NewNetwork();
      }
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Create Applications
  uint16_t port = 9; // Discard port (RFC 863)

  // Source and Destination nodes
  int src_pod = 0;
  int src_host = 0;
  int dst_pod = 1;
  int dst_host = 1;

  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (hosts[dst_pod][dst_host].Get(0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces[dst_pod][dst_host].GetAddress (0), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (hosts[src_pod][src_host].Get(0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable Tracing using FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Enable Packet Capture
  //pointToPoint.EnablePcapAll ("fat-tree");

  // Enable NetAnim visualization
  AnimationInterface anim ("fat-tree.xml");
  anim.EnablePacketMetadata(true);

  // Set node positions for better visualization
  for (int i = 0; i < (k / 2) * (k / 2); ++i) {
    anim.SetConstantPosition (coreSwitches.Get (i), 5 + i*2, 20);
  }
  for (int i = 0; i < num_pods; ++i) {
    for (int j = 0; j < k / 2; ++j) {
      anim.SetConstantPosition (aggregationSwitches[i].Get (j), 5 + i*2, 15 + j*2);
      anim.SetConstantPosition (edgeSwitches[i].Get (j), 5 + i*2, 10 + j*2);
      anim.SetConstantPosition (hosts[i][j].Get (0), 5 + i*2, 5 + j*2);
    }
  }

  // Run the simulation
  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();

  // Print Flow Monitor Statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Bytes: " << i->second.lostBytes << "\n";
      std::cout << "  Delay Sum:  " << i->second.delaySum << "\n";
      std::cout << "  Jitter Sum: " << i->second.jitterSum << "\n";
    }

  Simulator::Destroy ();
  return 0;
}