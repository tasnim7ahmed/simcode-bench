#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/mesh-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SensorNetworkStarExample");

const uint32_t NUM_SENSORS = 5;

void
PacketReceiveCallback (Ptr<const Packet> packet, const Address &)
{
  NS_LOG_INFO ("Sink received packet of size " << packet->GetSize ());
}

int main (int argc, char *argv[])
{
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  // 1 central sink + NUM_SENSORS
  NodeContainer nodes;
  nodes.Create (NUM_SENSORS + 1);

  // Setup mobility: Place sink at origin, sensors on a circle
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0, 0, 0)); // Sink at center

  double radius = 20.0; // meters
  for (uint32_t i = 0; i < NUM_SENSORS; ++i)
    {
      double angle = 2 * M_PI * i / NUM_SENSORS;
      double x = radius * std::cos (angle);
      double y = radius * std::sin (angle);
      positionAlloc->Add (Vector (x, y, 0));
    }
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.SetPositionAllocator (positionAlloc);
  mobility.Install (nodes);

  // Install LrWpanNetDevice (IEEE 802.15.4)
  LrWpanHelper lrWpanHelper;
  NetDeviceContainer lrwpanDevs = lrWpanHelper.Install (nodes);

  // Assign 16-bit short addresses to PAN
  lrWpanHelper.AssociateToPan (lrwpanDevs, 0);

  // Enable pcap for PHY
  lrWpanHelper.EnablePcapAll (std::string ("star-lrwpan"), false);

  // 6LoWPAN to provide IPv6 over LR-WPAN
  SixLowPanHelper sixLowPanHelper;
  NetDeviceContainer sixLowPanDevs = sixLowPanHelper.Install (lrwpanDevs);

  // Install IPv6 stack
  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign (sixLowPanDevs);
  interfaces.SetForwarding (0, true);
  interfaces.SetDefaultRouteInAllNodes (0);

  // Create UDP server on the sink (node 0)
  uint16_t udpPort = 50000;
  UdpServerHelper udpServer (udpPort);
  ApplicationContainer serverApps = udpServer.Install (nodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (30.0));

  // Each sensor sends UDP to sink
  uint32_t packetSize = 40;
  double interval = 2.0; // seconds
  for (uint32_t i = 1; i <= NUM_SENSORS; ++i)
    {
      UdpClientHelper udpClient (interfaces.GetAddress (0, 1), udpPort);
      udpClient.SetAttribute ("MaxPackets", UintegerValue (15));
      udpClient.SetAttribute ("Interval", TimeValue (Seconds (interval)));
      udpClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
      ApplicationContainer clientApp = udpClient.Install (nodes.Get (i));
      clientApp.Start (Seconds (2.0 + i)); // Slightly staggered
      clientApp.Stop (Seconds (30.0));
    }

  // Enable NetAnim output
  AnimationInterface anim ("sensor-star-network.xml");
  anim.SetConstantPosition (nodes.Get (0), 0, 0);
  for (uint32_t i = 1; i <= NUM_SENSORS; ++i)
    {
      double angle = 2 * M_PI * (i - 1) / NUM_SENSORS;
      double x = radius * std::cos (angle);
      double y = radius * std::sin (angle);
      anim.SetConstantPosition (nodes.Get (i), x, y);
    }

  // FlowMonitor for statistics
  Ptr<FlowMonitor> flowmon;
  FlowMonitorHelper flowmonHelper;
  flowmon = flowmonHelper.InstallAll ();

  Simulator::Stop (Seconds (32.0));
  Simulator::Run ();

  flowmon->CheckForLostPackets ();
  Ptr<Ipv6FlowClassifier> classifier = DynamicCast<Ipv6FlowClassifier> (flowmonHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats ();

  std::cout << "\n--- Flow Monitor Results ---\n";
  for (auto &flow : stats)
    {
      Ipv6FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
      std::cout << "Flow " << flow.first << " ("
                << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
      std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
      std::cout << "  Packet Delivery Ratio: "
                << (flow.second.txPackets > 0 ?
                    100.0 * flow.second.rxPackets / flow.second.txPackets : 0) << "%\n";
      std::cout << "  Throughput: " <<
        (flow.second.rxBytes * 8.0 / (flow.second.timeLastRxPacket.GetSeconds() -
                                       flow.second.timeFirstTxPacket.GetSeconds()) / 1000)
        << " kbps" << "\n";
      if (flow.second.rxPackets > 0)
        {
          std::cout << "  Mean Delay: "
                    << (flow.second.delaySum.GetSeconds () / flow.second.rxPackets) << " s\n";
        }
      else
        {
          std::cout << "  Mean Delay: N/A (no received packets)\n";
        }
    }
  std::cout << "----------------------------\n";

  Simulator::Destroy ();
  return 0;
}