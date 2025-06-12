// wireless-interference-rss.cc

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WirelessInterferenceRss");

int main (int argc, char *argv[])
{
  double primaryRssDbm = -80.0;
  double interfererRssDbm = -80.0;
  double timeOffset = 0.0; // seconds
  uint32_t primaryPacketSize = 1400;
  uint32_t interfererPacketSize = 1400;
  bool verbose = false;
  std::string phyMode ("DsssRate11Mbps");
  std::string pcapPrefix = "wireless-interference-rss";

  CommandLine cmd;
  cmd.AddValue ("primaryRssDbm", "RSS at receiver for the primary transmission (dBm, default: -80)", primaryRssDbm);
  cmd.AddValue ("interfererRssDbm", "RSS at receiver for the interferer transmission (dBm, default: -80)", interfererRssDbm);
  cmd.AddValue ("timeOffset", "Time offset between primary and interferer transmissions (s)", timeOffset);
  cmd.AddValue ("primaryPacketSize", "Packet size for the primary transmission (bytes)", primaryPacketSize);
  cmd.AddValue ("interfererPacketSize", "Packet size for the interferer transmission (bytes)", interfererPacketSize);
  cmd.AddValue ("verbose", "Enable verbose Wi-Fi logging", verbose);
  cmd.AddValue ("phyMode", "Wi-Fi PHY mode", phyMode);
  cmd.AddValue ("pcapPrefix", "Pcap trace file prefix", pcapPrefix);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      wifiHelper.EnableLogComponents ();
      LogComponentEnable ("YansWifiPhy", LOG_LEVEL_ALL);
      LogComponentEnable ("MacLow", LOG_LEVEL_ALL);
    }

  // Create nodes
  NodeContainer nodes;
  nodes.Create (3); // 0: transmitter, 1: receiver, 2: interferer

  // WiFi setup
  WifiHelper wifiHelper;
  wifiHelper.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default ();
  phyHelper.SetChannel (channelHelper.Create ());
  phyHelper.Set ("TxPowerStart", DoubleValue (0.0));
  phyHelper.Set ("TxPowerEnd", DoubleValue (0.0));
  phyHelper.Set ("EnergyDetectionThreshold", DoubleValue (-105.0));
  phyHelper.Set ("RxSensitivity", DoubleValue (-105.0));

  WifiMacHelper macHelper;

  Ssid ssid = Ssid ("wifi-rss");

  macHelper.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifiHelper.Install (phyHelper, macHelper, nodes);

  // Force data rate for control and data
  wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue (phyMode),
                                      "ControlMode", StringValue (phyMode));

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();

  // We will position nodes on a line: transmitter-receiver-interferer
  // We'll calculate Tx-Rx and Interferer-Rx distances to achieve specified RSS using Friis
  double lambda = 3e8 / 2.4e9; // 2.4GHz for 802.11b
  double txPowerDbm = 0.0; // Set by us in phy
  double rxGainDb = 0.0;
  double txGainDb = 0.0;
  double systemLossDb = 0.0;

  auto computeDistanceForRss = [&] (double txPowerDbm, double rxPowerDbm) -> double
  {
    // Friis: Pr/Pt = (Gt*Gr * lambda^2)/(16*pi^2*d^2) / L
    // rearranged: d = lambda / (4*pi) * 10^((Pt + Gt + Gr - L - Pr)/20)
    double exponent = (txPowerDbm + txGainDb + rxGainDb - systemLossDb - rxPowerDbm) / 20.0;
    double d = lambda / (4 * M_PI) * std::pow (10.0, exponent);
    return d;
  };

  double txRxDistance = computeDistanceForRss (txPowerDbm, primaryRssDbm);
  double interfererRxDistance = computeDistanceForRss (txPowerDbm, interfererRssDbm);

  // Place nodes accordingly:
  // Transmitter at (0,0,0), Receiver at (txRxDistance,0,0), Interferer at (txRxDistance,interfererRxDistance,0)
  positionAlloc->Add (Vector (0.0, 0.0, 0.0)); // Transmitter
  positionAlloc->Add (Vector (txRxDistance, 0.0, 0.0)); // Receiver
  positionAlloc->Add (Vector (txRxDistance, interfererRxDistance, 0.0)); // Interferer

  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Install Internet stack for applications
  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaces = ipv4.Assign (devices);

  // Applications
  uint16_t port = 5000;

  // UDP receiver on node 1
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory",
                              InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (5.0));

  // Primary transmitter: node 0 -> node 1
  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     InetSocketAddress (ifaces.GetAddress (1), port));
  onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("54Mbps"))); // Very fast so size dominates tx duration
  onoff.SetAttribute ("PacketSize", UintegerValue (primaryPacketSize));
  onoff.SetAttribute ("MaxPackets", UintegerValue (1));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (4.0)));

  ApplicationContainer primaryApp = onoff.Install (nodes.Get (0));

  // Interferer: node 2 -> node 1, no carrier sense (use Raw sockets)
  Ptr<Socket> interfererSocket = Socket::CreateSocket (nodes.Get (2), UdpSocketFactory::GetTypeId ());
  interfererSocket->SetAllowBroadcast (true);

  Simulator::Schedule (Seconds (1.0 + timeOffset), [=] {
      // Send one UDP packet directly
      Ptr<Packet> packet = Create<Packet> (interfererPacketSize);
      interfererSocket->SendTo (packet, 0, InetSocketAddress (ifaces.GetAddress (1), port));
    });

  // Wi-Fi pcap tracing
  phyHelper.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  phyHelper.EnablePcapAll (pcapPrefix, true);

  Simulator::Stop (Seconds (5.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}