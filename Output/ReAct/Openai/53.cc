#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/wifi-helper.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiN_Aggregation_Demo");

// TXOP tracing structures
struct TxopStats
{
  double maxDurationUs = 0.0;
  double totalBytes = 0.0;
};

// Store per-network stats
std::vector<TxopStats> g_txopStats(4);

// Callback for TXOP updates
void
TxopTraceCallback(uint32_t networkIdx, double txopStart, double txopDuration, uint64_t bytes)
{
  if (txopDuration > g_txopStats[networkIdx].maxDurationUs)
  {
    g_txopStats[networkIdx].maxDurationUs = txopDuration;
  }
  g_txopStats[networkIdx].totalBytes += bytes;
}

int main(int argc, char *argv[])
{
  double distance = 5.0;
  bool enableRtsCts = false;
  double simulationTime = 10.0;
  double txopLimitUs[4] = {5808, 5808, 5808, 5808}; // Default 802.11n TXOP limit in us for AC_BE

  CommandLine cmd;
  cmd.AddValue("distance", "Distance between AP and STA [m]", distance);
  cmd.AddValue("enableRtsCts", "Enable RTS/CTS?", enableRtsCts);
  cmd.AddValue("txopA", "TXOP limit for network A (us)", txopLimitUs[0]);
  cmd.AddValue("txopB", "TXOP limit for network B (us)", txopLimitUs[1]);
  cmd.AddValue("txopC", "TXOP limit for network C (us)", txopLimitUs[2]);
  cmd.AddValue("txopD", "TXOP limit for network D (us)", txopLimitUs[3]);
  cmd.Parse(argc, argv);

  uint8_t channels[4] = {36, 40, 44, 48};
  std::string ssids[4] = {"AP_A", "AP_B", "AP_C", "AP_D"};

  NodeContainer aps, stas;
  aps.Create(4);
  stas.Create(4);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phys[4];

  for (uint32_t i = 0; i < 4; ++i)
  {
    phys[i] = YansWifiPhyHelper::Default();
    phys[i].SetChannel(channel.Create());
    // Set channel number for each network
    phys[i].Set("ChannelNumber", UintegerValue(channels[i]));
  }

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

  WifiMacHelper mac;
  NetDeviceContainer apDevices[4], staDevices[4];

  for (uint32_t i = 0; i < 4; ++i)
  {
    // Configure MAC for AP
    Ssid ssid = Ssid(ssids[i]);
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "QosSupported", BooleanValue(true));

    apDevices[i] = wifi.Install(phys[i], mac, NodeContainer(aps.Get(i)));

    // Configure MAC for STA
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "QosSupported", BooleanValue(true));

    staDevices[i] = wifi.Install(phys[i], mac, NodeContainer(stas.Get(i)));
  }

  // Aggregation and TXOP setup
  for (uint32_t i = 0; i < 4; ++i)
  {
    Ptr<NetDevice> staDev = staDevices[i].Get(0)->GetObject<NetDevice>();
    Ptr<WifiNetDevice> wifiStaDev = DynamicCast<WifiNetDevice>(staDev);
    Ptr<WifiMac> macSta = wifiStaDev->GetMac();

    Ptr<NetDevice> apDev = apDevices[i].Get(0)->GetObject<NetDevice>();
    Ptr<WifiNetDevice> wifiApDev = DynamicCast<WifiNetDevice>(apDev);
    Ptr<WifiMac> macAp = wifiApDev->GetMac();

    // Set aggregation settings per network
    if (i == 0)
    {
      // Station A: default A-MPDU, max size 65kB
      Config::Set("/NodeList/" + std::to_string(stas.Get(i)->GetId()) +
                      "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DcaTxop/MaxAmpduSize",
                  UintegerValue(65536));
      // Enable A-MPDU, default A-MSDU
      Config::Set("/NodeList/" + std::to_string(stas.Get(i)->GetId()) +
                      "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DcaTxop/AmsduEnabled",
                  BooleanValue(false));
    }
    else if (i == 1)
    {
      // Station B: disable aggregation
      Config::Set("/NodeList/" + std::to_string(stas.Get(i)->GetId()) +
                      "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DcaTxop/AmpduEnabled",
                  BooleanValue(false));
      Config::Set("/NodeList/" + std::to_string(stas.Get(i)->GetId()) +
                      "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DcaTxop/AmsduEnabled",
                  BooleanValue(false));
    }
    else if (i == 2)
    {
      // Station C: A-MSDU only, max size 8kB, disable A-MPDU
      Config::Set("/NodeList/" + std::to_string(stas.Get(i)->GetId()) +
                      "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DcaTxop/AmpduEnabled",
                  BooleanValue(false));
      Config::Set("/NodeList/" + std::to_string(stas.Get(i)->GetId()) +
                      "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DcaTxop/AmsduEnabled",
                  BooleanValue(true));
      Config::Set("/NodeList/" + std::to_string(stas.Get(i)->GetId()) +
                      "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DcaTxop/MaxAmsduSize",
                  UintegerValue(8192));
    }
    else if (i == 3)
    {
      // Station D: both A-MPDU (32kB) and A-MSDU (4kB)
      Config::Set("/NodeList/" + std::to_string(stas.Get(i)->GetId()) +
                      "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DcaTxop/AmpduEnabled",
                  BooleanValue(true));
      Config::Set("/NodeList/" + std::to_string(stas.Get(i)->GetId()) +
                      "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DcaTxop/AmsduEnabled",
                  BooleanValue(true));
      Config::Set("/NodeList/" + std::to_string(stas.Get(i)->GetId()) +
                      "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DcaTxop/MaxAmpduSize",
                  UintegerValue(32768));
      Config::Set("/NodeList/" + std::to_string(stas.Get(i)->GetId()) +
                      "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DcaTxop/MaxAmsduSize",
                  UintegerValue(4096));
    }

    // Set TXOP limit for AC_BE
    // Both AP and STA for fairness
    Config::Set("/NodeList/" + std::to_string(aps.Get(i)->GetId()) +
                    "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::ApWifiMac/EdcaQueues/0/Aifsn",
                UintegerValue(3));
    Config::Set("/NodeList/" + std::to_string(aps.Get(i)->GetId()) +
                    "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::ApWifiMac/EdcaQueues/0/TXOPLimit",
                TimeValue(MicroSeconds(txopLimitUs[i])));

    Config::Set("/NodeList/" + std::to_string(stas.Get(i)->GetId()) +
                    "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/EdcaQueues/0/Aifsn",
                UintegerValue(3));
    Config::Set("/NodeList/" + std::to_string(stas.Get(i)->GetId()) +
                    "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/EdcaQueues/0/TXOPLimit",
                TimeValue(MicroSeconds(txopLimitUs[i])));

    // Set RTS/CTS threshold
    Config::Set("/NodeList/" + std::to_string(stas.Get(i)->GetId()) +
                    "/DeviceList/0/$ns3::WifiNetDevice/Phy/ReceiveOkRssi", DoubleValue(-60.0));
    Config::Set("/NodeList/" + std::to_string(stas.Get(i)->GetId()) +
                    "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/RtsCtsThreshold",
                UintegerValue(enableRtsCts ? 0 : 2347));
    Config::Set("/NodeList/" + std::to_string(aps.Get(i)->GetId()) +
                    "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/RtsCtsThreshold",
                UintegerValue(enableRtsCts ? 0 : 2347));
  }

  // Mobility: place each AP/STA pair at specific locations, separated by distance
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
  for (uint32_t i = 0; i < 4; ++i)
  {
    // Place APs along x axis (i*20, 0, 0), STA at (i*20 + distance, 0, 0)
    posAlloc->Add(Vector(i * 20.0, 0, 0));
    posAlloc->Add(Vector(i * 20.0 + distance, 0, 0));
  }
  mobility.SetPositionAllocator(posAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  NodeContainer allNodes;
  for (uint32_t i = 0; i < 4; ++i)
  {
    allNodes.Add(aps.Get(i));
    allNodes.Add(stas.Get(i));
  }
  mobility.Install(allNodes);

  // Stack and IP address assignment
  InternetStackHelper internet;
  internet.Install(allNodes);

  Ipv4AddressHelper ipv4;
  std::string netbases[4] = {"10.0.0.0", "10.0.1.0", "10.0.2.0", "10.0.3.0"};
  Ipv4InterfaceContainer apIfaces[4], staIfaces[4];

  for (uint32_t i = 0; i < 4; ++i)
  {
    ipv4.SetBase(Ipv4Address(netbases[i].c_str()), "255.255.255.0");
    Ipv4InterfaceContainer ifaces = ipv4.Assign(NetDeviceContainer(apDevices[i], staDevices[i]));
    apIfaces[i].Add(ifaces.Get(0));
    staIfaces[i].Add(ifaces.Get(1));
  }

  // Applications: UDP server on AP, client on STA for each network
  uint16_t port = 5000;
  ApplicationContainer serverApps, clientApps;

  for (uint32_t i = 0; i < 4; ++i)
  {
    // Server (sink) at AP
    UdpServerHelper server(port);
    serverApps.Add(server.Install(aps.Get(i)));

    // Client at STA
    UdpClientHelper client(apIfaces[i].GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client.SetAttribute("Interval", TimeValue(Time("0.0001s")));
    client.SetAttribute("PacketSize", UintegerValue(1472));
    clientApps.Add(client.Install(stas.Get(i)));
  }

  serverApps.Start(Seconds(1.0));
  clientApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(1.0 + simulationTime + 1));
  clientApps.Stop(Seconds(1.0 + simulationTime));

  // Setup TXOP tracing for each AP
  for (uint32_t i = 0; i < 4; ++i)
  {
    Ptr<NetDevice> apDev = apDevices[i].Get(0)->GetObject<NetDevice>();
    Ptr<WifiNetDevice> wifiApDev = DynamicCast<WifiNetDevice>(apDev);
    Ptr<WifiMac> apMac = wifiApDev->GetMac();
    Ptr<EdcaTxopN> edca = apMac->GetBEQueue();
    // custom trace source: connect to "TxopTrace" and use a callback with network index
    edca->TraceConnectWithoutContext("TxopTrace",
      MakeBoundCallback([](uint32_t idx, uint64_t start, uint64_t duration, uint64_t bytes){
        TxopTraceCallback(idx, double(start), double(duration), bytes);
      }, i)
    );
  }

  GlobalRoutingHelper::PopulateRoutingTables();
  Simulator::Stop(Seconds(1.0 + simulationTime + 2));
  Simulator::Run();

  // Throughput Output
  std::cout << "Results:\n";
  for (uint32_t i = 0; i < 4; ++i)
  {
    uint64_t rxBytes = 0;
    Ptr<UdpServer> server = serverApps.Get(i)->GetObject<UdpServer>();
    rxBytes = server->GetReceived() * 1472;

    double throughputMbps = ((rxBytes * 8.0) / simulationTime) / 1e6;
    std::cout << "Network " << char('A' + i) << " (Channel " << int(channels[i])
              << "): Throughput=" << throughputMbps << " Mbps, "
              << "Max TXOP Duration=" << g_txopStats[i].maxDurationUs / 1e6 << " s\n";
  }

  Simulator::Destroy();
  return 0;
}