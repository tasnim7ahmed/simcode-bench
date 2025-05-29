#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/propagation-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

struct SimulationParams
{
  double simTime = 10.0;
  double distance = 10.0;
  std::string phyType = "Yans";
  std::string wifiStandard = "802.11ac";
  std::string errorModelType = "Nist";
  uint32_t channelWidth = 20;
  bool shortGuardInterval = false;
  std::vector<uint8_t> mcsList = {};
};

std::vector<uint8_t> GetMcsListByStandard (std::string standard)
{
  if (standard == "802.11ac")
    return {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  if (standard == "802.11n")
    return {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  return {0, 1, 2, 3, 4, 5, 6, 7};
}

std::string GetModulation (std::string standard, uint8_t mcs)
{
  if (standard == "802.11ac")
    {
      if (mcs < 1) return "BPSK 1/2";
      if (mcs < 3) return "QPSK 1/2 - 3/4";
      if (mcs < 5) return "16-QAM 1/2 - 3/4";
      if (mcs < 7) return "64-QAM 2/3 - 3/4";
      if (mcs < 9) return "256-QAM 3/4 - 5/6";
      return "256-QAM 5/6";
    }
  if (standard == "802.11n")
    {
      if (mcs < 1) return "BPSK 1/2";
      if (mcs < 3) return "QPSK 1/2 - 3/4";
      if (mcs < 5) return "16-QAM 1/2 - 3/4";
      if (mcs < 7) return "64-QAM 2/3 - 3/4";
      return "64-QAM";
    }
  return "Unknown";
}

void RunSimulation (const SimulationParams &params, uint8_t mcs, std::ostream &out)
{
  NodeContainer wifiStaNode, wifiApNode;
  wifiStaNode.Create (1);
  wifiApNode.Create (1);

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (params.distance, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNode);
  mobility.Install (wifiApNode);

  // PHY & Channel
  Ptr<YansWifiChannel> yansChannel = CreateObject<YansWifiChannel> ();
  Ptr<YansWifiPhy> yansPhy = 0;
  Ptr<SpectrumWifiPhy> spectrumPhy = 0;
  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetStandard (params.wifiStandard == "802.11ac" ? WIFI_PHY_STANDARD_80211ac : WIFI_PHY_STANDARD_80211n);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue ("VhtMcs" + std::to_string (mcs)),
                               "ControlMode", StringValue ("VhtMcs0"));

  WifiMacHelper wifiMac = WifiMacHelper::Default ();
  NetDeviceContainer staDevice, apDevice;

  if (params.phyType == "Spectrum")
    {
      SpectrumWifiPhyHelper phy = SpectrumWifiPhyHelper::Default ();
      Ptr<MultiModelSpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel> ();
      phy.SetChannel (spectrumChannel);
      phy.Set ("ShortGuardEnabled", BooleanValue (params.shortGuardInterval));
      phy.Set ("ChannelWidth", UintegerValue (params.channelWidth));
      if (params.errorModelType == "Nist")
        phy.SetErrorRateModel ("ns3::NistErrorRateModel");
      else
        phy.SetErrorRateModel ("ns3::YansErrorRateModel");
      wifiMac.SetType ("ns3::StaWifiMac",
                       "Ssid", SsidValue (Ssid ("ns3-ssid")),
                       "ActiveProbing", BooleanValue (false));
      staDevice = wifi.Install (phy, wifiMac, wifiStaNode);

      wifiMac.SetType ("ns3::ApWifiMac",
                       "Ssid", SsidValue (Ssid ("ns3-ssid")));
      apDevice = wifi.Install (phy, wifiMac, wifiApNode);
      spectrumPhy = DynamicCast<SpectrumWifiPhy> (staDevice.Get (0)->GetObject<NetDevice> ()->GetObject<WifiNetDevice> ()->GetPhy ());
    }
  else
    {
      YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
      yansChannel->SetPropagationDelayModel (CreateObject<ConstantSpeedPropagationDelayModel> ());
      yansChannel->SetPropagationLossModel (CreateObject<LogDistancePropagationLossModel> ());
      phy.SetChannel (yansChannel);
      phy.Set ("ShortGuardEnabled", BooleanValue (params.shortGuardInterval));
      phy.Set ("ChannelWidth", UintegerValue (params.channelWidth));
      if (params.errorModelType == "Nist")
        phy.SetErrorRateModel ("ns3::NistErrorRateModel");
      else
        phy.SetErrorRateModel ("ns3::YansErrorRateModel");
      wifiMac.SetType ("ns3::StaWifiMac",
                       "Ssid", SsidValue (Ssid ("ns3-ssid")),
                       "ActiveProbing", BooleanValue (false));
      staDevice = wifi.Install (phy, wifiMac, wifiStaNode);

      wifiMac.SetType ("ns3::ApWifiMac",
                       "Ssid", SsidValue (Ssid ("ns3-ssid")));
      apDevice = wifi.Install (phy, wifiMac, wifiApNode);
      yansPhy = DynamicCast<YansWifiPhy> (staDevice.Get (0)->GetObject<NetDevice> ()->GetObject<WifiNetDevice> ()->GetPhy ());
    }

  // Internet stack
  InternetStackHelper stack;
  stack.Install (wifiStaNode);
  stack.Install (wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staIf = address.Assign (staDevice);
  Ipv4InterfaceContainer apIf = address.Assign (apDevice);

  // UDP traffic
  uint16_t port = 9;
  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     InetSocketAddress (staIf.GetAddress (0), port));
  onoff.SetConstantRate (DataRate ("400Mbps"), 1400);
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (params.simTime)));

  ApplicationContainer app = onoff.Install (wifiApNode.Get (0));
  PacketSinkHelper sink ("ns3::UdpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sink.Install (wifiStaNode.Get (0));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (params.simTime));

  FlowMonitorHelper fmHelper;
  Ptr<FlowMonitor> monitor = fmHelper.InstallAll ();

  Simulator::Stop (Seconds (params.simTime));
  Simulator::Run ();

  double throughput = 0.0;
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (fmHelper.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (auto &flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
      if (t.destinationPort == port)
        {
          throughput = flow.second.rxBytes * 8.0 / (params.simTime - 1.0) / 1e6;
        }
    }
  std::string mod = GetModulation (params.wifiStandard, mcs);

  out << "MCS=" << int (mcs)
      << " Modulation=" << mod
      << " Width=" << params.channelWidth << "MHz"
      << " SGI=" << (params.shortGuardInterval ? "Yes" : "No")
      << " Throughput=" << throughput << " Mbps"
      << std::endl;

  Simulator::Destroy ();
}

int main (int argc, char *argv[])
{
  SimulationParams params;
  CommandLine cmd;

  cmd.AddValue ("simTime", "Simulation time (s)", params.simTime);
  cmd.AddValue ("distance", "Distance between STA and AP (meters)", params.distance);
  cmd.AddValue ("phyType", "Phy type: Yans or Spectrum", params.phyType);
  cmd.AddValue ("wifiStandard", "WiFi standard: 802.11ac or 802.11n", params.wifiStandard);
  cmd.AddValue ("errorModelType", "Error Model: Nist or Yans", params.errorModelType);
  cmd.AddValue ("channelWidth", "Channel width (MHz)", params.channelWidth);
  cmd.AddValue ("shortGuardInterval", "Short guard interval", params.shortGuardInterval);

  std::string mcsListStr = "";
  cmd.AddValue ("mcsList", "Comma-separated MCS indices, empty=all", mcsListStr);

  cmd.Parse (argc, argv);

  if (params.mcsList.empty ())
    params.mcsList = GetMcsListByStandard (params.wifiStandard);

  if (!mcsListStr.empty ())
    {
      params.mcsList.clear ();
      std::istringstream iss (mcsListStr);
      std::string token;
      while (std::getline (iss, token, ','))
        params.mcsList.push_back (uint8_t (std::stoi (token)));
    }

  std::cout << "WiFiSimulation: "
            << params.wifiStandard << " Phy=" << params.phyType
            << " ChannelWidth=" << params.channelWidth
            << " ShortGI=" << (params.shortGuardInterval ? "Yes" : "No")
            << " Distance=" << params.distance << "m"
            << " ErrorModel=" << params.errorModelType << std::endl;

  for (uint8_t mcs : params.mcsList)
    RunSimulation (params, mcs, std::cout);

  return 0;
}