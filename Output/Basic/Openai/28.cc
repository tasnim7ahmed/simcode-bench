#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedWifiComparison");

enum TrafficType
{
  UDP_TRAFFIC,
  TCP_TRAFFIC
};

struct Scenario
{
  std::string name;
  bool erpProtection;
  bool shortSlotTime;
  bool htSupported;
  bool shortPreamble;
  uint32_t payloadSize;
  TrafficType trafficType;
};

void PrintThroughput(Ptr<FlowMonitor> flowMonitor)
{
  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
  double throughputSum = 0.0;
  uint32_t flowCount = 0;
  for (auto const &flow : stats)
  {
    double throughput = (flow.second.rxBytes * 8.0) / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1e6;
    throughputSum += throughput;
    flowCount++;
  }
  if (flowCount == 0)
  {
    std::cout << "No flows detected!" << std::endl;
  }
  else
  {
    std::cout << "Avg Throughput: " << throughputSum / flowCount << " Mbps" << std::endl;
  }
}

int main(int argc, char *argv[])
{
  // List of test scenarios
  std::vector<Scenario> scenarios = {
      {"Legacy/ERP, UDP, 512B", true, true, false, true, 512, UDP_TRAFFIC},
      {"Legacy/ERP, TCP, 1400B", true, true, false, true, 1400, TCP_TRAFFIC},
      {"ERP w/o Prot, short slot, UDP, 1400B", false, true, false, true, 1400, UDP_TRAFFIC},
      {"ERP w/ Prot, legacy slot, TCP, 512B", true, false, false, false, 512, TCP_TRAFFIC},
      {"HT/non-HT Mixed, short slot, UDP, 1024B", true, true, true, true, 1024, UDP_TRAFFIC},
      {"HT Only, short slot, TCP, 2048B", false, true, true, true, 2048, TCP_TRAFFIC},
  };

  for (auto scenario : scenarios)
  {
    std::cout << "---- Scenario: " << scenario.name << " ----" << std::endl;

    // Clean up the simulation before each repeat
    Simulator::Destroy();

    // Create nodes: 1 AP, 2 STA legacy, 2 STA HT (if enabled)
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    NodeContainer legacyStaNodes, htStaNodes;
    legacyStaNodes.Create(2);
    if (scenario.htSupported)
    {
      htStaNodes.Create(2);
    }

    // Manage channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure SSID
    Ssid ssid = Ssid("mixed-ssid");

    // Configure legacy and ERP (b/g)
    WifiHelper legacyWifi;
    legacyWifi.SetStandard(WIFI_STANDARD_80211g);
    WifiMacHelper legacyMac;

    phy.Set("ShortSlotTimeSupported", BooleanValue(scenario.shortSlotTime));
    phy.Set("ShortPreambleSupported", BooleanValue(scenario.shortPreamble));

    WifiHelper htWifi;
    WifiMacHelper htMac;
    if (scenario.htSupported)
    {
      htWifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);
    }

    // Set RemoteStationManager for fixed rates
    legacyWifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode", StringValue("ErpOfdmRate24Mbps"),
                                       "ControlMode", StringValue("ErpOfdmRate6Mbps"));
    if (scenario.htSupported)
    {
      htWifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode", StringValue("HtMcs3"),
                                     "ControlMode", StringValue("HtMcs0"));
    }

    // Protection settings for ERP
    Config::SetDefault("ns3::WifiMac::ErpProtectionMode", StringValue(scenario.erpProtection ? "CtstoSelf" : "None"));

    // Setup legacy/b/g stations
    NetDeviceContainer legacyStaDevices, htStaDevices, apDevices;
    legacyMac.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssid),
                      "QosSupported", BooleanValue(false),
                      "HtSupported", BooleanValue(false));

    legacyStaDevices = legacyWifi.Install(phy, legacyMac, legacyStaNodes);

    // Setup HT (n) stations, if enabled
    if (scenario.htSupported)
    {
      htMac.SetType("ns3::StaWifiMac",
                     "Ssid", SsidValue(ssid),
                     "QosSupported", BooleanValue(true),
                     "HtSupported", BooleanValue(true));
      htStaDevices = htWifi.Install(phy, htMac, htStaNodes);
    }

    // AP Configuration
    legacyMac.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid),
                      "QosSupported", BooleanValue(scenario.htSupported),
                      "HtSupported", BooleanValue(scenario.htSupported));
    if (scenario.htSupported)
      apDevices = htWifi.Install(phy, legacyMac, wifiApNode);
    else
      apDevices = legacyWifi.Install(phy, legacyMac, wifiApNode);

    // Mobility: all nodes static, placed near each other
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP
    positionAlloc->Add(Vector(1.0, 0.0, 0.0)); // STA1
    positionAlloc->Add(Vector(2.0, 0.0, 0.0)); // STA2
    positionAlloc->Add(Vector(3.0, 0.0, 0.0)); // STA3 (HT)
    positionAlloc->Add(Vector(4.0, 0.0, 0.0)); // STA4 (HT)
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(legacyStaNodes);
    if (scenario.htSupported)
      mobility.Install(htStaNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(legacyStaNodes);
    if (scenario.htSupported)
      stack.Install(htStaNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface, legacyStaInterfaces, htStaInterfaces;
    apInterface = address.Assign(apDevices);
    address.NewNetwork();
    legacyStaInterfaces = address.Assign(legacyStaDevices);
    if (scenario.htSupported)
    {
      address.NewNetwork();
      htStaInterfaces = address.Assign(htStaDevices);
    }

    // Server: AP node runs sink
    uint16_t port = 5000;

    ApplicationContainer sinks;
    ApplicationContainer sources;

    // Setup sink on AP node for each STA
    for (uint32_t i = 0; i < legacyStaNodes.GetN(); ++i)
    {
      if (scenario.trafficType == UDP_TRAFFIC)
      {
        UdpServerHelper udpServer(port + i);
        sinks.Add(udpServer.Install(wifiApNode.Get(0)));

        UdpClientHelper udpClient(apInterface.GetAddress(0), port + i);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000000));
        udpClient.SetAttribute("Interval", TimeValue(MicroSeconds(1500)));
        udpClient.SetAttribute("PacketSize", UintegerValue(scenario.payloadSize));
        sources.Add(udpClient.Install(legacyStaNodes.Get(i)));
      }
      else
      {
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port + i));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        sinks.Add(packetSinkHelper.Install(wifiApNode.Get(0)));

        OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port + i));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate("20Mbps")));
        onoff.SetAttribute("PacketSize", UintegerValue(scenario.payloadSize));
        onoff.SetAttribute("MaxBytes", UintegerValue(0));
        onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
        onoff.SetAttribute("StopTime", TimeValue(Seconds(11.0)));
        sources.Add(onoff.Install(legacyStaNodes.Get(i)));
      }
    }
    if (scenario.htSupported)
    {
      for (uint32_t i = 0; i < htStaNodes.GetN(); ++i)
      {
        if (scenario.trafficType == UDP_TRAFFIC)
        {
          UdpServerHelper udpServer(port + i + 10);
          sinks.Add(udpServer.Install(wifiApNode.Get(0)));

          UdpClientHelper udpClient(apInterface.GetAddress(0), port + i + 10);
          udpClient.SetAttribute("MaxPackets", UintegerValue(1000000));
          udpClient.SetAttribute("Interval", TimeValue(MicroSeconds(1500)));
          udpClient.SetAttribute("PacketSize", UintegerValue(scenario.payloadSize));
          sources.Add(udpClient.Install(htStaNodes.Get(i)));
        }
        else
        {
          Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port + i + 10));
          PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
          sinks.Add(packetSinkHelper.Install(wifiApNode.Get(0)));

          OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port + i + 10));
          onoff.SetAttribute("DataRate", DataRateValue(DataRate("30Mbps")));
          onoff.SetAttribute("PacketSize", UintegerValue(scenario.payloadSize));
          onoff.SetAttribute("MaxBytes", UintegerValue(0));
          onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
          onoff.SetAttribute("StopTime", TimeValue(Seconds(11.0)));
          sources.Add(onoff.Install(htStaNodes.Get(i)));
        }
      }
    }

    sinks.Start(Seconds(0.0));
    sinks.Stop(Seconds(12.0));
    sources.Start(Seconds(1.0));
    sources.Stop(Seconds(11.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(12.0));
    Simulator::Run();

    PrintThroughput(monitor);

    Simulator::Destroy();
  }

  return 0;
}