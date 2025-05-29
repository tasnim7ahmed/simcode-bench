#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/wifi-mac-header.h"
#include "ns3/wifi-phy-standard.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiAxPerformance");

struct SimulationConfig
{
  uint32_t nStations = 4;
  double simTime = 10.0;
  double distance = 20.0;
  std::string phyModel = "yans"; // "yans" or "spectrum"
  std::string band = "5GHz";     // "2.4GHz", "5GHz", "6GHz"
  bool useUdp = true;            // true for UDP, false for TCP
  uint32_t mcs = 5;
  uint32_t channelWidth = 80;
  bool useRtsCts = false;
  bool use80_80MHz = false;
  bool extBlockAck = false;
  bool ulOfdma = false;
  uint16_t payloadSize = 1400;
  double offeredLoad = 200e6;    // bits per second
  std::string guardInterval = "HE-GI-0.8"; // "HE-GI-0.8", "HE-GI-1.6", "HE-GI-3.2"
};

void PrintThroughput (Ptr<PacketSink> sink, Time startTime, double simTime, std::string proto, uint32_t stnId)
{
  uint64_t rxBytes = sink->GetTotalRx ();
  double throughput = rxBytes * 8.0 / (simTime * 1e6); // Mbps
  std::cout << proto << ": Station " << stnId << " throughput: " << throughput << " Mbps" << std::endl;
}

int main (int argc, char *argv[])
{
  SimulationConfig config;
  CommandLine cmd;
  cmd.AddValue ("nStations", "Number of stations", config.nStations);
  cmd.AddValue ("simTime", "Simulation time", config.simTime);
  cmd.AddValue ("distance", "Distance between AP and STAs", config.distance);
  cmd.AddValue ("phyModel", "PHY model: yans or spectrum", config.phyModel);
  cmd.AddValue ("band", "WLAN band: 2.4GHz | 5GHz | 6GHz", config.band);
  cmd.AddValue ("useUdp", "UDP (1) or TCP (0)", config.useUdp);
  cmd.AddValue ("mcs", "HE-MCS index [0..11/13 depending on bandwidth]", config.mcs);
  cmd.AddValue ("channelWidth", "Channel width (20,40,80,160)", config.channelWidth);
  cmd.AddValue ("useRtsCts", "Enable RTS/CTS", config.useRtsCts);
  cmd.AddValue ("use80_80MHz", "Enable 80+80MHz", config.use80_80MHz);
  cmd.AddValue ("extBlockAck", "Enable Extended Block Ack", config.extBlockAck);
  cmd.AddValue ("ulOfdma", "Enable UL OFDMA", config.ulOfdma);
  cmd.AddValue ("payloadSize", "Packet payload size", config.payloadSize);
  cmd.AddValue ("offeredLoad", "Offered load per STA (bps)", config.offeredLoad);
  cmd.AddValue ("guardInterval", "HE guard interval: HE-GI-0.8 | HE-GI-1.6 | HE-GI-3.2", config.guardInterval);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue (config.useRtsCts ? 0 : 999999));
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));

  NodeContainer wifiStaNodes, wifiApNode;
  wifiStaNodes.Create (config.nStations);
  wifiApNode.Create (1);

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 1.2)); // AP
  for (uint32_t i = 0; i < config.nStations; ++i)
    positionAlloc->Add (Vector (config.distance, (i * 2.0), 1.0)); // STAs
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);
  mobility.Install (wifiStaNodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211ax);

  Ssid ssid = Ssid ("ns3-80211ax");

  NetDeviceContainer staDevices, apDevice;
  if (config.phyModel == "yans")
    {
      YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
      YansWifiChannelHelper chan = YansWifiChannelHelper::Default ();
      phy.SetChannel (chan.Create ());
      phy.Set ("ChannelWidth", UintegerValue (config.channelWidth));
      if (config.use80_80MHz)
        phy.Set ("Frequency", UintegerValue (5210));
      if (config.band == "2.4GHz")
        phy.Set ("Frequency", UintegerValue (2412));
      else if (config.band == "5GHz")
        phy.Set ("Frequency", UintegerValue (5180));
      else if (config.band == "6GHz")
        phy.Set ("Frequency", UintegerValue (5955));
      phy.Set ("GuardInterval", StringValue (config.guardInterval));
      WifiMacHelper mac;
      mac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "QosSupported", BooleanValue (true),
                   "BE_MaxAmsduSize", UintegerValue (7935),
                   "BE_MaxAmpduSize", UintegerValue (1048575),
                   "HE_EXTENDED_BLOCK_ACK", BooleanValue (config.extBlockAck));
      wifi.SetRemoteStationManager ("ns3::HeWifiManager",
                                   "MaxHeMcs", UintegerValue (config.mcs),
                                   "MinHeMcs", UintegerValue (config.mcs));
      staDevices = wifi.Install (phy, mac, wifiStaNodes);

      WifiMacHelper apMac;
      apMac.SetType ("ns3::ApWifiMac",
                     "Ssid", SsidValue (ssid),
                     "QosSupported", BooleanValue (true),
                     "HE_EXTENDED_BLOCK_ACK", BooleanValue (config.extBlockAck));
      apDevice = wifi.Install (phy, apMac, wifiApNode);
    }
  else // spectrum
    {
      SpectrumWifiPhyHelper phy = SpectrumWifiPhyHelper::Default ();
      if (config.band == "2.4GHz")
        phy.Set ("Frequency", UintegerValue (2412));
      else if (config.band == "5GHz")
        phy.Set ("Frequency", UintegerValue (5180));
      else if (config.band == "6GHz")
        phy.Set ("Frequency", UintegerValue (5955));
      phy.Set ("ChannelWidth", UintegerValue (config.channelWidth));
      phy.Set ("GuardInterval", StringValue (config.guardInterval));
      WifiMacHelper mac;
      mac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "QosSupported", BooleanValue (true),
                   "BE_MaxAmsduSize", UintegerValue (7935),
                   "BE_MaxAmpduSize", UintegerValue (1048575),
                   "HE_EXTENDED_BLOCK_ACK", BooleanValue (config.extBlockAck));
      wifi.SetRemoteStationManager ("ns3::HeWifiManager",
                                   "MaxHeMcs", UintegerValue (config.mcs),
                                   "MinHeMcs", UintegerValue (config.mcs));
      staDevices = wifi.Install (phy, mac, wifiStaNodes);

      WifiMacHelper apMac;
      apMac.SetType ("ns3::ApWifiMac",
                     "Ssid", SsidValue (ssid),
                     "QosSupported", BooleanValue (true),
                     "HE_EXTENDED_BLOCK_ACK", BooleanValue (config.extBlockAck));
      apDevice = wifi.Install (phy, apMac, wifiApNode);
    }

  if (config.ulOfdma)
    {
      Config::SetDefault ("ns3::WifiMac::UseUora", BooleanValue (true));
    }

  // Install networking stack
  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);

  // IP assignment
  Ipv4AddressHelper address;
  address.SetBase ("10.1.0.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces, apInterface;
  staInterfaces = address.Assign (staDevices);
  apInterface = address.Assign (apDevice);

  // Applications
  double appStart = 1.0;
  double appStop = config.simTime - 1.0;
  std::vector<Ptr<Application>> clientApps;
  std::vector<Ptr<Application>> serverApps;
  std::vector<Ptr<PacketSink>> sinks;

  uint16_t port = 9;
  if (config.useUdp)
    {
      UdpServerHelper udpServer (port);
      ApplicationContainer serverApp = udpServer.Install (wifiApNode.Get (0));
      serverApp.Start (Seconds (appStart));
      serverApp.Stop (Seconds (appStop));
      sinks.push_back (DynamicCast<PacketSink> (serverApp.Get (0)));
      for (uint32_t i = 0; i < config.nStations; ++i)
        {
          double offeredRate = config.offeredLoad / 8.0 / config.nStations; // bytes/sec
          UdpClientHelper udpClient (apInterface.GetAddress (0), port);
          udpClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
          udpClient.SetAttribute ("Interval", TimeValue (Seconds (double (config.payloadSize) / offeredRate)));
          udpClient.SetAttribute ("PacketSize", UintegerValue (config.payloadSize));
          ApplicationContainer clientApp = udpClient.Install (wifiStaNodes.Get (i));
          clientApp.Start (Seconds (appStart));
          clientApp.Stop (Seconds (appStop));
          clientApps.push_back (clientApp.Get (0));
        }
    }
  else // TCP
    {
      uint16_t tcpPort = 50000;
      Address apLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), tcpPort));
      PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", apLocalAddress);
      ApplicationContainer packetSinkApp = packetSinkHelper.Install (wifiApNode.Get (0));
      packetSinkApp.Start (Seconds (appStart));
      packetSinkApp.Stop (Seconds (appStop));
      sinks.push_back (DynamicCast<PacketSink> (packetSinkApp.Get (0)));

      for (uint32_t i = 0; i < config.nStations; ++i)
        {
          OnOffHelper onoff ("ns3::TcpSocketFactory",
                             InetSocketAddress (apInterface.GetAddress (0), tcpPort));
          onoff.SetAttribute ("DataRate", DataRateValue (DataRate (uint64_t (config.offeredLoad / config.nStations))));
          onoff.SetAttribute ("PacketSize", UintegerValue (config.payloadSize));
          onoff.SetAttribute ("StartTime", TimeValue (Seconds (appStart)));
          onoff.SetAttribute ("StopTime", TimeValue (Seconds (appStop)));
          ApplicationContainer app = onoff.Install (wifiStaNodes.Get (i));
          clientApps.push_back (app.Get (0));
        }
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Tracing (pcap)
  wifi.EnablePcapAll ("wifi-ax-performance");

  Simulator::Schedule (Seconds (config.simTime),
                       [&]()
                       {
                         for (uint32_t i = 0; i < sinks.size (); ++i)
                           {
                             PrintThroughput (sinks.at (i), Seconds (appStart), appStop - appStart, config.useUdp ? "UDP" : "TCP", i);
                           }
                       });

  Simulator::Stop (Seconds (config.simTime + 1));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}