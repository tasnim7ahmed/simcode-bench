#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/csma-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/propagation-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/applications-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WSN_Circular_802154");

uint32_t packetsReceived = 0;
uint32_t packetsSent = 0;
double   sumDelay = 0.0;

void RxCallback (Ptr<const Packet> packet, const Address &srcAddress)
{
  packetsReceived++;
}

void DelayTracer (Ptr<const Packet> packet, const Address &from)
{
  // Not used, measured via FlowMonitor for accuracy
}

void PacketSentCallback (Ptr<const Packet> packet)
{
  packetsSent++;
}

int main (int argc, char *argv[])
{
  uint32_t numNodes = 6;
  double radius = 20.0;
  double interval = 2.0;
  uint32_t packetSize = 50;
  double simulationTime = 30.0;
  uint16_t sinkPort = 4000;

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (numNodes);

  // Position nodes in a circle
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      double angle = 2 * M_PI * i / numNodes;
      double x = radius * std::cos (angle);
      double y = radius * std::sin (angle);
      positionAlloc->Add (Vector (x, y, 0.0));
    }
  mobility.SetPositionAllocator (positionAlloc);
  mobility.Install (nodes);

  // Install IEEE 802.15.4
  LrWpanHelper lrWpanHelper;
  NetDeviceContainer devs = lrWpanHelper.Install (nodes);
  lrWpanHelper.AssociateToPan (devs, 0);

  // Energy not modelled. Enable 2.4 GHz PHY
  Ptr<LrWpanNetDevice> dev0 = devs.Get (0)->GetObject<LrWpanNetDevice> ();
  dev0->GetPhy ()->SetChannelNumber (20); // 2.4 GHz

  for (uint32_t i = 1; i < devs.GetN (); ++i)
    {
      Ptr<LrWpanNetDevice> dev = devs.Get (i)->GetObject<LrWpanNetDevice> ();
      dev->GetPhy ()->SetChannelNumber (20);
    }

  // Channel configuration (default, no extra propagation settings)
  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IPv6 addresses over 802.15.4
  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer ifaces = ipv6.Assign (devs);
  for (uint32_t i = 0; i < ifaces.GetN (); ++i)
    {
      ifaces.SetForwarding (i, true);
      ifaces.SetDefaultRouteInAllNodes (i);
    }

  // UDP server (sink) on node 0
  UdpServerHelper udpServer (sinkPort);
  ApplicationContainer serverApp = udpServer.Install (nodes.Get (0));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (simulationTime));

  // UDP clients on all other nodes
  std::vector<ApplicationContainer> clientApps;
  for (uint32_t i = 1; i < numNodes; ++i)
    {
      UdpClientHelper udpClient (ifaces.GetAddress (0, 1), sinkPort);
      udpClient.SetAttribute ("MaxPackets", UintegerValue (uint32_t (simulationTime / interval)));
      udpClient.SetAttribute ("Interval", TimeValue (Seconds (interval)));
      udpClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

      ApplicationContainer clientApp = udpClient.Install (nodes.Get (i));
      clientApp.Start (Seconds (1.0));
      clientApp.Stop (Seconds (simulationTime));
      clientApps.push_back (clientApp);
    }

  // Set up tracing
  // Use FlowMonitor for performance metrics
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // NetAnim Visualization
  AnimationInterface anim ("wsn-circular.xml");
  // Set labels
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      std::ostringstream oss;
      if (i == 0)
        oss << "Sink";
      else
        oss << "Sensor-" << i;
      anim.UpdateNodeDescription (i, oss.str ());
      anim.UpdateNodeColor (i, i == 0 ? 0 : 0, i == 0 ? 255 : 127, 0);
    }
  anim.EnablePacketMetadata ();

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  // Compute performance metrics using FlowMonitor
  monitor->CheckForLostPackets ();
  double rxPackets = 0;
  double txPackets = 0;
  double throughput = 0;
  double totalDelay = 0;
  double nFlows = 0;

  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (auto it = stats.begin (); it != stats.end (); ++it)
    {
      Ipv6FlowClassifier::FiveTuple t = 
        flowmon.GetClassifier ()->FindFlow (it->first);
      if (t.destinationAddress == ifaces.GetAddress (0, 1))
        {
          txPackets += it->second.txPackets;
          rxPackets += it->second.rxPackets;
          totalDelay += it->second.delaySum.GetSeconds ();
          throughput += (it->second.rxBytes * 8.0) / simulationTime / 1000;
          nFlows += 1;
        }
    }

  double pdr = rxPackets / txPackets;
  double avgDelay = (rxPackets > 0) ? (totalDelay / rxPackets) : 0;

  std::cout << "---------- Performance Metrics ----------" << std::endl;
  std::cout << "Packets Sent:     " << txPackets << std::endl;
  std::cout << "Packets Received: " << rxPackets << std::endl;
  std::cout << "Packet Delivery Ratio: " << pdr << std::endl;
  std::cout << "Average End-to-End Delay: " << avgDelay << " s" << std::endl;
  std::cout << "Throughput: " << throughput << " kbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}