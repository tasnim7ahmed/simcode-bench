#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/qos-wifi-mac-helper.h"
#include "ns3/ssid.h"
#include "ns3/on-off-helper.h"
#include "ns3/udp-server-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/applications-module.h"
#include "ns3/pcap-file-wrapper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiQosMultiNetworkExample");

struct AppFlowStats
{
  uint64_t rxBytes;
  uint32_t rxPackets;
  Time    startRx;
  Time    lastRx;
};

struct TxopInfo
{
  Time start;
  Time totalDuration;
  bool ongoing;
};

void
RxPacketTrace(Ptr<AppFlowStats> stats, Ptr<const Packet> packet, const Address &addr)
{
  if (stats->rxPackets == 0)
    stats->startRx = Simulator::Now();
  stats->lastRx = Simulator::Now();
  stats->rxPackets++;
  stats->rxBytes += packet->GetSize();
}

void
TxopTraceFunction(Ptr<TxopInfo> txop, std::string context, Time duration)
{
  txop->totalDuration += duration;
}

int main(int argc, char *argv[])
{
  // User-configurable parameters
  uint32_t payloadSize = 1472;
  double   distance = 20.0;
  bool     rtsCts = false;
  double   simTime = 10.0;
  bool     enablePcap = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("payloadSize", "Payload size [bytes]", payloadSize);
  cmd.AddValue("distance", "Distance between AP and STA [meters]", distance);
  cmd.AddValue("rtsCts", "Enable RTS/CTS", rtsCts);
  cmd.AddValue("simTime", "Simulation time [s]", simTime);
  cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.Parse(argc, argv);

  // Configurations
  uint32_t numNetworks = 4;
  double   channelSpacing = 5.0; // meters offset for each network
  std::vector<std::string> acNames = {"AC_BE", "AC_VI"};
  std::vector<AcIndex> acs = {AC_BE, AC_VI};

  Time appStartTime = Seconds(1.0);
  Time appStopTime  = Seconds(simTime);

  // Set global WiFi parameters
  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(rtsCts ? 0 : 65535));
  Config::SetDefault("ns3::WifiMacQueue::MaxSize", StringValue("100p"));

  // Helper objects
  YansWifiChannelHelper  wifiChannel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper      wifiPhy     = YansWifiPhyHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());
  wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

  QosWifiMacHelper       wifiMac = QosWifiMacHelper::Default();
  WifiHelper             wifi    = WifiHelper::Default();
  wifi.SetStandard(WIFI_STANDARD_80211g);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("OfdmRate24Mbps"),
                               "ControlMode", StringValue("OfdmRate24Mbps"));

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

  InternetStackHelper stack;
  Ipv4AddressHelper ipv4Base;
  ipv4Base.SetBase("10.0.0.0", "255.255.255.0");

  // Per-network containers
  NodeContainer apNodes, staNodes;
  std::vector<NetDeviceContainer> apDevices, staDevices;
  std::vector<Ipv4InterfaceContainer> apIfaces, staIfaces;
  std::vector<ApplicationContainer> serverApps, clientApps;
  std::vector<Ptr<UdpServer>> serverPtrs;
  std::vector<Ipv4Address> apAddresses;
  std::vector<Ptr<AppFlowStats>> flowStats;
  std::vector<Ptr<TxopInfo>> txopInfos;

  for (uint32_t i = 0; i < numNetworks; ++i)
    {
      // One AP and one STA per network
      NodeContainer aps, stas;
      aps.Create(1);
      stas.Create(1);
      apNodes.Add(aps);
      staNodes.Add(stas);

      // SSID and MAC
      std::ostringstream oss;
      oss << "network-" << i;
      Ssid ssid = Ssid(oss.str());

      wifiMac.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid));
      NetDeviceContainer apDev = wifi.Install(wifiPhy, wifiMac, aps);

      wifiMac.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssid),
                      "ActiveProbing", BooleanValue(false));
      NetDeviceContainer staDev = wifi.Install(wifiPhy, wifiMac, stas);

      apDevices.push_back(apDev);
      staDevices.push_back(staDev);

      // Set AP and STA positions for this network
      Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
      positionAlloc->Add(Vector(i * channelSpacing, 0.0, 0.0));  // AP position
      positionAlloc->Add(Vector(i * channelSpacing, distance, 0.0)); // STA position
      mobility.SetPositionAllocator(positionAlloc);

      NodeContainer pair;
      pair.Add(aps);
      pair.Add(stas);
      mobility.Install(pair);

      // IP stack
      stack.Install(pair);

      // IP assignment
      Ipv4AddressHelper subnet;
      std::ostringstream sb;
      sb << "10." << i << ".0.0";
      subnet.SetBase(sb.str().c_str(), "255.255.255.0");
      Ipv4InterfaceContainer ifacePair = subnet.Assign(NetDeviceContainer(apDev, staDev));
      apIfaces.push_back(Ipv4InterfaceContainer(ifacePair.Get(0)));
      staIfaces.push_back(Ipv4InterfaceContainer(ifacePair.Get(1)));
      apAddresses.push_back(ifacePair.GetAddress(0));
    }

  // Application/traffic configuration
  for (uint32_t i = 0; i < numNetworks; ++i)
    {
      for (uint32_t j = 0; j < acs.size(); ++j)
        {
          // UDP Server at AP
          uint16_t port = 9000 + i * 10 + j;
          UdpServerHelper udpServer(port);
          ApplicationContainer serverApp = udpServer.Install(apNodes.Get(i));
          serverApp.Start(appStartTime);
          serverApp.Stop(appStopTime);
          serverApps.push_back(serverApp);

          // Save a pointer to get stats later
          Ptr<UdpServer> srv = DynamicCast<UdpServer>(serverApp.Get(0));
          serverPtrs.push_back(srv);

          // UDP Client (OnOff) at STA
          OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(apAddresses[i], port)));
          onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
          onoff.SetAttribute("DataRate", DataRateValue(DataRate("6Mbps")));
          onoff.SetAttribute("StartTime", TimeValue(appStartTime));
          onoff.SetAttribute("StopTime", TimeValue(appStopTime));
          onoff.SetAttribute("MaxBytes", UintegerValue(0));
          onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
          onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

          // Set user priority / QoS ac
          Ptr<Application> cApp = onoff.Install(staNodes.Get(i)).Get(0);
          Ptr<OnOffApplication> onoffPtr = DynamicCast<OnOffApplication>(cApp);
          onoffPtr->SetAttribute("QosTid", UintegerValue(j == 0 ? 0 : 5)); // AC_BE (0), AC_VI (5)

          // Store app stats
          Ptr<AppFlowStats> stats = CreateObject<AppFlowStats>();
          stats->rxBytes = 0;
          stats->rxPackets = 0;
          stats->startRx = Seconds(0);
          stats->lastRx = Seconds(0);

          // Trace UDP packet reception for throughput
          std::string path = "/NodeList/" + std::to_string(apNodes.Get(i)->GetId()) +
              "/ApplicationList/" + std::to_string(serverApps.size() - 1) + "/$ns3::UdpServer/Rx";
          Config::ConnectWithoutContext(path, MakeBoundCallback(&RxPacketTrace, stats));
          flowStats.push_back(stats);

          // TXOP trace
          Ptr<TxopInfo> txop = CreateObject<TxopInfo>();
          txop->start = Seconds(0);
          txop->totalDuration = Seconds(0);
          txop->ongoing = false;

          // Find MAC on STA device
          Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice>(staDevices[i].Get(0));
          Ptr<RegularWifiMac> regMac = DynamicCast<RegularWifiMac>(wifiDev->GetMac());
          std::string txopCtx = "/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) +
                                "/DeviceList/0/$ns3::WifiNetDevice/Mac/Txop/TxopUsed";

          Config::Connect(txopCtx, MakeBoundCallback(&TxopTraceFunction, txop));
          txopInfos.push_back(txop);

          clientApps.push_back(ApplicationContainer(cApp));
        }
    }

  // PCAP tracing
  if (enablePcap)
    {
      for (uint32_t i = 0; i < numNetworks; ++i)
        {
          wifiPhy.EnablePcap("ap-" + std::to_string(i), apDevices[i].Get(0));
          wifiPhy.EnablePcap("sta-" + std::to_string(i), staDevices[i].Get(0));
        }
    }

  // Run simulation
  Simulator::Stop(Seconds(simTime + 1));
  Simulator::Run();

  // Output throughput and TXOP durations
  std::cout << "QoS Wi-Fi Networks Throughput/Txop Statistics:" << std::endl;
  for (uint32_t i = 0; i < numNetworks; ++i)
    {
      for (uint32_t j = 0; j < acs.size(); ++j)
        {
          uint32_t flowIdx = i * acs.size() + j;
          Ptr<AppFlowStats> stats = flowStats[flowIdx];
          double duration = (stats->lastRx - stats->startRx).GetSeconds();
          double throughput = 0.0;
          if (duration > 0)
            throughput = (stats->rxBytes * 8.0) / duration / 1e6; // Mbps
          else
            throughput = 0.0;

          std::cout << "Network[" << i << "] AC[" << acNames[j] << "]: ";
          std::cout << "RxBytes=" << stats->rxBytes << ", RxPackets=" << stats->rxPackets;
          std::cout << ", Duration=" << duration << "s";
          std::cout << ", Throughput=" << throughput << " Mbps";
          std::cout << ", TXOP total=" << txopInfos[flowIdx]->totalDuration.GetMicroSeconds() / 1e6 << " s";
          std::cout << std::endl;
        }
    }

  Simulator::Destroy();
  return 0;
}