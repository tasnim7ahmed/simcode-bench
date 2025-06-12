#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("QosWiFiExample");

class QosWifiExample {
public:
  QosWifiExample();
  void Run(int argc, char *argv[]);

private:
  uint32_t m_nPayloadSize;
  double m_distance;
  bool m_rtscts;
  double m_simTime;
  bool m_pcapEnabled;

  NodeContainer apNodes[4];
  NodeContainer staNodes[4];
  NetDeviceContainer apDevices[4];
  NetDeviceContainer staDevices[4];
  Ipv4InterfaceContainer apInterfaces[4];
  Ipv4InterfaceContainer staInterfaces[4];

  void SetupNetwork(uint32_t index, uint16_t channelNumber);
  void SetupApplications(uint32_t index);
  void PhyTxTrace(Ptr<const Packet> packet, double txopDuration);
};

QosWifiExample::QosWifiExample()
    : m_nPayloadSize(1000), m_distance(1.0), m_rtscts(false), m_simTime(10.0),
      m_pcapEnabled(false) {}

void QosWifiExample::Run(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.AddValue("payloadSize", "Payload size in bytes.", m_nPayloadSize);
  cmd.AddValue("distance", "Distance between nodes (meters).", m_distance);
  cmd.AddValue("rtscts", "Enable RTS/CTS.", m_rtscts);
  cmd.AddValue("simTime", "Simulation time in seconds.", m_simTime);
  cmd.AddValue("pcap", "Enable pcap output.", m_pcapEnabled);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold",
                     m_rtscts ? StringValue("0") : StringValue("999999"));

  for (uint32_t i = 0; i < 4; ++i) {
    SetupNetwork(i, 1 + i); // Different channels
  }

  for (uint32_t i = 0; i < 4; ++i) {
    SetupApplications(i);
  }

  if (m_pcapEnabled) {
    for (uint32_t i = 0; i < 4; ++i) {
      std::string prefix = "qos-wifi-" + std::to_string(i);
      apDevices[i].Get(0)->GetChannel()->GetObject<YansWifiChannel>()->AddTraceSink(
          "Rx", MakeBoundCallback(&PhyTxTrace, this));
      staDevices[i].Get(0)->GetChannel()->GetObject<YansWifiChannel>()->AddTraceSink(
          "Rx", MakeBoundCallback(&PhyTxTrace, this));
    }
  }

  Simulator::Stop(Seconds(m_simTime));
  Simulator::Run();

  for (uint32_t i = 0; i < 4; ++i) {
    UdpServerHelper serverHelper =
        UdpServerHelper::GetUdpServer(staInterfaces[i].GetAddress(0), 4000);
    UdpServer serverApp =
        DynamicCast<UdpServer>(serverHelper.GetServerApplication());
    double throughput =
        serverApp->GetReceived() * m_nPayloadSize * 8 / (m_simTime * 1e6);
    NS_LOG_UNCOND("Throughput AC" << (i == 0 || i == 1 ? "BE" : "VI")
                                 << " (Mbps): " << throughput);
  }

  Simulator::Destroy();
}

void QosWifiExample::SetupNetwork(uint32_t index, uint16_t channelNumber) {
  WifiHelper wifi;
  YansWifiPhyHelper phy;
  WifiMacHelper mac;
  Ssid ssid = Ssid("qos-network-" + std::to_string(index));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  wifi.SetStandard(WIFI_STANDARD_80211n_5MHz);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("HtMcs7"),
                               "ControlMode", StringValue("HtMcs0"));

  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing",
              BooleanValue(false));

  if (index == 0 || index == 1) {
    mac.SetQosAckPolicyForAc(AC_BE, Mac48Address::GetBroadcast(),
                             WifiMacHeader::NORMAL_ACK);
  } else {
    mac.SetQosAckPolicyForAc(AC_VI, Mac48Address::GetBroadcast(),
                             WifiMacHeader::NORMAL_ACK);
  }

  staNodes[index].Create(1);
  staDevices[index] = wifi.Install(phy, mac, staNodes[index]);

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  apNodes[index].Create(1);
  apDevices[index] = wifi.Install(phy, mac, apNodes[index]);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(m_distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNodes[index]);
  mobility.Install(staNodes[index]);

  InternetStackHelper stack;
  stack.Install(apNodes[index]);
  stack.Install(staNodes[index]);

  Ipv4AddressHelper address;
  address.SetBase("10.1." + std::to_string(index) + ".0", "255.255.255.0");
  apInterfaces[index] = address.Assign(apDevices[index]);
  staInterfaces[index] = address.Assign(staDevices[index]);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();
}

void QosWifiExample::SetupApplications(uint32_t index) {
  UdpServerHelper server(staInterfaces[index].GetAddress(0), 4000);
  ApplicationContainer serverApp = server.Install(staNodes[index]);
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(m_simTime));

  OnOffHelper client("ns3::UdpSocketFactory",
                     InetSocketAddress(staInterfaces[index].GetAddress(0), 4000));
  client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  client.SetAttribute("PacketSize", UintegerValue(m_nPayloadSize));
  client.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));

  ApplicationContainer clientApp = client.Install(apNodes[index]);
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(m_simTime));
}

void QosWifiExample::PhyTxTrace(Ptr<const Packet> packet, double txopDuration) {
  NS_LOG_UNCOND("TXOP Duration: " << txopDuration << " microseconds");
}

int main(int argc, char *argv[]) {
  QosWifiExample example;
  example.Run(argc, argv);
  return 0;
}