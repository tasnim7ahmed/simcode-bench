#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/ssid.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiHtTosSimulation");

class ThroughputMonitor
{
public:
  ThroughputMonitor () : m_totalRx (0) { }
  void RxCallback (Ptr<const Packet> packet, const Address & addr) {
    m_totalRx += packet->GetSize ();
  }
  uint64_t GetTotalRx () const { return m_totalRx; }
private:
  uint64_t m_totalRx;
};

int main (int argc, char *argv[])
{
  uint32_t nStations = 4;
  uint8_t mcs = 7; // 0~7
  uint32_t channelWidth = 40; // MHz, 20 or 40
  bool shortGuardInterval = true;
  double distance = 5.0; // meters
  bool useRtsCts = false;
  double simulationTime = 10.0; // seconds
  std::string dataRate = "50Mbps";

  CommandLine cmd;
  cmd.AddValue ("nStations", "Number of Wi-Fi stations", nStations);
  cmd.AddValue ("mcs", "HT MCS (0-7)", mcs);
  cmd.AddValue ("channelWidth", "HT Channel width (20 or 40 MHz)", channelWidth);
  cmd.AddValue ("shortGuardInterval", "Use short guard interval (1) or long (0)", shortGuardInterval);
  cmd.AddValue ("distance", "Distance (m) between stations and AP", distance);
  cmd.AddValue ("useRtsCts", "Enable RTS/CTS for data packets", useRtsCts);
  cmd.AddValue ("dataRate", "UDP traffic data rate", dataRate);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue (useRtsCts ? 0 : 2347));
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2346"));

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nStations);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  phy.Set ("ChannelWidth", UintegerValue (channelWidth));
  phy.Set ("ShortGuardEnabled", BooleanValue (shortGuardInterval));
  phy.Set ("RxNoiseFigure", DoubleValue (7.0));

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_2_4GHZ);
  std::ostringstream oss;
  oss << "HtMcs" << unsigned(mcs);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue (oss.str ()),
                               "ControlMode", StringValue ("HtMcs0"));

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ht-tos-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  for (uint32_t i = 0; i < nStations; ++i)
    {
      double angle = 2 * M_PI * i / nStations;
      double x = distance * std::cos (angle);
      double y = distance * std::sin (angle);
      positionAlloc->Add (Vector (x, y, 0.0));
    }
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);
  mobility.Install (wifiStaNodes);

  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign (apDevice);
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

  // Assign different TOS/DSCP (e.g., AF11, AF21, AF31, AF41) in a round robin way
  std::vector<uint8_t> tosValues = {0x28, 0x48, 0x68, 0x88}; // AF11, AF21, AF31, AF41

  // Application setup & Rx tracing
  uint16_t port = 5000;
  std::vector<Ptr<ThroughputMonitor>> monitors;
  ApplicationContainer serverApps, clientApps;
  for (uint32_t i = 0; i < nStations; ++i)
    {
      // Install UDP server on AP
      UdpServerHelper server (port + i);
      ApplicationContainer serverApp = server.Install (wifiApNode.Get (0));
      serverApp.Start (Seconds (0.0));
      serverApp.Stop (Seconds (simulationTime+1));
      serverApps.Add (serverApp);

      // Install UDP client on STA
      UdpClientHelper client (apInterface.GetAddress (0), port + i);
      client.SetAttribute ("MaxPackets", UintegerValue (0));
      client.SetAttribute ("Interval", TimeValue (Seconds (0.0005)));
      client.SetAttribute ("PacketSize", UintegerValue (1200));
      client.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
      client.SetAttribute ("StopTime", TimeValue (Seconds (simulationTime)));
      client.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
      // Set the TOS field
      client.SetAttribute ("IpTos", UintegerValue (tosValues[i % tosValues.size()]));
      ApplicationContainer clientApp = client.Install (wifiStaNodes.Get (i));
      clientApps.Add (clientApp);

      // Install throughput tracer
      Ptr<ThroughputMonitor> monitor = CreateObject<ThroughputMonitor> ();
      monitors.push_back (monitor);
      Ptr<UdpServer> serverPtr = DynamicCast<UdpServer> (serverApp.Get (0));
      serverPtr->TraceConnectWithoutContext ("Rx", MakeCallback (&ThroughputMonitor::RxCallback, monitor));
    }

  Simulator::Stop (Seconds (simulationTime + 1.0));
  Simulator::Run ();

  // Throughput calculation
  uint64_t totalRx = 0;
  for (uint32_t i = 0; i < monitors.size (); ++i)
    {
      totalRx += monitors[i]->GetTotalRx ();
    }
  double throughput = totalRx * 8.0 / (simulationTime * 1e6); // Mbps

  std::cout << "===== Simulation Results =====" << std::endl;
  std::cout << "Stations: " << nStations << std::endl;
  std::cout << "HT MCS: " << unsigned(mcs) << std::endl;
  std::cout << "Channel Width: " << channelWidth << " MHz" << std::endl;
  std::cout << "Guard Interval: " << (shortGuardInterval ? "Short" : "Long") << std::endl;
  std::cout << "RTS/CTS: " << (useRtsCts ? "Enabled" : "Disabled") << std::endl;
  std::cout << "Distance: " << distance << " m" << std::endl;
  std::cout << "Aggregation UDP Throughput: " << throughput << " Mbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}