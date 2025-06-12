#include <iostream>
#include <fstream>
#include <string>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/stats-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SensorNetworkSimulation");

int main (int argc, char *argv[])
{
  // Enable logging
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  // Set simulation parameters
  uint32_t numNodes = 5; // Number of sensor nodes
  double simulationTime = 10.0; // Simulation time in seconds
  double sinkX = 50.0;
  double sinkY = 50.0;
  double nodeRange = 20.0;
  std::string phyMode ("OfdmRate6Mbps");

  // Create nodes
  NodeContainer sinkNode;
  sinkNode.Create (1);
  NodeContainer sensorNodes;
  sensorNodes.Create (numNodes);

  // Install PHY and MAC layers for LR-WPAN (IEEE 802.15.4)
  LrWpanHelper lrWpanHelper;
  NetDeviceContainer sinkDevices = lrWpanHelper.Install (sinkNode);
  NetDeviceContainer sensorDevices = lrWpanHelper.Install (sensorNodes);

  // Create channels and assign devices
  ChannelAttributes channelAtt;
  channelAtt.delay = Seconds (0.001);
  channelAtt.dataRate = DataRate ("250kbps");
  Ptr<LrWpanChannel> channel = CreateObject<LrWpanChannel> (channelAtt);

  for (uint32_t i = 0; i < sinkDevices.GetN (); ++i)
    {
      Ptr<NetDevice> sinkNetDevice = sinkDevices.Get (i);
      channel->AddDevice (sinkNetDevice);
    }
  for (uint32_t i = 0; i < sensorDevices.GetN (); ++i)
    {
      Ptr<NetDevice> sensorNetDevice = sensorDevices.Get (i);
      channel->AddDevice (sensorNetDevice);
    }

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (sinkNode);
  internet.Install (sensorNodes);

  // Assign IPv4 addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer sinkInterfaces = ipv4.Assign (sinkDevices);
  Ipv4InterfaceContainer sensorInterfaces = ipv4.Assign (sensorDevices);

  // Install UDP client and server applications
  uint16_t port = 9; // Discard port (RFC 863)
  UdpServerHelper server (port);
  ApplicationContainer sinkApp = server.Install (sinkNode.Get (0));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (simulationTime));

  UdpClientHelper client (sinkInterfaces.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      clientApps.Add (client.Install (sensorNodes.Get (i)));
    }
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime));

  // Mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (nodeRange),
                                 "DeltaY", DoubleValue (nodeRange),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (sensorNodes);

  Ptr<Node> sinkNodePtr = sinkNode.Get (0);
  Ptr<MobilityModel> sinkMobility = sinkNodePtr->GetObject<MobilityModel> ();
  Vector3D position (sinkX, sinkY, 0.0);
  sinkMobility->SetPosition (position);
  mobility.Install (sinkNode);

  // Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Animation interface
  AnimationInterface anim ("sensor-network.xml");
  anim.SetConstantPosition (sinkNode.Get (0), sinkX, sinkY);
  for (uint32_t i = 0; i < numNodes; ++i)
  {
      Ptr<Node> node = sensorNodes.Get(i);
      Ptr<MobilityModel> mob = node->GetObject<MobilityModel> ();
      Vector position = mob->GetPosition ();
      anim.SetConstantPosition (node, position.x, position.y);
  }

  // Run simulation
  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  // Print flow statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024  << " kbps\n";
      std::cout << "  Delay: " << i->second.delaySum.GetSeconds () / i->second.rxPackets << " s\n";

    }

  Simulator::Destroy ();
  return 0;
}