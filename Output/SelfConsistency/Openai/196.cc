#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ssid.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/pcap-file-wrapper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiApStaUdpExample");

// Statistics struct for each station
struct StationStats {
  uint32_t rxPackets = 0;
  uint32_t rxBytes = 0;
  Time     firstRxTime = Seconds(0);
  Time     lastRxTime = Seconds(0);
};

std::map<uint16_t, StationStats> g_stationStats;

// Packet receive callback for each station
void RxPacketCallback(Ptr<const Packet> packet, const Address &address, uint16_t port)
{
  uint32_t pktSize = packet->GetSize();
  uint16_t stationPort = port;
  StationStats &stats = g_stationStats[stationPort];

  stats.rxPackets += 1;
  stats.rxBytes += pktSize;
  Time now = Simulator::Now();
  if (stats.rxPackets == 1)
  {
    stats.firstRxTime = now;
  }
  stats.lastRxTime = now;
}

int main(int argc, char *argv[])
{
  uint32_t nSta = 2;
  double simulationTime = 10.0; // seconds

  // Create nodes: 2 STAs + 1 AP
  NodeContainer wifiStaNodes, wifiApNode;
  wifiStaNodes.Create(nSta);
  wifiApNode.Create(1);

  // Configure PHY and channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  // Configure WiFi
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211g);

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-wifi");

  // Set up STA devices
  mac.SetType("ns3::StaWifiMac", 
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

  // Set up AP device
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));  // AP
  positionAlloc->Add(Vector(5.0, 0.0, 0.0));  // STA 0
  positionAlloc->Add(Vector(10.0, 0.0, 0.0)); // STA 1

  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiApNode);
  mobility.Install(wifiStaNodes);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNodes);

  // IP address assignment
  Ipv4AddressHelper address;
  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

  // UDP server runs on AP, one port for each STA
  uint16_t basePort = 8000;
  ApplicationContainer serverApps;
  for(uint32_t i = 0; i < nSta; ++i)
  {
    UdpServerHelper server(basePort + i);
    ApplicationContainer app = server.Install(wifiApNode.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(simulationTime));
    serverApps.Add(app);
  }

  // UDP clients run on each STA, targeting their port at the AP
  ApplicationContainer clientApps;
  for(uint32_t i = 0; i < nSta; ++i)
  {
    UdpClientHelper client(apInterface.GetAddress(0), basePort + i);
    client.SetAttribute("MaxPackets", UintegerValue(32000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(50))); // 20 pkts/sec
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer app = client.Install(wifiStaNodes.Get(i));
    app.Start(Seconds(2.0));
    app.Stop(Seconds(simulationTime));
    clientApps.Add(app);
  }

  // Attach rx trace sinks to the UDP server (for each port)
  for(uint32_t i = 0; i < nSta; ++i)
  {
    Ptr<UdpServer> server = DynamicCast<UdpServer>(serverApps.Get(i));
    server->TraceConnectWithoutContext("Rx", MakeBoundCallback(&RxPacketCallback, uint16_t(basePort + i)));
  }

  // Enable pcap tracing for all devices
  phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
  phy.EnablePcap("wifi-ap", apDevice, true, true);
  phy.EnablePcap("wifi-sta0", staDevices.Get(0), true, true);
  phy.EnablePcap("wifi-sta1", staDevices.Get(1), true, true);

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  // Print stats
  std::cout << "\n==== UDP Traffic Statistics per STA ====\n";
  for(uint32_t i = 0; i < nSta; ++i)
  {
    uint16_t stationPort = basePort + i;
    StationStats &stats = g_stationStats[stationPort];
    double throughput = 0.0;
    double duration = (stats.lastRxTime - stats.firstRxTime).GetSeconds();
    if (duration > 0)
    {
      throughput = (stats.rxBytes * 8.0) / duration / 1024.0; // kbps
    }

    std::cout << "STA " << i
              << " (Port " << stationPort << "): "
              << "\n  Packets received: " << stats.rxPackets
              << "\n  Bytes received: " << stats.rxBytes
              << "\n  Throughput: " << throughput << " kbps"
              << std::endl;
  }

  Simulator::Destroy();

  return 0;
}