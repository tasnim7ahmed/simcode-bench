#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/energy-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSpectrumInterferenceExample");

void
RxPacketTrace(std::string context, Ptr<const Packet> packet, double snr, WifiTxVector txvector, SpectrumWifiPhyRxfailureReason reason)
{
  NS_UNUSED(packet);
  NS_UNUSED(txvector);
  NS_UNUSED(reason);
  std::cout << Simulator::Now().GetSeconds()
            << "s [" << context << "] "
            << "Received packet SNR=" << 10*log10(snr) << " dB\n";
}

void
PhyTxTrace(std::string context, Ptr<const Packet> packet, WifiTxVector txvector, double txPowerDbm, WifiPreamble preamble)
{
  NS_UNUSED(packet);
  NS_UNUSED(txvector);
  NS_UNUSED(preamble);
  std::cout << Simulator::Now().GetSeconds()
            << "s [" << context << "] "
            << "Transmitting packet, Tx Power = " << txPowerDbm << " dBm\n";
}

void
CalcThroughput(Ptr<PacketSink> sink, Time interval, double &lastTotalReceived, double &totalThroughput)
{
  double currentTotalReceived = sink->GetTotalRx();
  double throughput = (currentTotalReceived - lastTotalReceived) * 8.0 / interval.GetSeconds() / 1e6; // Mbps
  totalThroughput = throughput;
  std::cout << Simulator::Now().GetSeconds() << "s: Throughput: " << throughput << " Mbps" << std::endl;
  lastTotalReceived = currentTotalReceived;
  Simulator::Schedule(interval, &CalcThroughput, sink, interval, lastTotalReceived, totalThroughput);
}

double
GetRss(Ptr<WifiPhy> phy)
{
  // Dummy RSS value for demonstration. Should be obtained/averaged in a real environment.
  Ptr<NetDevice> dev = phy->GetDevice();
  return phy->GetRxSensitivity();
}

double
ComputeSnr(double signal, double noise)
{
  return 10 * std::log10(signal / noise);
}

int main(int argc, char *argv[])
{
  // Simulation parameters
  double simulationTime = 10.0;
  double distance = 5.0;
  uint32_t mcsIndex = 0;   // For HT MCS
  uint32_t channelWidth = 20; // MHz
  bool shortGuard = false;
  double waveformPower = 20.0; // dBm
  std::string wifiType = "spectrum"; // or "yans"
  std::string errorModel = "n"; // n: none, or string code for enabled
  bool pcapTracing = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("distance", "Distance between Wi-Fi nodes (meters)", distance);
  cmd.AddValue("mcsIndex", "MCS index (0-31 for 802.11n)", mcsIndex);
  cmd.AddValue("channelWidth", "Channel width in MHz (20 or 40)", channelWidth);
  cmd.AddValue("shortGuard", "Enable short guard interval", shortGuard);
  cmd.AddValue("waveformPower", "Power of interferer waveform (dBm)", waveformPower);
  cmd.AddValue("wifiType", "Type of Wi-Fi channel: spectrum or yans", wifiType);
  cmd.AddValue("errorModel", "Type of error model (n = none)", errorModel);
  cmd.AddValue("pcap", "Enable PCAP tracing", pcapTracing);
  cmd.Parse(argc, argv);

  // Nodes
  NodeContainer wifiStaNode;
  wifiStaNode.Create(1);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  NodeContainer interfererNode;
  interfererNode.Create(1);

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));
  positionAlloc->Add(Vector(distance, 0.0, 0.0));
  positionAlloc->Add(Vector(distance/2, 5.0, 0));
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNode);
  mobility.Install(wifiApNode);
  mobility.Install(interfererNode);

  // Wi-Fi configuration
  YansWifiPhyHelper wifiPhy;
  SpectrumWifiPhyHelper spectrumPhy;

  NetDeviceContainer staDevice, apDevice;

  if (wifiType == "spectrum") {
    // Spectrum channel
    Ptr<MultiModelSpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel> ();
    Ptr<ConstantSpeedPropagatonDelayModel> delayModel = CreateObject<ConstantSpeedPropagatonDelayModel>();
    spectrumChannel->SetPropagationDelayModel(delayModel);
    Ptr<LogDistancePropagationLossModel> propLoss = CreateObject<LogDistancePropagationLossModel>();
    spectrumChannel->AddPropagationLossModel(propLoss);

    spectrumPhy = SpectrumWifiPhyHelper::Default();
    spectrumPhy.SetChannel(spectrumChannel);
    spectrumPhy.Set("ChannelWidth", UintegerValue(channelWidth));
    spectrumPhy.Set("TxPowerStart", DoubleValue(20.0));
    spectrumPhy.Set("TxPowerEnd", DoubleValue(20.0));
    spectrumPhy.SetPcapDataLinkType(SpectrumWifiPhyHelper::DLT_IEEE802_11_RADIO);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns3-80211n");
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs" + std::to_string(mcsIndex)),
                                 "ControlMode", StringValue("HtMcs" + std::to_string(mcsIndex)),
                                 "RtsCtsThreshold", UintegerValue(2200));
    staDevice = wifi.Install(spectrumPhy, wifiMac, wifiStaNode);

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));
    apDevice = wifi.Install(spectrumPhy, wifiMac, wifiApNode);

  } else {
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.Set("ChannelWidth", UintegerValue(channelWidth));
    wifiPhy.Set("TxPowerStart", DoubleValue(20.0));
    wifiPhy.Set("TxPowerEnd", DoubleValue(20.0));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns3-80211n");
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs" + std::to_string(mcsIndex)),
                                 "ControlMode", StringValue("HtMcs" + std::to_string(mcsIndex)),
                                 "RtsCtsThreshold", UintegerValue(2200));
    staDevice = wifi.Install(wifiPhy, wifiMac, wifiStaNode);

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));
    apDevice = wifi.Install(wifiPhy, wifiMac, wifiApNode);
  }

  // Interferer: non-Wi-Fi waveform (modeled as spectrum or raw emitter)
  Ptr<SpectrumChannel> spectrumChannel;
  if (wifiType == "spectrum")
    spectrumChannel = DynamicCast<SpectrumWifiPhy>(apDevice.Get(0)->GetObject<NetDevice>()->GetObject<WifiNetDevice>()->GetPhy())->GetChannel();
  else {
    spectrumChannel = 0;
  }
  if (spectrumChannel)
  {
    Ptr<WaveformGenerator> waveformGenerator = CreateObject<WaveformGenerator>();
    waveformGenerator->SetAttribute("Period", TimeValue(Seconds(0.00001)));
    waveformGenerator->SetAttribute("DutyCycle", DoubleValue(0.5));
    waveformGenerator->SetAttribute("Amplitude", DoubleValue(pow(10, waveformPower/10.0)/1000.0)); // convert dBm to W
    Ptr<SpectrumPhy> spectrumPhy = CreateObject<SpectrumPhy>();
    spectrumPhy->SetChannel(spectrumChannel);
    waveformGenerator->Start(Seconds(1.0));
    interfererNode.Get(0)->AggregateObject(waveformGenerator);
    interfererNode.Get(0)->AggregateObject(spectrumPhy);
  }

  // Internet stack and IP addresses
  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface, staInterface;
  apInterface = address.Assign(apDevice);
  staInterface = address.Assign(staDevice);

  // Applications: TCP and UDP flows
  uint16_t port = 9;

  // UDP
  UdpServerHelper udpServer(port);
  ApplicationContainer serverApp = udpServer.Install(wifiStaNode.Get(0));
  serverApp.Start(Seconds(2.0));
  serverApp.Stop(Seconds(simulationTime));

  UdpClientHelper udpClient(staInterface.GetAddress(0), port);
  udpClient.SetAttribute("MaxPackets", UintegerValue(1000000));
  udpClient.SetAttribute("Interval", TimeValue(MicroSeconds(500)));
  udpClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApp = udpClient.Install(wifiApNode.Get(0));
  clientApp.Start(Seconds(2.1));
  clientApp.Stop(Seconds(simulationTime));

  // TCP
  uint16_t tcpPort = 50000;
  Address sinkAddress(InetSocketAddress(staInterface.GetAddress(0), tcpPort));
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer tcpSinkApp = sinkHelper.Install(wifiStaNode.Get(0));
  tcpSinkApp.Start(Seconds(3.0));
  tcpSinkApp.Stop(Seconds(simulationTime));

  OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(1448));
  onoff.SetAttribute("StartTime", TimeValue(Seconds(3.1)));
  onoff.SetAttribute("StopTime", TimeValue(Seconds(simulationTime)));
  ApplicationContainer tcpSourceApp = onoff.Install(wifiApNode.Get(0));

  // Tracing
  if (pcapTracing)
  {
    if (wifiType == "spectrum")
      spectrumPhy.EnablePcap("80211n-spectrum-ap", apDevice.Get(0));
    else
      wifiPhy.EnablePcap("80211n-yans-ap", apDevice.Get(0));
  }

  Config::Connect("/NodeList/*/DeviceList/*/Phy/MonitorSnifferRx", MakeBoundCallback(&RxPacketTrace));
  Config::Connect("/NodeList/*/DeviceList/*/Phy/PhyTxBegin", MakeBoundCallback(&PhyTxTrace));

  // Statistics reporting
  Ptr<PacketSink> sink = DynamicCast<PacketSink>(tcpSinkApp.Get(0));
  double lastTotalRx = 0.0;
  double totalThroughput = 0.0;
  Simulator::Schedule(Seconds(4.0), &CalcThroughput, sink, Seconds(1.0), lastTotalRx, totalThroughput);

  // FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
  std::cout << "\nFlow Monitor Results:\n";
  for (auto& f : stats)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(f.first);
    std::cout << "FlowID: " << f.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << f.second.txPackets << "\n";
    std::cout << "  Rx Packets: " << f.second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << f.second.lostPackets << "\n";
    if (f.second.rxPackets > 0)
    {
      std::cout << "  Throughput: " << (f.second.rxBytes * 8.0 / (simulationTime - 1.0) / 1e6) << " Mbps\n";
    }
    else
    {
      std::cout << "  Throughput: 0 Mbps\n";
    }
  }

  Simulator::Destroy();
  return 0;
}