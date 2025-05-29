#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SensorStarIpv4Simulation");

class PacketStats : public Object
{
public:
  PacketStats () : m_rx (0), m_tx (0), m_totalLatency (Seconds (0.0)) {}

  void TxCallback (Ptr<const Packet> packet)
  {
    m_tx++;
    m_sentTime[packet->GetUid ()] = Simulator::Now ();
  }

  void RxCallback (Ptr<const Packet> packet, const Address &)
  {
    m_rx++;
    auto it = m_sentTime.find (packet->GetUid ());
    if (it != m_sentTime.end ())
      {
        Time delay = Simulator::Now () - it->second;
        m_totalLatency += delay;
        m_sentTime.erase (it);
      }
  }

  uint32_t GetRx () const { return m_rx; }
  uint32_t GetTx () const { return m_tx; }
  Time GetTotalLatency () const { return m_totalLatency; }

private:
  uint32_t m_rx;
  uint32_t m_tx;
  Time m_totalLatency;
  std::map<uint64_t, Time> m_sentTime;
};

int main (int argc, char *argv[])
{
  // Simulation parameters
  uint32_t nSensors = 5;
  double simTime = 30.0;
  double pktInterval = 2.0;
  uint32_t pktSize = 50;

  // Create nodes: 1 sink + nSensors
  NodeContainer sinkNode;
  sinkNode.Create (1);
  NodeContainer sensorNodes;
  sensorNodes.Create (nSensors);

  NodeContainer allNodes;
  allNodes.Add (sinkNode);
  allNodes.Add (sensorNodes);

  // Install mobility: center sink, sensors in circle 20m away
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (50.0, 50.0, 0.0)); // sink at center
  double radius = 20.0;
  for (uint32_t i = 0; i < nSensors; ++i)
    {
      double angle = 2 * M_PI * i / nSensors;
      double x = 50.0 + radius * std::cos (angle);
      double y = 50.0 + radius * std::sin (angle);
      positionAlloc->Add (Vector (x, y, 0.0));
    }
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (allNodes);

  // Install IEEE 802.15.4 LR-WPAN devices
  LrWpanHelper lrWpanHelper;
  NetDeviceContainer devices = lrWpanHelper.Install (allNodes);
  lrWpanHelper.AssociateToPan (devices, 0);

  // Install 6LoWPAN over 802.15.4 devices
  SixLowPanHelper sixlowpan;
  NetDeviceContainer sixlowpanDevices = sixlowpan.Install (devices);

  // Install Internet stack (IPv6, but for UDP, we can use IPv4)
  InternetStackHelper internetv4;
  internetv4.Install (allNodes);

  // Assign IPv4 addresses (hacky but functional for this simple setup)
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (sixlowpanDevices);

  // Set routing: static routing for star topology
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  for (uint32_t i = 1; i < allNodes.GetN (); ++i)
    {
      Ptr<Ipv4StaticRouting> routing = ipv4RoutingHelper.GetStaticRouting (allNodes.Get (i)->GetObject<Ipv4> ());
      routing->AddHostRouteTo (interfaces.GetAddress (0), 1, 1);
    }

  // Install UDP server on sink
  uint16_t udpPort = 4000;
  UdpServerHelper server (udpPort);
  ApplicationContainer serverApps = server.Install (sinkNode.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (simTime + 1));

  // Packet statistics per node
  std::vector<Ptr<PacketStats>> stats (nSensors);

  // Install UDP client on each sensor
  for (uint32_t i = 0; i < nSensors; ++i)
    {
      UdpClientHelper client (interfaces.GetAddress (0), udpPort);
      client.SetAttribute ("MaxPackets", UintegerValue (10000));
      client.SetAttribute ("Interval", TimeValue (Seconds (pktInterval)));
      client.SetAttribute ("PacketSize", UintegerValue (pktSize));
      ApplicationContainer clientApps = client.Install (sensorNodes.Get (i));
      clientApps.Start (Seconds (1.0 + 0.2 * i)); // slight offset
      clientApps.Stop (Seconds (simTime));

      stats[i] = CreateObject<PacketStats> ();
      Ptr<Node> node = sensorNodes.Get (i);

      clientApps.Get (0)->TraceConnectWithoutContext (
          "Tx", MakeCallback (&PacketStats::TxCallback, stats[i]));
      sinkNode.Get (0)->GetApplication (0)->TraceConnectWithoutContext (
          "Rx", MakeCallback (&PacketStats::RxCallback, stats[i]));
    }

  // NetAnim
  AnimationInterface anim ("sensor-star.xml");
  anim.UpdateNodeDescription (0, "Sink");
  anim.UpdateNodeColor (0, 255, 0, 0);
  for (uint32_t i = 0; i < nSensors; ++i)
    {
      anim.UpdateNodeDescription (i+1, "Sensor-" + std::to_string (i+1));
      anim.UpdateNodeColor (i+1, 0, 0, 255);
      anim.UpdateNodeSize (i+1, 1.0, 1.0);
    }

  Simulator::Stop (Seconds (simTime + 1));
  Simulator::Run ();

  // Statistics
  uint32_t sumRx = 0, sumTx = 0;
  Time sumLat = Seconds (0);
  for (uint32_t i = 0; i < nSensors; ++i)
    {
      sumTx += stats[i]->GetTx ();
      sumRx += stats[i]->GetRx ();
      sumLat += stats[i]->GetTotalLatency ();
    }
  double pktDeliveryRatio = sumTx > 0 ? double (sumRx) / sumTx : 0.0;
  double avgLatency = sumRx > 0 ? sumLat.GetSeconds () / sumRx : 0.0;
  double throughput = sumRx * pktSize * 8 / simTime; // in bits/sec

  std::cout << "Simulation results:\n";
  std::cout << "  Sent:    " << sumTx << " packets\n";
  std::cout << "  Received " << sumRx << " packets\n";
  std::cout << "  Delivery ratio: " << pktDeliveryRatio * 100 << " %\n";
  std::cout << "  Average latency: " << avgLatency * 1000 << " ms\n";
  std::cout << "  Throughput: " << throughput / 1000 << " kbps\n";

  Simulator::Destroy ();
  return 0;
}