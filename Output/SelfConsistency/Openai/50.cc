/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * This program sets up a simple Wi-Fi network with a configurable
 * PHY layer (YansWifiPhy or SpectrumWifiPhy). It allows experimentation
 * with different MCS indices, guard intervals, channel widths, and
 * error models, as well as simulation parameters such as simulation time
 * and distance between nodes. Throughput statistics per MCS are output.
 *
 * Usage example:
 *  ./waf --run "wifi-mcs-experiment --simulationTime=10 --distance=20 --phyType=Yans --channelWidth=20 --shortGuardInterval=true --mcs=9 --errorModel=Nist"
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMcsExperiment");

// Custom sink to record received bytes and timestamps.
class ThroughputSink : public Application
{
public:
  ThroughputSink () : m_totalRx (0) {}
  virtual ~ThroughputSink () {}

  void Setup (Address addr, uint16_t port)
  {
    m_local = InetSocketAddress (Ipv4Address::GetAny (), port);
  }

  uint64_t GetTotalRx () const { return m_totalRx; }
  Time GetStartTime () const { return m_startTime; }
  Time GetLastRxTime () const { return m_lastRx; }

private:
  virtual void StartApplication () override
  {
    if (!m_socket)
      {
        m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
        m_socket->Bind (m_local);
        m_socket->SetRecvCallback (MakeCallback (&ThroughputSink::HandleRead, this));
      }
    m_startTime = Simulator::Now ();
  }

  virtual void StopApplication () override
  {
    if (m_socket) { m_socket->Close (); }
  }

  void HandleRead (Ptr<Socket> socket)
  {
    Ptr<Packet> p;
    Address from;
    while ((p = socket->RecvFrom (from)))
      {
        if (m_totalRx == 0)
          {
            m_startTime = Simulator::Now ();
          }
        m_totalRx += p->GetSize ();
        m_lastRx = Simulator::Now ();
      }
  }

  Ptr<Socket> m_socket;
  Address m_local;
  uint64_t m_totalRx;
  Time m_startTime;
  Time m_lastRx;
};

// Function to get a readable MCS description depending on PHY standard
std::string GetMcsDescription (uint8_t mcs, WifiPhyStandard standard)
{
  std::ostringstream os;
  if (standard == WIFI_PHY_STANDARD_80211n_5GHZ || standard == WIFI_PHY_STANDARD_80211n_2_4GHZ)
    {
      os << "MCS-" << unsigned (mcs);
    }
  else if (standard == WIFI_PHY_STANDARD_80211ac)
    {
      os << "VHT-MCS-" << unsigned (mcs);
    }
  else if (standard == WIFI_PHY_STANDARD_80211ax_5GHZ || standard == WIFI_PHY_STANDARD_80211ax_2_4GHZ)
    {
      os << "HE-MCS-" << unsigned (mcs);
    }
  else
    {
      os << "UnknownMCS-" << unsigned (mcs);
    }
  return os.str ();
}

int main (int argc, char *argv[])
{
  double simulationTime = 10.0; // seconds
  double distance = 10.0; // meters
  std::string phyType = "Yans"; // or "Spectrum"
  uint32_t channelWidth = 20; // MHz: 20/40/80/160
  bool shortGuardInterval = false;
  uint8_t mcs = 7; // For 802.11n: 0..7/15, for 11ac/ax up to 9/11
  std::string wifiStandard = "802.11n_5GHz"; // settable: 802.11n_5GHz, 802.11ac, 802.11ax
  std::string errorModelType = "Nist"; // "Nist" or "Yans"
  uint16_t udpPort = 5000;
  double appDataRateMbps = 1000.0; // UDP application rate in Mbps

  CommandLine cmd (__FILE__);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("distance", "Distance between AP and STA (meters)", distance);
  cmd.AddValue ("phyType", "Physical layer type: Yans or Spectrum", phyType);
  cmd.AddValue ("channelWidth", "Channel width (MHz)", channelWidth);
  cmd.AddValue ("shortGuardInterval", "Enable short guard interval (400ns)", shortGuardInterval);
  cmd.AddValue ("mcs", "MCS index (depends on standard)", mcs);
  cmd.AddValue ("wifiStandard", "WiFi standard: 802.11n_5GHz, 802.11ac, 802.11ax_5GHz", wifiStandard);
  cmd.AddValue ("errorModel", "Error model: Nist or Yans", errorModelType);
  cmd.AddValue ("appDataRate", "UDP Application offered data rate in Mbps", appDataRateMbps);
  cmd.Parse (argc, argv);

  WifiHelper wifi;
  WifiPhyStandard phyStandard;
  if (wifiStandard == "802.11n_5GHz")
    {
      phyStandard = WIFI_PHY_STANDARD_80211n_5GHZ;
    }
  else if (wifiStandard == "802.11ac")
    {
      phyStandard = WIFI_PHY_STANDARD_80211ac;
    }
  else if (wifiStandard == "802.11ax_5GHz")
    {
      phyStandard = WIFI_PHY_STANDARD_80211ax_5GHZ;
    }
  else if (wifiStandard == "802.11n_2_4GHz")
    {
      phyStandard = WIFI_PHY_STANDARD_80211n_2_4GHZ;
    }
  else if (wifiStandard == "802.11ax_2_4GHz")
    {
      phyStandard = WIFI_PHY_STANDARD_80211ax_2_4GHZ;
    }
  else
    {
      NS_ABORT_MSG ("Unsupported WiFi standard: " << wifiStandard);
    }

  // PHY/Channel
  Ptr<YansWifiPhy> yansWifiPhy;
  Ptr<SpectrumWifiPhy> spectrumWifiPhy;
  Ptr<WifiChannel> wifiChannel;
  NetDeviceContainer devices;

  // Helper pointers
  Ptr<YansWifiChannel> yansChannel;
  Ptr<SpectrumChannel> spectrumChannel;

  // Node containers
  NodeContainer wifiStaNode, wifiApNode;
  wifiStaNode.Create (1);
  wifiApNode.Create (1);

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator> ();
  posAlloc->Add (Vector (0.0, 0.0, 1.0));       // AP position
  posAlloc->Add (Vector (distance, 0.0, 1.0));  // STA position
  mobility.SetPositionAllocator (posAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);
  mobility.Install (wifiStaNode);

  // Configure mac/phy helpers
  Ssid ssid = Ssid ("wifi-mcs-exp");
  YansWifiPhyHelper yansPhy;
  SpectrumWifiPhyHelper spectrumPhy;
  WifiMacHelper wifiMac;

  if (phyType == "Yans")
    {
      yansChannel = CreateObject<YansWifiChannel> ();
      Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel> ();
      Ptr<LogDistancePropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel> ();
      yansChannel->SetPropagationDelayModel (delayModel);
      yansChannel->SetPropagationLossModel (lossModel);

      yansPhy.SetChannel (yansChannel);

      yansPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
      yansPhy.Set ("ChannelWidth", UintegerValue (channelWidth));
      yansPhy.Set ("ShortGuardEnabled", BooleanValue (shortGuardInterval));
      yansPhy.Set ("Standard", EnumValue (phyStandard));
    }
  else if (phyType == "Spectrum")
    {
      spectrumChannel = CreateObject<MultiModelSpectrumChannel> ();
      Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel> ();
      Ptr<LogDistancePropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel> ();
      spectrumChannel->AddPropagationLossModel (lossModel);
      spectrumChannel->SetPropagationDelayModel (delayModel);

      spectrumPhy.SetChannel (spectrumChannel);
      spectrumPhy.Set ("ChannelWidth", UintegerValue (channelWidth));
      spectrumPhy.Set ("ShortGuardEnabled", BooleanValue (shortGuardInterval));
      spectrumPhy.Set ("Standard", EnumValue (phyStandard));
    }
  else
    {
      NS_ABORT_MSG ("phyType should be Yans or Spectrum");
    }

  // Error model
  if (errorModelType == "Nist")
    {
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                    "DataMode", StringValue ("HtMcs" + std::to_string (mcs)),
                                    "ControlMode", StringValue ("HtMcs0"));
    }
  else if (errorModelType == "Yans")
    {
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                    "DataMode", StringValue ("HtMcs" + std::to_string (mcs)),
                                    "ControlMode", StringValue ("HtMcs0"));
    }
  else
    {
      NS_ABORT_MSG ("Unsupported error model: " << errorModelType);
    }

  // MAC
  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));
  if (phyType == "Yans")
    devices.Add (wifi.Install (yansPhy, wifiMac, wifiStaNode));
  else
    devices.Add (wifi.Install (spectrumPhy, wifiMac, wifiStaNode));

  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid));
  if (phyType == "Yans")
    devices.Add (wifi.Install (yansPhy, wifiMac, wifiApNode));
  else
    devices.Add (wifi.Install (spectrumPhy, wifiMac, wifiApNode));

  // Internet stack
  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // UDP Application: server on AP, client on STA
  Ptr<ThroughputSink> sink = CreateObject<ThroughputSink> ();
  sink->Setup (Ipv4Address::GetAny (), udpPort);
  wifiApNode.Get (0)->AddApplication (sink);
  sink->SetStartTime (Seconds (0.0));
  sink->SetStopTime (Seconds (simulationTime + 2));

  // UDP client app on STA
  uint32_t maxPacketSize = 1472;
  OnOffHelper client ("ns3::UdpSocketFactory",
                      InetSocketAddress (interfaces.GetAddress (1), udpPort));
  client.SetAttribute ("DataRate", DataRateValue (DataRate (appDataRateMbps * 1e6)));
  client.SetAttribute ("PacketSize", UintegerValue (maxPacketSize));
  client.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  client.SetAttribute ("StopTime", TimeValue (Seconds (simulationTime)));

  ApplicationContainer clientApp = client.Install (wifiStaNode);

  Simulator::Stop (Seconds (simulationTime + 2.0));
  Simulator::Run ();

  double rxBytes = sink->GetTotalRx ();
  double throughputMbps = rxBytes * 8.0 / (simulationTime * 1e6);

  std::cout << "========== Simulation Results ==========" << std::endl;
  std::cout << "Wi-Fi Standard: " << wifiStandard << std::endl;
  std::cout << "PHY Type: " << phyType << std::endl;
  std::cout << "MCS Index: " << unsigned (mcs)
            << " (" << GetMcsDescription (mcs, phyStandard) << ")"
            << std::endl;
  std::cout << "Channel width: " << channelWidth << " MHz" << std::endl;
  std::cout << "Short Guard Interval: " << (shortGuardInterval ? "Enabled" : "Disabled") << std::endl;
  std::cout << "Distance: " << distance << " m" << std::endl;
  std::cout << "Error Model: " << errorModelType << std::endl;
  std::cout << "Simulation Time: " << simulationTime << " s" << std::endl;
  std::cout << "UDP Offered Data Rate: " << appDataRateMbps << " Mbps" << std::endl;
  std::cout << std::setprecision (4) << "Received Throughput: " << throughputMbps << " Mbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}