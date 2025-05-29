/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiQosAcDemo");

// Helper struct for tracking throughput
struct AppStats
{
  uint64_t bytesReceived = 0;
};

void RxPacket (Ptr<AppStats> stats, Ptr<const Packet> packet, const Address &)
{
  stats->bytesReceived += packet->GetSize ();
}

class TxopTracer : public Object
{
public:
  TxopTracer () : m_totalTxop (Seconds (0)), m_active (false) {}

  void TxopStarted (Time duration)
  {
    if (!m_active)
      {
        m_lastStart = Simulator::Now ();
        m_active = true;
      }
    m_lastDuration = duration;
  }

  void TxopFinished ()
  {
    if (m_active)
      {
        Time end = Simulator::Now ();
        Time txopDuration = end - m_lastStart;
        m_totalTxop += txopDuration;
        m_active = false;
      }
  }

  Time GetTotalTxopTime () const { return m_totalTxop; }

private:
  Time m_lastStart;
  Time m_lastDuration;
  Time m_totalTxop;
  bool m_active;
};

int
main (int argc, char *argv[])
{
  double simulationTime = 10.0;
  uint32_t payloadSize = 1472;
  double distance = 10.0;
  bool enableRtsCts = false;
  bool enablePcap = false;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("simulationTime", "Duration of the simulation in seconds", simulationTime);
  cmd.AddValue ("payloadSize", "UDP payload size in bytes", payloadSize);
  cmd.AddValue ("distance", "Distance between AP and STA (meters)", distance);
  cmd.AddValue ("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (payloadSize));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("20Mbps"));

  // Settings for Wi-Fi
  std::string phyMode = "HtMcs7";
  uint32_t numNetworks = 4;
  std::vector<uint32_t> channels = {36, 40, 44, 48}; // 5 GHz channels (UNII-1 band)

  // Containers
  std::vector<Ptr<Node>> aps(numNetworks), stas(numNetworks);
  NodeContainer wifiStaNodes, wifiApNodes;

  wifiStaNodes.Create (numNetworks);
  wifiApNodes.Create (numNetworks);

  // Mobility setup
  MobilityHelper mobility;

  // Place each AP and STA at opposite ends, stagger them on y axis
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  for (uint32_t i = 0; i < numNetworks; ++i)
    {
      positionAlloc->Add (Vector (0.0, i * 2 * distance, 0.0));        // AP
      positionAlloc->Add (Vector (distance, i * 2 * distance, 0.0));   // STA
    }
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  // Apply to ALL nodes (APs followed by STAs)
  NodeContainer allNodes;
  allNodes.Add (wifiApNodes);
  allNodes.Add (wifiStaNodes);
  mobility.Install (allNodes);

  InternetStackHelper stack;
  stack.Install (allNodes);

  std::vector<NetDeviceContainer> staDevs(numNetworks), apDevs(numNetworks);
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();

  // Use different logical channels (frequency separation)
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);
  QosWifiMacHelper mac = QosWifiMacHelper::Default ();

  // Set up application statistics and tracers for results
  std::vector<Ptr<AppStats>> stats(numNetworks);
  std::vector<Ptr<TxopTracer>> txopTracers(numNetworks);

  // Application port
  uint16_t port = 5000;

  // IP addressing
  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");

  for (uint32_t i = 0; i < numNetworks; ++i)
    {
      // Per-network Wi-Fi setup
      Ptr<YansWifiChannel> phyChannel = channel.Create ();
      phy.SetChannel (phyChannel);

      if (enableRtsCts)
        {
          Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("0"));
        }
      else
        {
          Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
        }

      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                   "DataMode", StringValue (phyMode),
                                   "ControlMode", StringValue ("HtMcs0"));

      // Set unique SSID per network
      std::ostringstream ssidStream;
      ssidStream << "ssid-net-" << i;
      Ssid ssid = Ssid (ssidStream.str ());

      // AP
      mac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid));

      apDevs[i] = phy.Install (wifi, mac, wifiApNodes.Get (i));

      // STA
      mac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));
      staDevs[i] = phy.Install (wifi, mac, wifiStaNodes.Get (i));

      // Set channel (frequency)/bandwidth per network
      phy.Set ("ChannelNumber", UintegerValue (channels[i]));
      phy.Set ("ShortGuardEnabled", BooleanValue (true));
      phy.Set ("ChannelWidth", UintegerValue (20));

      if (enablePcap)
        {
          phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
          phy.EnablePcap ("wifi-ap-" + std::to_string (i), apDevs[i]);
          phy.EnablePcap ("wifi-sta-" + std::to_string (i), staDevs[i]);
        }

      // Internet stack
      NetDeviceContainer ndc;
      ndc.Add (staDevs[i]);
      ndc.Add (apDevs[i]);
      Ipv4InterfaceContainer interfaces = address.Assign (ndc);
      address.NewNetwork ();

      // UdpServer (at AP)
      uint16_t serverPort = port+i;
      UdpServerHelper server (serverPort);
      ApplicationContainer serverApp = server.Install (wifiApNodes.Get (i));
      serverApp.Start (Seconds (0.0));
      serverApp.Stop (Seconds (simulationTime + 1));
      // Track received packets
      stats[i] = CreateObject<AppStats> ();
      server.GetServer ()->TraceConnectWithoutContext (
        "Rx", MakeBoundCallback (&RxPacket, stats[i]));

      // UdpClient (at STA) with OnOffHelper (Qos enabled)
      OnOffHelper onoff ("ns3::UdpSocketFactory",
                         InetSocketAddress (interfaces.GetAddress (1), serverPort));
      onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      onoff.SetAttribute ("PacketSize", UintegerValue (payloadSize));
      onoff.SetAttribute ("DataRate", StringValue ("20Mbps"));

      // Set Qos for traffic: even = AC_BE, odd = AC_VI
      uint8_t tid = (i % 2 == 0) ? 0 : 4; // 0: BE, 4: VI
      onoff.SetAttribute ("QosTid", UintegerValue (tid));

      ApplicationContainer clientApp = onoff.Install (wifiStaNodes.Get (i));
      clientApp.Start (Seconds (1.0));
      clientApp.Stop (Seconds (simulationTime + 1));

      // TXOP duration tracing
      txopTracers[i] = CreateObject<TxopTracer> ();
      std::ostringstream accessCategory;
      accessCategory << "AC_" << ((tid == 0) ? "BE" : "VI");

      std::string path = "/NodeList/" +
        std::to_string (wifiStaNodes.Get (i)->GetId ()) +
        "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DcaTxop/" +
        accessCategory.str () + "/TxopStart";

      // Try to find the correct path for tracing per AC. Use dynamic connect.
      // For simplicity, connect on the station MAC. (Note: if DcaTxop is not available, can be more complex)
      std::string txopStart = "/NodeList/" + std::to_string (wifiStaNodes.Get (i)->GetId ()) +
        "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/" +
        accessCategory.str () + "/TxopStart";

      std::string txopFinish = "/NodeList/" + std::to_string (wifiStaNodes.Get (i)->GetId ()) +
        "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/" +
        accessCategory.str () + "/TxopFinished";

      // Connect using Config::Connect* API. If trace source not found, skip.
      Config::ConnectWithoutContext (txopStart,
        MakeCallback ([tracer = txopTracers[i]] (Time duration) {
          tracer->TxopStarted (duration);
        }));

      Config::ConnectWithoutContext (txopFinish,
        MakeCallback ([tracer = txopTracers[i]] () mutable {
          tracer->TxopFinished ();
        }));
    }

  Simulator::Stop (Seconds (simulationTime + 2));
  Simulator::Run ();

  std::cout << "======== Simulation Results ========" << std::endl;
  for (uint32_t i = 0; i < numNetworks; ++i)
    {
      std::string acStr = (i % 2 == 0) ? "AC_BE" : "AC_VI";
      double throughput = static_cast<double> (stats[i]->bytesReceived * 8) / (simulationTime * 1e6); // Mbps
      Time txopTime = txopTracers[i]->GetTotalTxopTime ();

      std::cout << "Network " << i << " (" << acStr << ")\n";
      std::cout << "  Throughput: " << throughput << " Mbps\n";
      std::cout << "  Total TXOP duration: " << txopTime.GetSeconds () << " s\n";
    }

  Simulator::Destroy ();
  return 0;
}