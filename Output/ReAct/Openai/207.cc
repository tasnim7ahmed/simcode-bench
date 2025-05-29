#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiTcpAodvExample");

class ThroughputCalculator
{
public:
  ThroughputCalculator() : m_lastTotalRx (0) {}

  void Setup (Ptr<PacketSink> sink)
  {
    m_sink = sink;
    m_startTime = Simulator::Now ();
    m_stream = &std::cout;
    Simulator::Schedule (Seconds (1.0), &ThroughputCalculator::Calculate, this);
  }
private:
  void Calculate ()
  {
    Time curTime = Simulator::Now ();
    uint64_t totalRx = m_sink->GetTotalRx ();
    double throughput = (totalRx - m_lastTotalRx) * 8.0 / 1000000.0; // Mbps
    *m_stream << curTime.GetSeconds () << "s: Throughput: " << throughput << " Mbps" << std::endl;
    m_lastTotalRx = totalRx;
    if (curTime < Seconds (simulationTime - 0.1))
      Simulator::Schedule (Seconds (1.0), &ThroughputCalculator::Calculate, this);
  }

  Ptr<PacketSink> m_sink;
  Time m_startTime;
  uint64_t m_lastTotalRx;
  std::ostream* m_stream;
  static constexpr double simulationTime = 10.0;
};

int main (int argc, char *argv[])
{
  double simulationTime = 10.0;

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g);

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("simple-wifi");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevice;
  staDevice = wifi.Install (phy, mac, nodes.Get (1));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, nodes.Get (0));

  NetDeviceContainer devices;
  devices.Add (apDevice.Get (0));
  devices.Add (staDevice.Get (0));

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (20.0),
                                "DeltaY", DoubleValue (0.0),
                                "GridWidth", UintegerValue (2),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper stack;
  AodvHelper aodv;
  stack.SetRoutingHelper (aodv);
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // TCP Server (PacketSink) on Node 0
  uint16_t port = 50000;
  Address serverAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", serverAddress);
  ApplicationContainer serverApp = packetSinkHelper.Install (nodes.Get (0));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (simulationTime + 1));

  // TCP Client (OnOffApplication) on Node 1
  OnOffHelper client ("ns3::TcpSocketFactory",
                      Address (InetSocketAddress (interfaces.GetAddress (0), port)));
  client.SetAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  client.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  client.SetAttribute ("StopTime", TimeValue (Seconds (simulationTime)));

  ApplicationContainer clientApp = client.Install (nodes.Get (1));

  // Throughput calculation
  Ptr<PacketSink> sink = DynamicCast<PacketSink> (serverApp.Get (0));
  ThroughputCalculator calc;
  calc.Setup (sink);

  // NetAnim
  AnimationInterface anim ("wifi-tcp-aodv.xml");
  anim.SetConstantPosition (nodes.Get (0), 0, 0);
  anim.SetConstantPosition (nodes.Get (1), 20, 0);

  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}