#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/ssid.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/spectrum-module.h"
#include "ns3/waveform-generator-helper.h"
#include "ns3/error-model.h"
#include "ns3/packet-sink.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiInterferenceSimulation");

void
PhyStats (Ptr<WifiPhy> phy)
{
  Time now = Simulator::Now ();
  double rssi = phy->GetLastRxSignalStrength ();
  double noise = phy->GetNoiseFigure ();
  double snr = phy->CalculateSnr (rssi, noise);
  std::cout << now.GetSeconds () << "s: "
            << "RSSI=" << rssi << " dBm, "
            << "Noise=" << noise << " dB, "
            << "SNR=" << snr << " dB" << std::endl;
  Simulator::Schedule (Seconds (1.0), &PhyStats, phy);
}

void
ThroughputMonitor (Ptr<PacketSink> sink, double interval)
{
  static uint64_t lastBytes = 0;
  static Time lastTime = Seconds (0.0);
  Time now = Simulator::Now ();
  uint64_t curBytes = sink->GetTotalRx ();
  double throughput = (curBytes - lastBytes) * 8.0 / (now.GetSeconds () - lastTime.GetSeconds ()) / 1e6; // Mbps
  std::cout << now.GetSeconds () << "s: Throughput = " << throughput << " Mbps" << std::endl;
  lastBytes = curBytes;
  lastTime = now;
  Simulator::Schedule (Seconds (interval), &ThroughputMonitor, sink, interval);
}

struct WifiConfig
{
  uint8_t mcs;
  uint8_t channelWidth;
  bool shortGuard;
  std::string wifiType;
  std::string errorModel;
};

WifiConfig
IndexToWifiConfig (uint32_t index)
{
  // Sample mapping; can be expanded or read from array.
  std::vector<WifiConfig> configs{
      {7, 20, false, "yans", ""},      // index 0
      {7, 20, true,  "yans", ""},      // index 1
      {9, 40, false, "spectrum", "ns3::NistErrorRateModel"}, // index 2
      {9, 40, true,  "spectrum", ""}   // index 3
  };
  if (index >= configs.size ()) index = 0;
  return configs[index];
}

int main (int argc, char *argv[])
{
  double simulationTime = 10.0;
  double distance = 5.0;
  uint32_t configIndex = 0;
  double waveformPowerDbm = 10.0;
  std::string trafficType = "tcp";
  bool enablePcap = false;

  CommandLine cmd;
  cmd.AddValue ("time", "Simulation time (s)", simulationTime);
  cmd.AddValue ("distance", "Distance between STA and AP (meters)", distance);
  cmd.AddValue ("configIndex", "Wi-Fi config index", configIndex);
  cmd.AddValue ("waveformPower", "Non-Wi-Fi interferer power (dBm)", waveformPowerDbm);
  cmd.AddValue ("trafficType", "Traffic type: tcp or udp", trafficType);
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.Parse (argc, argv);

  WifiConfig wifiConfig = IndexToWifiConfig (configIndex);

  NodeContainer wifiNodes;
  wifiNodes.Create (2); // STA + AP

  NodeContainer interfererNode;
  interfererNode.Create (1);

  // Mobility: AP at (0,0,0), STA at (distance,0,0), interferer at (distance/2, distance, 0)
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator> ();
  pos->Add (Vector (0.0, 0.0, 0.0));              // AP
  pos->Add (Vector (distance, 0.0, 0.0));         // STA
  mobility.SetPositionAllocator (pos);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiNodes);

  MobilityHelper mobIntf;
  Ptr<ListPositionAllocator> posIntf = CreateObject<ListPositionAllocator> ();
  posIntf->Add (Vector (distance/2.0, distance, 0.0));
  mobIntf.SetPositionAllocator (posIntf);
  mobIntf.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobIntf.Install (interfererNode);

  // Wi-Fi setup
  Ssid ssid = Ssid ("wifi-ssid");
  NetDeviceContainer wifiDevs;
  Ptr<YansWifiChannel> yansChannel;
  Ptr<SpectrumChannel> spectrumChannel;
  Ptr<WifiPhy> staPhy;
  if (wifiConfig.wifiType == "spectrum")
    {
      SpectrumWifiPhyHelper phy = SpectrumWifiPhyHelper::Default ();
      SpectrumChannelHelper chanHelper = SpectrumChannelHelper::Default ();
      spectrumChannel = chanHelper.Create ();
      phy.SetChannel (spectrumChannel);
      SpectrumWifiHelper wifi;
      wifi.SetStandard (WIFI_STANDARD_80211n);
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                    "DataMode", StringValue ("HtMcs" + std::to_string (wifiConfig.mcs)),
                                    "ControlMode", StringValue ("HtMcs0"));
      WifiMacHelper mac;
      mac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));
      wifiDevs.Add (wifi.Install (phy, mac, wifiNodes.Get (0))); // STA
      mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
      wifiDevs.Add (wifi.Install (phy, mac, wifiNodes.Get (1))); // AP
      phy.Set ("ShortGuardEnabled", BooleanValue (wifiConfig.shortGuard));
      phy.Set ("ChannelWidth", UintegerValue (wifiConfig.channelWidth));
      if (!wifiConfig.errorModel.empty ())
        {
          phy.Set ("ErrorRateModel", StringValue (wifiConfig.errorModel));
        }
      staPhy = DynamicCast<SpectrumWifiPhy> (wifiDevs.Get (0)->GetObject<NetDevice> ()->GetPhy ());
      // Only first device for stats
    }
  else // yans
    {
      YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
      yansChannel = channel.Create ();
      YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
      phy.SetChannel (yansChannel);
      phy.Set ("ShortGuardEnabled", BooleanValue (wifiConfig.shortGuard));
      phy.Set ("ChannelWidth", UintegerValue (wifiConfig.channelWidth));
      if (!wifiConfig.errorModel.empty ())
        {
          phy.Set ("ErrorRateModel", StringValue (wifiConfig.errorModel));
        }
      WifiHelper wifi;
      wifi.SetStandard (WIFI_STANDARD_80211n);
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                    "DataMode", StringValue ("HtMcs" + std::to_string (wifiConfig.mcs)),
                                    "ControlMode", StringValue ("HtMcs0"));
      WifiMacHelper mac;
      mac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));
      wifiDevs.Add (wifi.Install (phy, mac, wifiNodes.Get (0))); // STA
      mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
      wifiDevs.Add (wifi.Install (phy, mac, wifiNodes.Get (1))); // AP
      staPhy = wifiDevs.Get (0)->GetObject<WifiNetDevice> ()->GetPhy ();
    }

  // Non-Wi-Fi interferer (Waveform Generator)
  Ptr<SpectrumChannel> ifaceChannel;
  if (wifiConfig.wifiType == "spectrum")
    {
      ifaceChannel = spectrumChannel;
    }
  else
    {
      ifaceChannel = yansChannel->GetObject<YansWifiChannel> ();
    }
  WaveformGeneratorHelper waveform;
  waveform.SetChannel (ifaceChannel);
  waveform.Set ("Period", TimeValue (Seconds (0.0005)));
  waveform.Set ("DutyCycle", DoubleValue (0.5));
  waveform.Set ("Amplitude", DoubleValue (pow (10.0, waveformPowerDbm / 20.0) * 0.001)); // mV
  waveform.Install (interfererNode.Get (0));

  // Internet stack
  InternetStackHelper internet;
  internet.Install (wifiNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (wifiDevs);

  // Application setup
  uint16_t port = 9;
  ApplicationContainer appSink, appServer;
  if (trafficType == "udp")
    {
      UdpServerHelper udpServer (port);
      appSink = udpServer.Install (wifiNodes.Get (1)); // AP
      appSink.Start (Seconds (0.0));
      appSink.Stop (Seconds (simulationTime));

      UdpClientHelper udpClient (interfaces.GetAddress (1), port);
      udpClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
      udpClient.SetAttribute ("Interval", TimeValue (MilliSeconds (1)));
      udpClient.SetAttribute ("PacketSize", UintegerValue (1472));
      appServer = udpClient.Install (wifiNodes.Get (0)); // STA
    }
  else // tcp
    {
      uint32_t pktSize = 1448;
      PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory",
                                   InetSocketAddress (Ipv4Address::GetAny (), port));
      appSink = sinkHelper.Install (wifiNodes.Get (1)); // AP
      appSink.Start (Seconds (0.0));
      appSink.Stop (Seconds (simulationTime));

      OnOffHelper clientHelper ("ns3::TcpSocketFactory",
                                    InetSocketAddress (interfaces.GetAddress (1), port));
      clientHelper.SetAttribute ("DataRate", DataRateValue ("100Mbps"));
      clientHelper.SetAttribute ("PacketSize", UintegerValue (pktSize));
      clientHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
      clientHelper.SetAttribute ("StopTime", TimeValue (Seconds (simulationTime)));
      appServer = clientHelper.Install (wifiNodes.Get (0)); // STA
    }
  appServer.Start (Seconds (1.0));
  appServer.Stop (Seconds (simulationTime));

  // Start interferer
  interfererNode.Get (0)->GetApplication (0)->SetStartTime (Seconds (0.0));
  interfererNode.Get (0)->GetApplication (0)->SetStopTime (Seconds (simulationTime));

  // Enable PCAP
  if (enablePcap)
    {
      if (wifiConfig.wifiType == "spectrum")
        {
          SpectrumWifiPhyHelper spectrumPhy = SpectrumWifiPhyHelper::Default ();
          spectrumPhy.EnablePcap ("sta", wifiDevs.Get (0));
          spectrumPhy.EnablePcap ("ap", wifiDevs.Get (1));
        }
      else
        {
          YansWifiPhyHelper yansPhy = YansWifiPhyHelper::Default ();
          yansPhy.EnablePcap ("sta", wifiDevs.Get (0));
          yansPhy.EnablePcap ("ap", wifiDevs.Get (1));
        }
    }

  Simulator::Schedule (Seconds (1.0), &PhyStats, staPhy);
  Ptr<PacketSink> pktSink = DynamicCast<PacketSink> (appSink.Get (0));
  Simulator::Schedule (Seconds (1.0), &ThroughputMonitor, pktSink, 1.0);

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}