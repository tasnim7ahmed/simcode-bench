#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("QosWiFiExample");

class QosStats {
public:
  QosStats(std::string name, uint32_t payloadSize, Time simulationTime)
      : m_name(name), m_payloadSize(payloadSize), m_simulationTime(simulationTime),
        m_packetsSent(0), m_bytesSent(0) {}

  void TxCallback(Ptr<const Packet> packet, const WifiMacHeader &header) {
    m_packetsSent++;
    m_bytesSent += packet->GetSize();
  }

  void PrintResults() {
    double throughput = (m_bytesSent * 8.0) / (m_simulationTime.GetSeconds() * 1e6);
    std::cout << m_name << ": Throughput=" << throughput << " Mbps, Packets Sent=" << m_packetsSent << std::endl;
  }

private:
  std::string m_name;
  uint32_t m_payloadSize;
  Time m_simulationTime;
  uint32_t m_packetsSent;
  uint64_t m_bytesSent;
};

int main(int argc, char *argv[]) {
  uint32_t payloadSize = 1472; // bytes
  double distance = 1.0;       // meters
  bool enableRtsCts = false;
  Time simulationTime = Seconds(10);
  bool pcapEnabled = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("distance", "Distance between nodes in meters", distance);
  cmd.AddValue("enableRtsCts", "Enable RTS/CTS handshake", enableRtsCts);
  cmd.AddValue("simulationTime", "Total simulation time", simulationTime);
  cmd.AddValue("pcap", "Enable pcap file generation", pcapEnabled);
  cmd.Parse(argc, argv);

  NodeContainer apNodes, staNodes;
  apNodes.Create(4);
  staNodes.Create(4);

  YansWifiPhyHelper phy;
  phy.Set("ChannelSettings", StringValue("{0, 0, BW20MHz, 0}"));
  phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

  WifiChannelHelper channel = WifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid;
  NetDeviceContainer apDevices[4], staDevices[4];

  for (uint32_t i = 0; i < 4; ++i) {
    std::ostringstream oss;
    oss << "network-" << i;
    ssid = Ssid(oss.str());

    // AP setup
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    if (enableRtsCts) {
      Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/RtsCtsThreshold", UintegerValue(1));
    }
    apDevices[i] = mac.Install(phy, apNodes.Get(i));

    // STA setup
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    staDevices[i] = mac.Install(phy, staNodes.Get(i));
  }

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(4),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(NodeContainer(apNodes, staNodes));

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer apInterfaces[4], staInterfaces[4];
  for (uint32_t i = 0; i < 4; ++i) {
    std::ostringstream network;
    network << "192.168." << i << ".0";
    address.SetBase(network.str().c_str(), "255.255.255.0");
    apInterfaces[i] = address.Assign(apDevices[i]);
    staInterfaces[i] = address.Assign(staDevices[i]);
  }

  UdpServerHelper server(9);
  ApplicationContainer serverApps;
  for (uint32_t i = 0; i < 4; ++i) {
    serverApps.Add(server.Install(apNodes.Get(i)));
  }
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(simulationTime);

  OnOffHelper client("ns3::UdpSocketFactory", InetSocketAddress());
  client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  client.SetAttribute("PacketSize", UintegerValue(payloadSize));
  client.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < 4; ++i) {
    InetSocketAddress remote(staInterfaces[i].GetAddress(0), 9);
    client.SetAttribute("Remote", AddressValue(remote));
    clientApps = client.Install(staNodes.Get(i));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(simulationTime - Seconds(0.1));
  }

  if (pcapEnabled) {
    phy.EnablePcapAll("qos-wifi-example");
  }

  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpServer/Rx", MakeCallback([](Ptr<const Packet> p, const Address &) {
    static std::map<std::string, QosStats> statsMap;
    std::string context;
    Simulator::GetContext(context);
    if (!statsMap.count(context)) {
      statsMap.emplace(context, QosStats(context, payloadSize, simulationTime));
    }
    statsMap[context].TxCallback(p, WifiMacHeader());
  }));

  Simulator::Stop(simulationTime);
  Simulator::Run();

  std::map<std::string, QosStats> statsMap;
  Config::WalkAlive();

  std::cout << "Simulation completed. Results:" << std::endl;
  for (auto &pair : statsMap) {
    pair.second.PrintResults();
  }

  Simulator::Destroy();
  return 0;
}