#include <iostream>
#include <fstream>
#include <string>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HybridTcpUdpSimulation");

int main (int argc, char *argv[])
{
  // Enable logging (optional)
  // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
  // LogComponentEnable ("TcpClient", LOG_LEVEL_INFO);
  // LogComponentEnable ("TcpServer", LOG_LEVEL_INFO);

  // Set default values for simulation parameters
  uint32_t nTcpClients = 3;
  uint32_t nUdpNodes = 4;
  double simulationTime = 10.0;
  std::string dataRate = "5Mbps";
  std::string delay = "2ms";

  // Allow command-line arguments to override defaults
  CommandLine cmd;
  cmd.AddValue ("nTcpClients", "Number of TCP clients", nTcpClients);
  cmd.AddValue ("nUdpNodes", "Number of UDP nodes", nUdpNodes);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("dataRate", "Data rate for point-to-point links", dataRate);
  cmd.AddValue ("delay", "Delay for point-to-point links", delay);
  cmd.Parse (argc, argv);

  //
  // Create nodes
  //
  NodeContainer tcpClients, udpNodes, tcpServerNode;
  tcpClients.Create (nTcpClients);
  udpNodes.Create (nUdpNodes);
  tcpServerNode.Create (1);

  NodeContainer allNodes;
  allNodes.Add (tcpClients);
  allNodes.Add (udpNodes);
  allNodes.Add (tcpServerNode);

  //
  // Create point-to-point links
  //
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  pointToPoint.SetChannelAttribute ("Delay", StringValue (delay));

  // TCP Star Topology
  NetDeviceContainer tcpClientDevices[nTcpClients];
  NetDeviceContainer tcpServerDevices;

  for (uint32_t i = 0; i < nTcpClients; ++i)
    {
      NodeContainer link;
      link.Add (tcpClients.Get (i));
      link.Add (tcpServerNode.Get (0));
      NetDeviceContainer devices = pointToPoint.Install (link);
      tcpClientDevices[i] = NetDeviceContainer (devices.Get (0));
      tcpServerDevices.Add (devices.Get (1));
    }

  // UDP Mesh Topology
  NetDeviceContainer udpDevices[nUdpNodes];
  for (uint32_t i = 0; i < nUdpNodes; ++i)
    {
        udpDevices[i] = NetDeviceContainer ();
    }

  for (uint32_t i = 0; i < nUdpNodes; ++i)
  {
    for (uint32_t j = i+1; j < nUdpNodes; ++j)
    {
      NodeContainer link;
      link.Add (udpNodes.Get (i));
      link.Add (udpNodes.Get (j));
      NetDeviceContainer devices = pointToPoint.Install (link);
      udpDevices[i].Add (devices.Get (0));
      udpDevices[j].Add (devices.Get (1));
    }
  }

  //
  // Install Internet stack
  //
  InternetStackHelper stack;
  stack.Install (allNodes);

  //
  // Assign IP addresses
  //
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer tcpClientInterfaces[nTcpClients];
  Ipv4InterfaceContainer tcpServerInterfaces;
  for (uint32_t i = 0; i < nTcpClients; ++i)
  {
      NetDeviceContainer devices;
      devices.Add (tcpClientDevices[i]);
      devices.Add (tcpServerDevices.Get (i));
      Ipv4InterfaceContainer interfaces = address.Assign (devices);
      tcpClientInterfaces[i] = Ipv4InterfaceContainer (interfaces.Get (0));
      tcpServerInterfaces.Add (interfaces.Get (1));
      address.NewNetwork ();
  }


  Ipv4InterfaceContainer udpInterfaces[nUdpNodes];
  for (uint32_t i = 0; i < nUdpNodes; ++i)
  {
      Ipv4InterfaceContainer interfaces = address.Assign (udpDevices[i]);
      udpInterfaces[i] = interfaces;
      address.NewNetwork ();
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


  //
  // Create TCP application
  //
  uint16_t tcpPort = 9;
  BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (tcpServerInterfaces.GetAddress (0), tcpPort));
  source.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer sourceApps;
  for(uint32_t i = 0; i < nTcpClients; ++i) {
    sourceApps.Add (source.Install (tcpClients.Get (i)));
  }
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds (simulationTime - 1.0));

  PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), tcpPort));
  ApplicationContainer sinkApps = sink.Install (tcpServerNode.Get (0));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (simulationTime));

  //
  // Create UDP application
  //
  uint16_t udpPort = 9;
  UdpEchoServerHelper echoServer (udpPort);
  ApplicationContainer serverApps = echoServer.Install (udpNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime - 1.0));

  UdpEchoClientHelper echoClient (udpInterfaces[0].GetAddress (0), udpPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps[nUdpNodes - 1];

  for (uint32_t i = 1; i < nUdpNodes; ++i) {
    echoClient.SetAttribute ("RemoteAddress", AddressValue (InetSocketAddress (udpInterfaces[0].GetAddress (0), udpPort)));
    clientApps[i-1] = echoClient.Install (udpNodes.Get (i));
    clientApps[i-1].Start (Seconds (2.0));
    clientApps[i-1].Stop (Seconds (simulationTime - 2.0));
  }



  //
  // Enable Tracing using Flowmonitor
  //
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  //
  // Enable NetAnim
  //
  AnimationInterface anim ("hybrid-tcp-udp-simulation.xml");
  anim.SetConstantPosition (tcpServerNode.Get (0), 10, 20);
  for(uint32_t i = 0; i < nTcpClients; ++i) {
      anim.SetConstantPosition (tcpClients.Get (i), 10 + (i * 5), 10);
  }
  for(uint32_t i = 0; i < nUdpNodes; ++i) {
      anim.SetConstantPosition (udpNodes.Get (i), 30 + (i * 5), 15);
  }


  //
  // Run the simulation
  //
  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  //
  // Print Flow statistics
  //
  monitor->CheckForLostPackets ();
  Ptr<Iprobe> probe = monitor->GetIprobe ();
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (auto i = stats.begin (); i != stats.end (); ++i)
    {
      std::cout << "Flow ID: " << i->first << "\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
    }

  Simulator::Destroy ();

  return 0;
}