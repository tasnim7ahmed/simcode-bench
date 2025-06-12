#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/wifi-helper.h"
#include "ns3/config-store-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/error-model.h"
#include "ns3/spectrum-module.h"
#include "ns3/spectrum-analyzer-helper.h"
#include "ns3/spectrum-interference-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/pcap-file-wrapper.h"
#include "ns3/ssid.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiWithInterferenceExample");

void
RxStats (Ptr<const Packet> packet, double snr, WifiMode mode, enum WifiPreamble preamble, uint8_t nss, double rxPower, double noise)
{
  static uint32_t receivedPackets = 0;
  receivedPackets++;
  std::cout << Simulator::Now ().GetSeconds ()
            << "s RX Packet #" << receivedPackets
            << " Mode=" << mode
            << " NSS=" << (uint32_t)nss
            << " RxPower=" << rxPower << " dBm"
            << " Noise=" << noise << " dBm"
            << " SNR=" << snr << " dB" << std::endl;
}

void
ThroughputMonitor (Ptr<FlowMonitor> flowMonitor, FlowMonitorHelper *flowHelper)
{
  flowMonitor->CheckForLostPackets ();
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats ();
  for (auto& flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = flowHelper->GetClassifier ()->FindFlow (flow.first);
      double throughput = flow.second.rxBytes * 8.0 / (flow.second.timeLastRxPacket.GetSeconds () - flow.second.timeFirstTxPacket.GetSeconds ()) / 1e6;
      std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ") "
                << "Throughput: " << throughput << " Mbps" << std::endl;
    }
  Simulator::Schedule (Seconds (1.0), &ThroughputMonitor, flowMonitor, flowHelper);
}

int main (int argc, char *argv[])
{
  double simulationTime = 10.0;
  double distance = 5.0;
  uint32_t mcsIndex = 0;
  uint32_t channelWidthIndex = 1; // 0:20MHz, 1:40MHz, 2:80MHz, etc.
  uint32_t giIndex = 0; // 0:normal GI, 1:short GI
  double waveformPower = 10.0; // dBm
  std::string wifiType = "spectrum"; // "spectrum" or "yans"
  std::string channelModel = "Yans";
  std::string errorModel = "none"; // "none", "nist", "yans"
  bool enablePcap = false;
  std::string trafficType = "TCP"; // "TCP" or "UDP"
  std::string modulation = "HtMcs0";
  uint32_t udpPacketSize = 1472;
  uint32_t udpPacketRate = 1000000;

  CommandLine cmd;
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("distance", "Distance between Wi-Fi STA and AP (meters)", distance);
  cmd.AddValue ("mcsIndex", "HT MCS index (0-7 single-stream)", mcsIndex);
  cmd.AddValue ("channelWidthIndex", "Channel width index: 0=20MHz,1=40MHz,2=80MHz,3=160MHz", channelWidthIndex);
  cmd.AddValue ("giIndex", "Guard Interval index: 0=normal(800ns),1=short(400ns)", giIndex);
  cmd.AddValue ("waveformPower", "Interferer waveform power in dBm", waveformPower);
  cmd.AddValue ("wifiType", "Wi-Fi channel type: spectrum or yans", wifiType);
  cmd.AddValue ("errorModel", "Error model: none, nist, yans", errorModel);
  cmd.AddValue ("enablePcap", "Enable/disable PCAP tracing", enablePcap);
  cmd.AddValue ("trafficType", "Traffic type: TCP or UDP", trafficType);
  cmd.Parse (argc, argv);

  if (mcsIndex >= 8) mcsIndex = 0; // limit to 1SS

  std::vector<std::string> mcsName = { "HtMcs0", "HtMcs1", "HtMcs2", "HtMcs3", "HtMcs4", "HtMcs5", "HtMcs6", "HtMcs7" };
  std::vector<uint16_t> cwValues = {20, 40, 80, 160};

  modulation = mcsName[mcsIndex];
  uint16_t channelWidth = cwValues[channelWidthIndex % cwValues.size ()];
  bool shortGi = (giIndex == 1);

  NodeContainer wifiStaNode, wifiApNode, interfererNode;
  wifiStaNode.Create (1);
  wifiApNode.Create (1);
  interfererNode.Create (1);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 1.5)); // AP
  positionAlloc->Add (Vector (distance, 0.0, 1.5)); // STA
  positionAlloc->Add (Vector (distance/2.0, 0.5, 1.5)); // Interferer
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);
  mobility.Install (wifiStaNode);
  mobility.Install (interfererNode);

  NetDeviceContainer staDevice, apDevice;

  WifiMacHelper mac;
  Ssid ssid = Ssid ("wifi-interference-ssid");

  if (wifiType == "spectrum")
    {
      SpectrumWifiPhyHelper spectrumPhy = SpectrumWifiPhyHelper::Default ();
      Ptr<MultiModelSpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel> ();
      Ptr<LogDistancePropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel> ();
      spectrumChannel->AddPropagationLossModel (lossModel);
      spectrumChannel->SetPropagationDelayModel (CreateObject<ConstantSpeedPropagationDelayModel> ());
      spectrumPhy.SetChannel (spectrumChannel);
      spectrumPhy.SetErrorRateModel ("ns3::NistErrorRateModel");

      spectrumPhy.Set ("ChannelWidth", UintegerValue (channelWidth));
      spectrumPhy.Set ("ShortGuardEnabled", BooleanValue (shortGi));
      spectrumPhy.Set ("TxPowerStart", DoubleValue (20.0));
      spectrumPhy.Set ("TxPowerEnd", DoubleValue (20.0));
      spectrumPhy.Set ("RxNoiseFigure", DoubleValue (9.0));

      WifiHelper wifiHelper;
      wifiHelper.SetStandard (WIFI_STANDARD_80211n_5GHZ);
      wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                          "DataMode", StringValue (modulation),
                                          "ControlMode", StringValue ("HtMcs0"));
      mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid), "ActiveProbing", BooleanValue (false));
      staDevice = wifiHelper.Install (spectrumPhy, mac, wifiStaNode);
      mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
      apDevice = wifiHelper.Install (spectrumPhy, mac, wifiApNode);
    }
  else
    {
      YansWifiChannelHelper yansChannel = YansWifiChannelHelper::Default ();
      yansChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
      yansChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
      YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
      phy.SetChannel (yansChannel.Create ());
      phy.Set ("ChannelWidth", UintegerValue (channelWidth));
      phy.Set ("ShortGuardEnabled", BooleanValue (shortGi));
      phy.Set ("TxPowerStart", DoubleValue (20.0));
      phy.Set ("TxPowerEnd", DoubleValue (20.0));
      phy.Set ("RxNoiseFigure", DoubleValue (9.0));
      if (errorModel == "none")
        phy.SetErrorRateModel ("ns3::NoErrorRateModel");
      else if (errorModel == "nist")
        phy.SetErrorRateModel ("ns3::NistErrorRateModel");
      else if (errorModel == "yans")
        phy.SetErrorRateModel ("ns3::YansErrorRateModel");
      WifiHelper wifiHelper;
      wifiHelper.SetStandard (WIFI_STANDARD_80211n_5GHZ);
      wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                          "DataMode", StringValue (modulation),
                                          "ControlMode", StringValue ("HtMcs0"));
      mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid), "ActiveProbing", BooleanValue (false));
      staDevice = wifiHelper.Install (phy, mac, wifiStaNode);
      mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
      apDevice = wifiHelper.Install (phy, mac, wifiApNode);
    }

  if (enablePcap)
    {
      if (wifiType == "spectrum")
        {
          SpectrumWifiPhyHelper spectrumPhy = SpectrumWifiPhyHelper::Default ();
          spectrumPhy.EnablePcap ("sta-spectr", staDevice.Get (0));
          spectrumPhy.EnablePcap ("ap-spectr", apDevice.Get (0));
        }
      else
        {
          YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
          phy.EnablePcap ("sta-yans", staDevice.Get (0));
          phy.EnablePcap ("ap-yans", apDevice.Get (0));
        }
    }

  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer staNodeInterface, apNodeInterface;
  staNodeInterface = address.Assign (staDevice);
  apNodeInterface = address.Assign (apDevice);

  uint16_t port = 5001;
  ApplicationContainer serverApps, clientApps;
  if (trafficType == "TCP")
    {
      PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (staNodeInterface.GetAddress (0), port));
      serverApps = sinkHelper.Install (wifiStaNode.Get (0));
      serverApps.Start (Seconds (0.0));
      serverApps.Stop (Seconds (simulationTime + 1));
      OnOffHelper onoff ("ns3::TcpSocketFactory", InetSocketAddress (staNodeInterface.GetAddress (0), port));
      onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
      onoff.SetAttribute ("PacketSize", UintegerValue (1460));
      onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
      onoff.SetAttribute ("StopTime", TimeValue (Seconds (simulationTime)));
      onoff.SetAttribute ("MaxBytes", UintegerValue (0));
      clientApps = onoff.Install (wifiApNode.Get (0));
    }
  else // UDP
    {
      UdpServerHelper udpServer (port);
      serverApps = udpServer.Install (wifiStaNode.Get (0));
      serverApps.Start (Seconds (0.0));
      serverApps.Stop (Seconds (simulationTime + 1));
      UdpClientHelper udpClient (staNodeInterface.GetAddress (0), port);
      udpClient.SetAttribute ("MaxPackets", UintegerValue (0));
      udpClient.SetAttribute ("Interval", TimeValue (Seconds (1.0 * udpPacketSize * 8 / double (udpPacketRate))));
      udpClient.SetAttribute ("PacketSize", UintegerValue (udpPacketSize));
      clientApps = udpClient.Install (wifiApNode.Get (0));
      clientApps.Start (Seconds (1.0));
      clientApps.Stop (Seconds (simulationTime));
    }

  // Interferer waveform (non-Wi-Fi).
  Ptr<SpectrumChannel> spectrumChannel;
  if (wifiType == "spectrum")
    spectrumChannel = DynamicCast<SpectrumWifiPhy> (staDevice.Get (0)->GetObject<WifiNetDevice> ()->GetPhy ())->GetChannel ();
  
  Ptr<IsotropicAntennaModel> antenna = CreateObject<IsotropicAntennaModel> ();
  Ptr<ConstantSpectrumPropagationLossModel> constantLoss = CreateObject<ConstantSpectrumPropagationLossModel> ();
  if (wifiType == "spectrum" && spectrumChannel)
    {
      // Simple waveform generator
      Ptr<SpectrumValue> txPsd = Create <SpectrumValue> (DefaultSpectrumModel::Get2400MHz ());
      *txPsd = waveformPower/10.0;
      Ptr<WaveformGenerator> generator = CreateObject<WaveformGenerator> ();
      generator->SetTxPowerSpectralDensity (txPsd);
      generator->SetAntenna (antenna);
      generator->SetPosition (Vector (distance/2.0, 0.5, 1.5));
      spectrumChannel->AddTx (generator);
      generator->Start (Seconds (1.0));
      generator->Stop (Seconds (simulationTime));
    }

  // Tracing signal-related parameters
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferRx", MakeCallback (&RxStats));

  FlowMonitorHelper flowHelper;
  Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll ();

  Simulator::Schedule (Seconds (2.0), &ThroughputMonitor, flowMonitor, &flowHelper);

  Simulator::Stop (Seconds (simulationTime+1));
  Simulator::Run ();

  flowMonitor->SerializeToXmlFile ("wifi-interf-flowmon.xml", true, true);

  if (trafficType == "TCP")
    {
      uint64_t rxBytes = DynamicCast<PacketSink> (serverApps.Get (0))->GetTotalRx ();
      double throughput = rxBytes * 8.0 / simulationTime / 1e6;
      std::cout << "TCP Received Bytes: " << rxBytes << ", Throughput: " << throughput << " Mbps" << std::endl;
    }
  else
    {
      uint32_t rxPackets = DynamicCast<UdpServer> (serverApps.Get (0))->GetReceived ();
      std::cout << "UDP Received Packets: " << rxPackets << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}