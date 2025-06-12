#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiMultiApUdpTest");

class StationInfo {
public:
  uint32_t totalBytesSent;
  StationInfo() : totalBytesSent(0) {}
  void TxCallback(Ptr<const Packet> packet) {
    totalBytesSent += packet->GetSize();
  }
};

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  // Enable logging
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  // Create nodes: 2 APs + multiple STAs
  NodeContainer apNodes;
  apNodes.Create(2);

  NodeContainer staNodes1, staNodes2;
  staNodes1.Create(3); // Group 1 connected to AP 0
  staNodes2.Create(3); // Group 2 connected to AP 1

  // Create channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  // Setup MAC and PHY for APs
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("HtMcs7"),
                               "ControlMode", StringValue("HtMcs0"));

  Ssid ssid1 = Ssid("wifi-network-1");
  Ssid ssid2 = Ssid("wifi-network-2");

  WifiMacHelper mac;
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid1),
              "BeaconInterval", TimeValue(Seconds(2.5)));

  NetDeviceContainer apDevices1 = wifi.Install(phy, mac, apNodes.Get(0));
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid2),
              "BeaconInterval", TimeValue(Seconds(2.5)));
  NetDeviceContainer apDevices2 = wifi.Install(phy, mac, apNodes.Get(1));

  // Setup MAC and PHY for STAs
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid1),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices1 = wifi.Install(phy, mac, staNodes1);

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid2),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices2 = wifi.Install(phy, mac, staNodes2);

  // Mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNodes);
  mobility.Install(staNodes1);
  mobility.Install(staNodes2);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(apNodes);
  stack.Install(staNodes1);
  stack.Install(staNodes2);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");

  Ipv4InterfaceContainer apInterfaces1 = address.Assign(apDevices1);
  Ipv4InterfaceContainer apInterfaces2 = address.Assign(apDevices2);

  Ipv4InterfaceContainer staInterfaces1 = address.Assign(staDevices1);
  Ipv4InterfaceContainer staInterfaces2 = address.Assign(staDevices2);

  // Connect all APs to a common server (e.g., on AP 0 node)
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
  p2p.SetChannelAttribute("Delay", StringValue("1ms"));

  NodeContainer serverNode = apNodes.Get(0); // Server hosted on AP 0
  NodeContainer csmaNodes;
  csmaNodes.Add(serverNode);
  csmaNodes.Add(apNodes.Get(1));

  NetDeviceContainer p2pDevices = p2p.Install(csmaNodes);
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces = address.Assign(p2pDevices);

  // Install UDP server
  UdpServerHelper server(9);
  ApplicationContainer serverApp = server.Install(serverNode);
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // Install UDP clients
  UdpClientHelper clientHelper;
  clientHelper.SetAttribute("RemoteAddress", Ipv4AddressValue(p2pInterfaces.GetAddress(0)));
  clientHelper.SetAttribute("RemotePort", UintegerValue(9));
  clientHelper.SetAttribute("MaxPackets", UintegerValue(0)); // infinite packets
  clientHelper.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
  clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < staNodes1.GetN(); ++i) {
    clientApps.Add(clientHelper.Install(staNodes1.Get(i)));
  }
  for (uint32_t i = 0; i < staNodes2.GetN(); ++i) {
    clientApps.Add(clientHelper.Install(staNodes2.Get(i)));
  }

  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Tracing
  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> asciiStream = asciiTraceHelper.CreateFileStream("wifi-multi-ap-udp.tr");
  phy.EnableAsciiAll(asciiStream);
  phy.EnablePcapAll("wifi-multi-ap-udp");

  // Output stats at end
  Simulator::Stop(Seconds(10.0));

  std::vector<StationInfo> stationInfos(staNodes1.GetN() + staNodes2.GetN());
  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpClient/Tx", MakeBoundCallback(&StationInfo::TxCallback, &stationInfos));

  Simulator::Run();

  for (size_t i = 0; i < stationInfos.size(); ++i) {
    NS_LOG_UNCOND("Station " << i << " sent total bytes: " << stationInfos[i].totalBytesSent);
  }

  Simulator::Destroy();
  return 0;
}