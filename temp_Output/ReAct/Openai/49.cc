#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/config-store-module.h"
#include <fstream>
#include <iostream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wifi80211nWithInterferer");

struct SimulationParams
{
  double simTime = 10.0;
  double distance = 5.0;
  uint32_t mcsIndex = 0;
  uint32_t channelWidth = 20;
  uint32_t guardInterval = 0; // 0: long, 1: short
  double waveformPower = 0.1; // in Watts
  std::string wifiType = "yans"; // "yans" or "spectrum"
  std::string errorModel = "none";
  bool useUdp = true;
  bool pcap = false;
  std::string phyMode = "HtMcs0";
};

void
RxCallback (
            std::string context,
            Ptr<const Packet> packet,
            uint16_t channelFreqMhz,
            WifiTxVector txVector,
            MpduInfo aMpdu,
            SignalNoiseDbm snr)
{
  static double rxBytes = 0;
  rxBytes += packet->GetSize();
}

void
RssiSnCallback (
    std::string context,
    double oldRss,
    double newRss)
{
  NS_UNUSED (context);
  NS_UNUSED (oldRss);
  NS_UNUSED (newRss);
}

void
PhyMonitorTrace (
    Ptr<OutputStreamWrapper> stream,
    Ptr<const Packet> packet,
    double snr,
    WifiMode mode,
    enum WifiPreamble preamble)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t"
                        << packet->GetSize () << "\t"
                        << snr << "\t"
                        << mode.GetUniqueName () << std::endl;
}

int main (int argc, char *argv[])
{
  SimulationParams params;
  CommandLine cmd;
  cmd.AddValue ("simTime", "Simulation time in seconds", params.simTime);
  cmd.AddValue ("distance", "Distance between Wi-Fi STA and AP (meters)", params.distance);
  cmd.AddValue ("mcsIndex", "MCS index (0-7 for 20 MHz/1 stream 11n)", params.mcsIndex);
  cmd.AddValue ("channelWidth", "Wi-Fi channel width (20 or 40 MHz)", params.channelWidth);
  cmd.AddValue ("guardInterval", "Guard interval: 0=Long, 1=Short", params.guardInterval);
  cmd.AddValue ("waveformPower", "Non-WiFi interferer power in Watts", params.waveformPower);
  cmd.AddValue ("wifiType", "Type of Wi-Fi channel: 'spectrum' or 'yans'", params.wifiType);
  cmd.AddValue ("errorModel", "Error model: none/corrupted", params.errorModel);
  cmd.AddValue ("useUdp", "Use UDP traffic instead of TCP", params.useUdp);
  cmd.AddValue ("pcap", "Enable PCAP tracing", params.pcap);
  cmd.Parse (argc, argv);

  std::string phyMode = "HtMcs" + std::to_string(params.mcsIndex);
  params.phyMode = phyMode;

  // WiFi network: 1 AP, 1 STA
  NodeContainer wifiStaNode;
  wifiStaNode.Create (1);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 1.0));
  positionAlloc->Add (Vector (params.distance, 0.0, 1.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNode);
  mobility.Install (wifiApNode);

  // Wi-Fi Channel
  Ptr<YansWifiChannel> yansChannel;
  Ptr<WifiSpectrumChannel> spectrumChannel;
  Ptr<WifiChannel> wifiChannel;

  if (params.wifiType == "spectrum")
    {
      spectrumChannel = CreateObject<WifiSpectrumChannel> ();
      Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel> ();
      spectrumChannel->SetPropagationDelayModel (delayModel);
      Ptr<LogDistancePropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel> ();
      spectrumChannel->AddPropagationLossModel (lossModel);
      wifiChannel = spectrumChannel;
    }
  else
    {
      yansChannel = CreateObject<YansWifiChannel> ();
      yansChannel->SetPropagationDelayModel (CreateObject<ConstantSpeedPropagationDelayModel> ());
      yansChannel->AddPropagationLossModel (CreateObject<LogDistancePropagationLossModel> ());
      wifiChannel = yansChannel;
    }

  // WiFi PHY and MAC
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_2_4GHZ);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi");

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  if (params.wifiType == "spectrum")
    {
      SpectrumWifiPhyHelper spectrumPhy = SpectrumWifiPhyHelper::Default ();
      spectrumPhy.SetChannel (spectrumChannel);
      spectrumPhy.Set ("ShortGuardEnabled", BooleanValue (params.guardInterval != 0));
      spectrumPhy.Set ("ChannelWidth", UintegerValue (params.channelWidth));
      phy = YansWifiPhyHelper (spectrumPhy);
    }
  else
    {
      phy.SetChannel (yansChannel);
      phy.Set ("ShortGuardEnabled", BooleanValue (params.guardInterval != 0));
      phy.Set ("ChannelWidth", UintegerValue (params.channelWidth));
    }
  phy.Set ("RxNoiseFigure", DoubleValue (7.0));
  phy.Set ("TxPowerStart", DoubleValue (20.0));
  phy.Set ("TxPowerEnd", DoubleValue (20.0));
  phy.Set ("EnergyDetectionThreshold", DoubleValue (-82.0));
  phy.Set ("CcaMode1Threshold", DoubleValue (-82.0));
  phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue (params.phyMode),
                               "ControlMode", StringValue (params.phyMode));

  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevice = wifi.Install (phy, mac, wifiStaNode);

  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiApNode);

  // Error Model
  if (params.errorModel == "corrupted")
    {
      Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
      em->SetAttribute ("ErrorRate", DoubleValue (0.0001));
      staDevice.Get (0)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
    }

  // Internet Stack
  InternetStackHelper stack;
  stack.Install (wifiStaNode);
  stack.Install (wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staIf = address.Assign (staDevice);
  Ipv4InterfaceContainer apIf = address.Assign (apDevice);

  // Non-Wi-Fi Interferer Node
  NodeContainer interfererNode;
  interfererNode.Create (1);
  Ptr<MobilityModel> interfererMob = CreateObject<ConstantPositionMobilityModel> ();
  interfererMob->SetPosition (Vector (params.distance/2, 1.0, 1.0));
  interfererNode.Get (0)->AggregateObject (interfererMob);

  Ptr<WaveformGenerator> waveformGen = CreateObject<WaveformGenerator> ();
  waveformGen->SetAttribute ("Period", TimeValue (Seconds (0.0001)));
  waveformGen->SetAttribute ("DutyCycle", DoubleValue (0.5));
  waveformGen->SetAttribute ("Amplitude", DoubleValue (sqrt (params.waveformPower * 50.0)));
  waveformGen->SetChannel (wifiChannel);

  if (params.wifiType == "spectrum")
    {
      waveformGen->SetAttribute ("Frequency", DoubleValue (2.437e9));
    }
  else
    {
      waveformGen->SetAttribute ("Frequency", DoubleValue (2.437e9));
    }
  interfererNode.Get (0)->AddDevice (waveformGen);

  // Applications
  uint16_t sinkPort = 9;
  Address sinkAddress (InetSocketAddress (staIf.GetAddress (0), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  if (params.useUdp)
    packetSinkHelper.SetProtocol (TypeId::LookupByName ("ns3::UdpSocketFactory"));
  ApplicationContainer sinkApps = packetSinkHelper.Install (wifiStaNode.Get (0));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (params.simTime+1));

  OnOffHelper onoff = params.useUdp
    ? OnOffHelper ("ns3::UdpSocketFactory", sinkAddress)
    : OnOffHelper ("ns3::TcpSocketFactory", sinkAddress);

  onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  onoff.SetAttribute ("PacketSize", UintegerValue (1472));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (params.simTime)));

  ApplicationContainer apps = onoff.Install (wifiApNode.Get (0));

  // Packet tracing, PCAP
  if (params.pcap)
    {
      phy.EnablePcap ("ap", apDevice.Get (0));
      phy.EnablePcap ("sta", staDevice.Get (0));
    }

  AsciiTraceHelper ascii;
  phy.EnableAsciiAll (ascii.CreateFileStream ("wifi.log"));

  // PHY SNR/rate tracing
  std::ofstream snrLog ("phy-monitor.log", std::ios_base::trunc);

  // Connect monitor to STA MAC/PHY
  Config::ConnectWithoutContext (
      "/NodeList/0/DeviceList/0/$ns3::WifiNetDevice/Phy/MonitorSnifferRx",
      MakeBoundCallback (&PhyMonitorTrace, Create<OutputStreamWrapper> (&snrLog)));

  // Connect RSSI
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferRx", MakeCallback (&RxCallback));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/RxRssi", MakeCallback (&RssiSnCallback));

  // FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (params.simTime+1));
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  uint64_t totalRx = 0;
  uint32_t nFlows = 0;

  std::cout << std::fixed << std::setprecision (2);
  for (auto it = stats.begin (); it != stats.end (); ++it)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (it->first);
      if (t.destinationPort == sinkPort)
        {
          nFlows++;
          double duration = it->second.timeLastRxPacket.GetSeconds () - it->second.timeFirstTxPacket.GetSeconds ();
          double throughput = it->second.rxBytes * 8.0 / (1e6 * duration);
          totalRx += it->second.rxBytes;
          std::cout << "Flow " << it->first
                    << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx " << it->second.txPackets << " pkts (" << it->second.txBytes << " bytes)\n";
          std::cout << "  Rx " << it->second.rxPackets << " pkts (" << it->second.rxBytes << " bytes)\n";
          std::cout << "  Throughput: " << throughput << " Mbps\n";
        }
    }

  std::cout << "======== Simulation Results ========\n";
  std::cout << "Simulation Time: " << params.simTime << " s\n";
  std::cout << "STA-AP Distance: " << params.distance << " m\n";
  std::cout << "MCS Index: " << params.mcsIndex << "\n";
  std::cout << "Channel Width: " << params.channelWidth << " MHz\n";
  std::cout << "Guard Interval: " << (params.guardInterval ? "Short" : "Long") << "\n";
  std::cout << "Waveform (Interferer) Power: " << params.waveformPower << " W\n";
  std::cout << "Traffic Type: " << (params.useUdp ? "UDP" : "TCP") << "\n";
  std::cout << "Total Throughput: " << totalRx * 8.0 / (1e6 * params.simTime) << " Mbps\n";

  snrLog.close ();
  Simulator::Destroy ();
  return 0;
}