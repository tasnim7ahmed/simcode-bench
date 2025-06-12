#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

class ThroughputCalc
{
public:
  ThroughputCalc() : m_bytesRcvd(0) {}

  void RxCallback(Ptr<const Packet> packet, const Address &address)
  {
    m_bytesRcvd += packet->GetSize();
  }

  void ReportSeconds(double simTime)
  {
    std::cout << "Total bytes received: " << m_bytesRcvd << std::endl;
    double throughput = m_bytesRcvd * 8.0 / simTime / 1e6; // Mbps
    std::cout << "Average throughput at server: " << throughput << " Mbps" << std::endl;
  }

private:
  uint64_t m_bytesRcvd;
};

int main(int argc, char *argv[])
{
  uint32_t nSta = 3;
  double simTime = 10.0;

  CommandLine cmd;
  cmd.AddValue("nSta", "Number of WiFi STA devices", nSta);
  cmd.Parse(argc, argv);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(nSta);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  channel.AddPropagationLoss("ns3::FriisPropagationLossModel");
  channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  channel.Set("Delay", TimeValue(MilliSeconds(2))); // Set delay of 2ms

  Ptr<YansWifiChannel> chan = channel.Create();

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(chan);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211g); // 54 Mbps possible in 802.11g
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                              "DataMode", StringValue("ErpOfdmRate54Mbps"),
                              "ControlMode", StringValue("ErpOfdmRate24Mbps"));

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-wifi");

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, wifiStaNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (5.0),
                                "DeltaY", DoubleValue (10.0),
                                "GridWidth", UintegerValue (nSta+1),
                                "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install(wifiStaNodes);
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiStaNodes);
  stack.Install(wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign(apDevices);

  // UDP server on station 0
  uint16_t udpPort = 50000;
  UdpServerHelper server(udpPort);
  ApplicationContainer serverApp = server.Install(wifiStaNodes.Get(0));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(simTime));

  // UDP client on station 1, sending to station 0
  UdpClientHelper client(staInterfaces.GetAddress(0), udpPort);
  client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
  client.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
  client.SetAttribute("PacketSize", UintegerValue(1400));

  ApplicationContainer clientApp = client.Install(wifiStaNodes.Get(1));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(simTime));

  // Enable tracing
  phy.EnablePcapAll("wifi-multi-sta", true);
  AsciiTraceHelper ascii;
  phy.EnableAsciiAll(ascii.CreateFileStream("wifi-multi-sta.tr"));

  // Throughput measurement
  Ptr<UdpServer> udpServer = DynamicCast<UdpServer>(serverApp.Get(0));

  ThroughputCalc throughputCalc;
  Config::ConnectWithoutContext("/NodeList/" +
    std::to_string(wifiStaNodes.Get(0)->GetId()) +
    "/ApplicationList/0/$ns3::UdpServer/Rx",
    MakeCallback(&ThroughputCalc::RxCallback, &throughputCalc));

  Simulator::Schedule(Seconds(simTime),
                      &ThroughputCalc::ReportSeconds, &throughputCalc, simTime);

  Simulator::Stop(Seconds(simTime + 0.10));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}