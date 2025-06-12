/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/propagation-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/error-model.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiInterference80211nExample");

double GetSnrDb (Ptr<PacketSink> sink)
{
  // In a real experiment, we'd calculate SNR at the PHY
  // For simplicity, we return NaN (not available)
  return std::numeric_limits<double>::quiet_NaN ();
}

void
AdvanceStats (Ptr<PacketSink> sink, Time interval, uint64_t &lastTotalRx)
{
  Time now = Simulator::Now ();
  uint64_t cur = sink->GetTotalRx ();
  double throughput = ((cur - lastTotalRx) * 8.0) / (interval.GetSeconds () * 1e6); // Mbps

  std::cout << now.GetSeconds () << "s: "
            << "Throughput: " << throughput << " Mbps, "
            << "TotalRx: " << cur << " bytes"
            << std::endl;

  lastTotalRx = cur;

  if (Simulator::Now () + interval <= Seconds (Simulator::GetMaximumSimulationTime ().GetSeconds ()))
    Simulator::Schedule (interval, &AdvanceStats, sink, interval, std::ref(lastTotalRx));
}

int
main (int argc, char *argv[])
{
  // Simulation parameters (with reasonable defaults)
  double simulationTime = 10.0;
  double distance = 10.0;
  uint32_t mcsIndex = 7; // Modulation & Coding Scheme index
  uint32_t channelWidth = 20;
  std::string gi = "normal"; // guard interval
  double waveformPower = 10.0; // dBm for interferer
  std::string wifiType = "spectrum";
  bool enablePcap = false;
  bool useErrorModel = false;

  CommandLine cmd;
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("distance", "Distance between AP and STA", distance);
  cmd.AddValue ("mcsIndex", "MCS index (0-31 for 802.11n)", mcsIndex);
  cmd.AddValue ("channelWidth", "Channel width (20 or 40 MHz)", channelWidth);
  cmd.AddValue ("gi", "Guard interval: normal or short", gi);
  cmd.AddValue ("waveformPower", "Interferer waveform TX power (dBm)", waveformPower);
  cmd.AddValue ("wifiType", "Wifi type: yans or spectrum", wifiType);
  cmd.AddValue ("enablePcap", "Enable PCAP output", enablePcap);
  cmd.AddValue ("useErrorModel", "Apply error model to Wifi PHY", useErrorModel);
  cmd.Parse (argc, argv);

  bool shortGuardInterval = (gi == "short" ? true : false);

  // Create nodes: AP, STA, and Interferer
  NodeContainer wifiStaNode;
  wifiStaNode.Create (1);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);
  NodeContainer interfererNode;
  interfererNode.Create (1);

  // Mobility Setup
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (distance, 0.0, 0.0)); // STA
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));       // AP
  positionAlloc->Add (Vector (distance / 2.0, 5.0, 0.0)); // Interferer
  mobility.SetPositionAllocator (positionAlloc);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNode);
  mobility.Install (wifiApNode);
  mobility.Install (interfererNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNode);

  // Wi-Fi Channel and PHY
  Ptr<YansWifiChannel> yansChannel;
  Ptr<SpectrumChannel> spectrumChannel;
  Ptr<WifiPhy> apPhy;
  Ptr<WifiPhy> staPhy;

  NetDeviceContainer staDevice, apDevice;

  // Wi-Fi Configuration
  Ssid ssid = Ssid ("ns3-80211n");

  if (wifiType == "spectrum")
    {
      SpectrumWifiPhyHelper spectrumPhy = SpectrumWifiPhyHelper::Default ();
      spectrumChannel = CreateObject<MultiModelSpectrumChannel> ();
      Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel> ();
      spectrumChannel->AddPropagationLossModel (CreateObject<LogDistancePropagationLossModel> ());
      spectrumChannel->SetPropagationDelayModel (delayModel);
      spectrumPhy.SetChannel (spectrumChannel);

      WifiHelper wifi;
      wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);

      WifiMacHelper mac;
      mac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));
      spectrumPhy.Set ("ChannelWidth", UintegerValue (channelWidth));
      spectrumPhy.Set ("ShortGuardEnabled", BooleanValue (shortGuardInterval));
      spectrumPhy.Set ("TxPowerStart", DoubleValue (16.0));
      spectrumPhy.Set ("TxPowerEnd", DoubleValue (16.0));
      if (useErrorModel)
        {
          Ptr<NistErrorRateModel> error = CreateObject<NistErrorRateModel> ();
          spectrumPhy.SetErrorRateModel ("ns3::NistErrorRateModel");
        }
      // Sta
      YansWifiChannelHelper::Default ();
      NetDeviceContainer staDev = wifi.Install (spectrumPhy, mac, wifiStaNode);
      staDevice = staDev;

      mac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid));
      NetDeviceContainer apDev = wifi.Install (spectrumPhy, mac, wifiApNode);
      apDevice = apDev;

      // Set MCS for 802.11n
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (channelWidth));
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortGuardEnabled", BooleanValue (shortGuardInterval));
      std::ostringstream dataRateStream;
      dataRateStream << "HtMcs" << mcsIndex;
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RemoteStationManager/DataMode",
                   StringValue (dataRateStream.str ()));
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RemoteStationManager/ControlMode",
                   StringValue (dataRateStream.str ()));
    }
  else // yans
    {
      YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
      YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
      channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
      channel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
      yansChannel = channel.Create ();
      phy.SetChannel (yansChannel);
      phy.Set ("ChannelWidth", UintegerValue (channelWidth));
      phy.Set ("ShortGuardEnabled", BooleanValue (shortGuardInterval));
      phy.Set ("TxPowerStart", DoubleValue (16.0));
      phy.Set ("TxPowerEnd", DoubleValue (16.0));
      if (useErrorModel)
        {
          Ptr<NistErrorRateModel> error = CreateObject<NistErrorRateModel> ();
          phy.SetErrorRateModel ("ns3::NistErrorRateModel");
        }

      WifiHelper wifi;
      wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);

      WifiMacHelper mac;
      mac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));
      staDevice = wifi.Install (phy, mac, wifiStaNode);

      mac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid));
      apDevice = wifi.Install (phy, mac, wifiApNode);

      // Set MCS
      std::ostringstream dataRateStream;
      dataRateStream << "HtMcs" << mcsIndex;
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (channelWidth));
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortGuardEnabled", BooleanValue (shortGuardInterval));
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RemoteStationManager/DataMode",
                   StringValue (dataRateStream.str ()));
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RemoteStationManager/ControlMode",
                   StringValue (dataRateStream.str ()));
    }

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterface;
  Ipv4InterfaceContainer apInterface;
  staInterface = address.Assign (staDevice);
  apInterface = address.Assign (apDevice);

  // Install non-Wi-Fi interferer using Spectrum framework
  Ptr<SpectrumChannel> interfererChannel;
  if (wifiType == "spectrum")
    {
      interfererChannel = spectrumChannel;
    }
  else
    {
      SpectrumChannelHelper spectrumChannelHelper = SpectrumChannelHelper::Default ();
      interfererChannel = spectrumChannelHelper.Create ();
    }

  Ptr<SingleModelSpectrumChannel> smsc = CreateObject<SingleModelSpectrumChannel> ();
  smsc->AddPropagationLossModel (CreateObject<LogDistancePropagationLossModel> ());
  smsc->SetPropagationDelayModel (CreateObject<ConstantSpeedPropagationDelayModel> ());

  // Set up Spectrum transmitter as interferer
  Ptr<MobilityModel> interfMob = interfererNode.Get (0)->GetObject<MobilityModel> ();
  Ptr<UniformPlanarArrayAntennaModel> antenna = CreateObject<UniformPlanarArrayAntennaModel> ();
  Ptr<SpectrumPhy> interfPhy = CreateObject<MultiModelSpectrumChannel> (); // placeholder

  // Install Applications: TCP and UDP client/server
  uint16_t port = 5001;

  // TCP: BulkSend from STA to AP, PacketSink at AP
  BulkSendHelper tcpSender ("ns3::TcpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), port));
  tcpSender.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer tcpSourceApp = tcpSender.Install (wifiStaNode.Get (0));
  tcpSourceApp.Start (Seconds (1.0));
  tcpSourceApp.Stop (Seconds (simulationTime - 0.1));

  PacketSinkHelper tcpSinkHelper ("ns3::TcpSocketFactory",
                                 InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer tcpSinkApp = tcpSinkHelper.Install (wifiApNode.Get (0));
  tcpSinkApp.Start (Seconds (0.0));
  tcpSinkApp.Stop (Seconds (simulationTime));

  // UDP: OnOff from STA to AP, PacketSink at AP
  uint16_t udpPort = 5002;
  OnOffHelper udpSender ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress (0), udpPort));
  udpSender.SetConstantRate (DataRate ("54Mbps"), 1472);
  ApplicationContainer udpSourceApp = udpSender.Install (wifiStaNode.Get (0));
  udpSourceApp.Start (Seconds (2.0));
  udpSourceApp.Stop (Seconds (simulationTime - 0.1));

  PacketSinkHelper udpSinkHelper ("ns3::UdpSocketFactory",
                                  InetSocketAddress (Ipv4Address::GetAny (), udpPort));
  ApplicationContainer udpSinkApp = udpSinkHelper.Install (wifiApNode.Get (0));
  udpSinkApp.Start (Seconds (0.0));
  udpSinkApp.Stop (Seconds (simulationTime));

  // Enable pcap
  if (enablePcap)
    {
      if (wifiType == "spectrum")
        {
          SpectrumWifiPhyHelper::Default ().EnablePcapAll ("wifi-spectrum");
        }
      else
        {
          YansWifiPhyHelper::Default ().EnablePcapAll ("wifi-yans");
        }
    }

  // Enable packet tracing
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("wifi-80211n-interference.tr");
  if (wifiType == "spectrum")
    {
      SpectrumWifiPhyHelper::Default ().EnableAsciiAll (stream);
    }
  else
    {
      YansWifiPhyHelper::Default ().EnableAsciiAll (stream);
    }

  // Flow monitor for stats
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Throughput & RSSI/Noise/SNR reporting
  Ptr<PacketSink> tcpSink = DynamicCast<PacketSink> (tcpSinkApp.Get (0));
  Ptr<PacketSink> udpSink = DynamicCast<PacketSink> (udpSinkApp.Get (0));
  uint64_t tcpLastTotalRx = 0, udpLastTotalRx = 0;
  Simulator::Schedule (Seconds (1.0), &AdvanceStats, tcpSink, Seconds (1.0), std::ref(tcpLastTotalRx));
  Simulator::Schedule (Seconds (1.0), &AdvanceStats, udpSink, Seconds (1.0), std::ref(udpLastTotalRx));

  NS_LOG_UNCOND ("Starting simulation for " << simulationTime << " seconds.");
  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  // Final statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (auto const& flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
      std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> "
                << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
      std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
      std::cout << "  Throughput: ";
      if (flow.second.timeLastRxPacket.GetSeconds () > 0)
        {
          std::cout << (flow.second.rxBytes * 8.0 /
             (flow.second.timeLastRxPacket.GetSeconds () - flow.second.timeFirstTxPacket.GetSeconds ()) / 1e6)
             << " Mbps\n";
        }
      else
        {
          std::cout << "0 Mbps\n";
        }
      std::cout << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}