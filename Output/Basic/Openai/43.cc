#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi11nMpduAggHiddenNodes");

int
main(int argc, char *argv[])
{
  bool rtsCts = false;
  uint32_t nMpdu = 8;
  uint32_t payloadSize = 1460;
  double simulationTime = 10.0;
  double expectedThroughput = 30.0; // Mbps
  std::string phyMode = "HtMcs7";
  uint32_t mcs = 7;
  std::string asciiTraceFile = "wifi11n-hidden.ascii";
  std::string pcapFile = "wifi11n-hidden";
  uint64_t maxPackets = 0;
  double distance = 7.0; // meters, set later
  uint32_t maxAggr = 65535;

  CommandLine cmd;
  cmd.AddValue("rtsCts", "Enable RTS/CTS", rtsCts);
  cmd.AddValue("aggregatedMpdu", "Number of aggregated MPDUs", nMpdu);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("expectedThroughput", "Expected throughput (Mbps)", expectedThroughput);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue(rtsCts ? "0" : "2200"));
  Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("2200"));
  Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(phyMode));
  Config::SetDefault("ns3::WifiRemoteStationManager::DataMode", StringValue(phyMode));
  Config::SetDefault("ns3::WifiRemoteStationManager::ControlMode", StringValue(phyMode));
  Config::SetDefault("ns3::WifiMac::MaxAmpduSize", UintegerValue(maxAggr));
  Config::SetDefault("ns3::WifiMac::MaxAmsduSize", UintegerValue(0));
  Config::SetDefault("ns3::WifiMac::BE_MaxAmpduSize", UintegerValue(maxAggr));

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(2);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  phy.Set("RxGain", DoubleValue(0));
  phy.Set("TxPowerStart", DoubleValue(20));
  phy.Set("TxPowerEnd", DoubleValue(20));
  phy.Set("EnergyDetectionThreshold", DoubleValue(-101));
  phy.Set("CcaMode1Threshold", DoubleValue(-101));

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                              "DataMode", StringValue(phyMode),
                              "ControlMode", StringValue(phyMode));

  WifiMacHelper mac;
  Ssid ssid = Ssid("hidden-nodes-ssid");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false),
              "BE_MaxAmpduSize", UintegerValue(nMpdu * payloadSize)
             );
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BE_MaxAmpduSize", UintegerValue(nMpdu * payloadSize)
             );
  NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
  posAlloc->Add(Vector(0.0, 0.0, 0.0));        // AP
  posAlloc->Add(Vector(5.0, 0.0, 0.0));        // STA1
  posAlloc->Add(Vector(5.0, 5.1, 0.0));        // STA2, >5m from STA1
  mobility.SetPositionAllocator(posAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(NodeContainer(wifiApNode, wifiStaNodes));

  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

  uint16_t port = 5000;

  ApplicationContainer sinkApps;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", sinkLocalAddress);
  sinkApps = packetSinkHelper.Install(wifiApNode.Get(0));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(simulationTime + 1));

  ApplicationContainer udpApps;
  for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
    {
      OnOffHelper onoff("ns3::UdpSocketFactory",
                        Address(InetSocketAddress(apInterface.GetAddress(0), port)));
      onoff.SetAttribute("DataRate", DataRateValue(DataRate("1000Mbps")));
      onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
      onoff.SetAttribute("MaxPacketCount", UintegerValue(maxPackets));
      onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
      onoff.SetAttribute("StopTime", TimeValue(Seconds(simulationTime)));
      udpApps.Add(onoff.Install(wifiStaNodes.Get(i)));
    }

  phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
  phy.EnablePcap(pcapFile, apDevice);
  phy.EnablePcap(pcapFile + "-sta", staDevices);
  
  AsciiTraceHelper ascii;
  phy.EnableAsciiAll(ascii.CreateFileStream(asciiTraceFile));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simulationTime + 1));
  Simulator::Run();

  monitor->CheckForLostPackets();
  double throughput = 0.0;
  auto stats = monitor->GetFlowStats();
  for (auto flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = flowmon.GetClassifier()->FindFlow(flow.first);
      if (t.destinationPort == port)
        {
          double txTime = (std::min<double>(simulationTime, flow.second.timeLastRxPacket.GetSeconds()) -
                           std::max<double>(0.0, flow.second.timeFirstTxPacket.GetSeconds()));
          if (txTime > 0)
            throughput += (flow.second.rxBytes * 8.0) / (txTime * 1e6);
        }
    }

  std::cout << "Measured throughput: " << throughput << " Mbps\n";
  if (std::abs(throughput - expectedThroughput) > 0.1 * expectedThroughput)
    {
      std::cout << "ERROR: Throughput out of expected bound: " << expectedThroughput << " Mbps\n";
      Simulator::Destroy();
      return 1;
    }

  Simulator::Destroy();
  return 0;
}