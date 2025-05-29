#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

void
LogRssiSnr(Ptr<OutputStreamWrapper> stream, std::string context, Ptr<const Packet> pkt, uint8_t sig, WifiTxVector tx, double snr, WifiMode mode)
{
  *stream->GetStream() << Simulator::Now().GetSeconds() << ","
                       << sig << ","
                       << snr << ","
                       << mode.GetUniqueName() << std::endl;
}

void
ReportThroughput(Ptr<FlowMonitor> monitor, Ptr<Ipv4FlowClassifier> classifier, double simulationTime)
{
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (auto const & flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
      if (t.destinationPort == 5001 || t.destinationPort == 9)
        {
          double throughput = flow.second.rxBytes * 8.0 / (simulationTime * 1e6); // Mbps
          std::cout << "Flow " << flow.first
                    << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Throughput: " << throughput << " Mbps\n";
        }
    }
}

int main(int argc, char *argv[])
{
  bool useSpectrumPhy = true;
  double simulationTime = 10.0;
  double distance = 10.0;
  std::string phyMode = "HtMcs7";
  std::string appType = "udp";
  uint32_t mcs = 7;
  uint32_t channelWidth = 20;
  uint32_t guardIntervalNs = 800;
  bool enablePcap = false;

  CommandLine cmd;
  cmd.AddValue ("UseSpectrumPhy", "Use SpectrumWifiPhy (else YansWifiPhy)", useSpectrumPhy);
  cmd.AddValue ("SimTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("Distance", "Distance between STA and AP", distance);
  cmd.AddValue ("Mcs", "MCS index (0-7 for 802.11n single stream)", mcs);
  cmd.AddValue ("ChannelWidth", "Channel width (20/40)", channelWidth);
  cmd.AddValue ("ShortGuardInterval", "Guard interval (400 or 800 ns)", guardIntervalNs);
  cmd.AddValue ("AppType", "Traffic type: udp or tcp", appType);
  cmd.AddValue ("EnablePcap", "Enable PCAP tracing", enablePcap);
  cmd.Parse(argc, argv);

  // Prepare Output File for Signal/Noise/SNR logging
  std::ostringstream oss;
  oss << "wifi_signalnoise_mcs" << mcs << "_cw" << channelWidth
      << "_sgi" << guardIntervalNs << ".csv";
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream(oss.str());
  *stream->GetStream() << "time,signal,noise,mode\n";

  NodeContainer wifiStaNode;
  wifiStaNode.Create(1);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper yansChannel = YansWifiChannelHelper::Default();
  Ptr<YansWifiChannel> yans = yansChannel.Create();

  Ptr<WifiChannel> channel;
  WifiPhyHelper wifiPhy;
  SpectrumWifiPhyHelper spectrumPhy;
  if (useSpectrumPhy)
    {
      spectrumPhy = SpectrumWifiPhyHelper::Default();
      spectrumPhy.SetChannel(CreateObject<MultiModelSpectrumChannel>());
      channel = spectrumPhy.GetChannel();
    }
  else
    {
      wifiPhy = WifiPhyHelper::Default();
      wifiPhy.SetChannel(yans);
      channel = wifiPhy.GetChannel();
    }

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

  std::ostringstream mcsMode;
  mcsMode << "HtMcs" << mcs;
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                              "DataMode", StringValue(mcsMode.str()),
                              "ControlMode", StringValue("HtMcs0"));

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid("ns3-80211n");

  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices;
  if (useSpectrumPhy)
    {
      staDevices = wifi.Install(spectrumPhy, wifiMac, wifiStaNode);
    }
  else
    {
      staDevices = wifi.Install(wifiPhy, wifiMac, wifiStaNode);
    }

  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevices;
  if (useSpectrumPhy)
    {
      apDevices = wifi.Install(spectrumPhy, wifiMac, wifiApNode);
    }
  else
    {
      apDevices = wifi.Install(wifiPhy, wifiMac, wifiApNode);
    }

  // PHY configuration
  if (useSpectrumPhy)
    {
      spectrumPhy.Set("ChannelWidth", UintegerValue(channelWidth));
      spectrumPhy.Set("ShortGuardEnabled", BooleanValue(guardIntervalNs == 400));
    }
  else
    {
      wifiPhy.Set("ChannelWidth", UintegerValue(channelWidth));
      wifiPhy.Set("ShortGuardEnabled", BooleanValue(guardIntervalNs == 400));
    }

  // Channel configuration
  yansChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  yansChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 1.0));                      // AP
  positionAlloc->Add(Vector(distance, 0.0, 1.0));                  // STA
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiApNode);
  mobility.Install(wifiStaNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer staInterface;
  Ipv4InterfaceContainer apInterface;
  staInterface = address.Assign(staDevices);
  address.NewNetwork();
  apInterface = address.Assign(apDevices);

  // Application setup
  uint16_t port = (appType == "tcp") ? 5001 : 9;

  ApplicationContainer serverApp;
  if (appType == "tcp")
    {
      PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress(staInterface.GetAddress(0), port));
      serverApp = sink.Install(wifiStaNode.Get(0));
    }
  else
    {
      UdpServerHelper server(port);
      serverApp = server.Install(wifiStaNode.Get(0));
    }
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(simulationTime + 1));

  ApplicationContainer clientApp;
  if (appType == "tcp")
    {
      OnOffHelper client ("ns3::TcpSocketFactory", InetSocketAddress(staInterface.GetAddress(0), port));
      client.SetAttribute("DataRate", StringValue("200Mbps"));
      client.SetAttribute("PacketSize", UintegerValue(1500));
      client.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
      client.SetAttribute("StopTime", TimeValue(Seconds(simulationTime)));
      clientApp.Add(client.Install(wifiApNode.Get(0)));
    }
  else
    {
      UdpClientHelper client(staInterface.GetAddress(0), port);
      client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
      client.SetAttribute("Interval", TimeValue(Time("0.00001"))); // ~100 Mbps
      client.SetAttribute("PacketSize", UintegerValue(1472));
      clientApp = client.Install(wifiApNode.Get(0));
      clientApp.Start(Seconds(1.0));
      clientApp.Stop(Seconds(simulationTime));
    }

  // PCAP tracing
  if (enablePcap)
    {
      if (useSpectrumPhy)
        {
          spectrumPhy.EnablePcapAll("wifi_spectrum_phy", false);
        }
      else
        {
          wifiPhy.EnablePcapAll("wifi_yans_phy", false);
        }
    }

  // Trace for RSSI/SNR
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferRx", MakeBoundCallback(&LogRssiSnr, stream));

  // Flow Monitor for throughput
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simulationTime + 1));

  Simulator::Run();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  ReportThroughput(monitor, classifier, simulationTime);

  Simulator::Destroy();

  return 0;
}