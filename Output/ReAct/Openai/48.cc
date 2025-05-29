#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/propagation-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiPhyMcsExperiment");

double g_signalDbm = 0;
double g_noiseDbm = 0;
double g_snrDb = 0;

void
PhyRxTrace (Ptr<const Packet> packet, double snr, WifiMode mode, WifiPreamble preamble)
{
  NS_UNUSED(packet);
  NS_UNUSED(mode);
  NS_UNUSED(preamble);
  g_snrDb = 10 * std::log10 (snr);
}

void
MonitorSnr (Ptr<NetDevice> dev)
{
  Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice> (dev);
  Ptr<WifiPhy> phy = wifiDev->GetPhy ();
  WifiPhyStatistics stats = phy->GetPhyStatistics ();

  g_signalDbm = stats.lastSignalDbm;
  g_noiseDbm = stats.lastNoiseDbm;
}

int main (int argc, char *argv[])
{
  double simTime = 10.0;
  double distance = 10.0;
  std::string phyModel = "Yans";
  std::string trafficType = "UDP";
  bool enablePcap = false;
  uint32_t mcsStart = 0;
  uint32_t mcsEnd = 7;
  uint32_t channelWidthArr[] = {20, 40};
  bool shortGuardIntervalArr[] = {false,true};
  uint32_t channelWidthCount = 2;
  uint32_t sgiCount = 2;

  CommandLine cmd;
  cmd.AddValue ("simTime", "Duration of simulation in seconds", simTime);
  cmd.AddValue ("distance", "Distance between nodes (meters)", distance);
  cmd.AddValue ("phyModel", "Select 'Yans' or 'Spectrum'", phyModel);
  cmd.AddValue ("trafficType", "Select UDP or TCP", trafficType);
  cmd.AddValue ("enablePcap", "Enable pcap capture", enablePcap);
  cmd.AddValue ("mcsStart", "First MCS index", mcsStart);
  cmd.AddValue ("mcsEnd", "Last MCS index", mcsEnd);
  cmd.Parse (argc,argv);

  // Topology: 2 nodes (STA, AP)
  NodeContainer wifiStaNode, wifiApNode;
  wifiStaNode.Create (1);
  wifiApNode.Create (1);

  // Mobility: fix distance
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (distance, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNode);
  mobility.Install (wifiApNode);

  // Channel and PHY setup
  Ptr<WifiHelper> wifi = CreateObject<WifiHelper> ();
  wifi->SetStandard (WIFI_STANDARD_80211n_5GHZ);

  Ssid ssid = Ssid ("ns3-80211n");

  YansWifiChannelHelper yansChannel = YansWifiChannelHelper::Default ();
  SpectrumWifiPhyHelper spectrumPhy = SpectrumWifiPhyHelper::Default ();
  YansWifiPhyHelper yansPhy = YansWifiPhyHelper::Default ();

  WifiMacHelper mac;

  // Logging
  std::cout << "Simulation time: " << simTime
            << "s, node distance: " << distance
            << "m, PHY Model: " << phyModel
            << ", Traffic: " << trafficType << std::endl;
  std::cout << "MCS,ChWidth,SGI,Throughput(Mbps),Signal(dBm),Noise(dBm),SNR(dB)" << std::endl;

  for (uint32_t mcsIdx = mcsStart; mcsIdx <= mcsEnd; ++mcsIdx)
  for (uint32_t cwIdx=0; cwIdx<channelWidthCount; ++cwIdx)
  for (uint32_t sgiIdx=0; sgiIdx<sgiCount; ++sgiIdx)
    {
      double throughput = 0;
      g_signalDbm = 0;
      g_noiseDbm = 0;
      g_snrDb = 0;

      // Clean up between runs
      WifiHelper wifi2 = *wifi;
      Ptr<WifiPhy> apPhy, staPhy;

      // Select PHY
      NetDeviceContainer staDevice, apDevice;

      if (phyModel == "Spectrum" || phyModel == "spectrum")
        {
          // Spectrum-based 802.11n
          spectrumPhy.SetChannel (SpectrumChannelHelper::Default ().Create ());
          spectrumPhy.SetChannelWidth (channelWidthArr[cwIdx]);
          spectrumPhy.Set ("ShortGuardEnabled", BooleanValue (shortGuardIntervalArr[sgiIdx]));
          spectrumPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
          wifi2.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue ("HtMcs" + std::to_string (mcsIdx)),
                                      "ControlMode", StringValue ("HtMcs0"));
          mac.SetType ("ns3::StaWifiMac",
                       "Ssid", SsidValue (ssid),
                       "ActiveProbing", BooleanValue (false));
          staDevice = wifi2.Install (spectrumPhy, mac, wifiStaNode);

          mac.SetType ("ns3::ApWifiMac",
                       "Ssid", SsidValue (ssid));
          apDevice = wifi2.Install (spectrumPhy, mac, wifiApNode);

          apPhy = DynamicCast<WifiNetDevice> (apDevice.Get (0))->GetPhy ();
          staPhy = DynamicCast<WifiNetDevice> (staDevice.Get (0))->GetPhy ();
        }
      else
        {
          // Yans-based 802.11n
          Ptr<YansWifiChannel> ch = yansChannel.Create ();
          yansPhy.SetChannel (ch);
          yansPhy.SetChannelWidth (channelWidthArr[cwIdx]);
          yansPhy.Set ("ShortGuardEnabled", BooleanValue (shortGuardIntervalArr[sgiIdx]));
          yansPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

          wifi2.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue ("HtMcs" + std::to_string (mcsIdx)),
                                      "ControlMode", StringValue ("HtMcs0"));

          mac.SetType ("ns3::StaWifiMac",
                       "Ssid", SsidValue (ssid),
                       "ActiveProbing", BooleanValue(false));
          staDevice = wifi2.Install (yansPhy, mac, wifiStaNode);

          mac.SetType ("ns3::ApWifiMac",
                       "Ssid", SsidValue (ssid));
          apDevice = wifi2.Install (yansPhy, mac, wifiApNode);

          apPhy = DynamicCast<WifiNetDevice> (apDevice.Get (0))->GetPhy ();
          staPhy = DynamicCast<WifiNetDevice> (staDevice.Get (0))->GetPhy ();
        }

      // Install internet stack
      InternetStackHelper stack;
      stack.Install (wifiApNode);
      stack.Install (wifiStaNode);

      Ipv4AddressHelper address;
      address.SetBase ("10.1.1.0", "255.255.255.0");
      Ipv4InterfaceContainer staIf, apIf;
      staIf = address.Assign (staDevice);
      address.NewNetwork ();
      apIf = address.Assign (apDevice);

      // Application: BulkSend (TCP) or UDP
      uint16_t port = 5000;
      ApplicationContainer sourceApp, sinkApp;

      if (trafficType == "TCP" || trafficType == "tcp")
        {
          // TCP - use BulkSendApplication
          BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (staIf.GetAddress (0), port));
          source.SetAttribute ("MaxBytes", UintegerValue (0));
          source.SetAttribute ("SendSize", UintegerValue (1400));
          sourceApp = source.Install (wifiApNode.Get (0));

          PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
          sinkApp = sink.Install (wifiStaNode.Get (0));
        }
      else
        {
          // UDP - use OnOffApplication
          OnOffHelper source ("ns3::UdpSocketFactory", InetSocketAddress (staIf.GetAddress (0), port));
          source.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
          source.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
          source.SetAttribute ("DataRate", DataRateValue (DataRate ("400Mbps"))); // high enough
          source.SetAttribute ("PacketSize", UintegerValue (1400));
          sourceApp = source.Install (wifiApNode.Get (0));

          PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
          sinkApp = sink.Install (wifiStaNode.Get (0));
        }

      sourceApp.Start (Seconds (1.0));
      sourceApp.Stop (Seconds (simTime+1));
      sinkApp.Start (Seconds (0.0));
      sinkApp.Stop (Seconds (simTime+2));

      // PCAP capture
      if (enablePcap)
        {
          if (phyModel == "Spectrum" || phyModel == "spectrum")
            {
              spectrumPhy.EnablePcap ("sta-" + std::to_string(mcsIdx) + "-" + std::to_string(channelWidthArr[cwIdx]) + (shortGuardIntervalArr[sgiIdx]?"-sgi":"-lg"), staDevice.Get (0), true, true);
              spectrumPhy.EnablePcap ("ap-" + std::to_string(mcsIdx) + "-" + std::to_string(channelWidthArr[cwIdx]) + (shortGuardIntervalArr[sgiIdx]?"-sgi":"-lg"), apDevice.Get (0), true, true);
            }
          else
            {
              yansPhy.EnablePcap ("sta-" + std::to_string(mcsIdx) + "-" + std::to_string(channelWidthArr[cwIdx]) + (shortGuardIntervalArr[sgiIdx]?"-sgi":"-lg"), staDevice.Get (0), true, true);
              yansPhy.EnablePcap ("ap-" + std::to_string(mcsIdx) + "-" + std::to_string(channelWidthArr[cwIdx]) + (shortGuardIntervalArr[sgiIdx]?"-sgi":"-lg"), apDevice.Get (0), true, true);
            }
        }

      // Trace SNR and PHY stats
      Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferRx", MakeCallback (&PhyRxTrace));

      // Flow monitor
      FlowMonitorHelper flowmon;
      Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

      Simulator::Schedule (Seconds(simTime), &MonitorSnr, staDevice.Get(0));

      Simulator::Stop (Seconds (simTime+2));
      Simulator::Run ();

      // Get throughput
      monitor->CheckForLostPackets ();
      FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
      for (auto itr = stats.begin (); itr != stats.end (); ++itr)
        {
          double bytes = itr->second.rxBytes * 8.0;
          throughput = bytes / simTime / 1e6; // Mbps
        }

      // Output stats
      std::cout << mcsIdx << ","
                << channelWidthArr[cwIdx] << ","
                << (shortGuardIntervalArr[sgiIdx] ? "SGI" : "LGI") << ","
                << throughput << ","
                << g_signalDbm << ","
                << g_noiseDbm << ","
                << g_snrDb << std::endl;

      Simulator::Destroy ();
    }

  return 0;
}