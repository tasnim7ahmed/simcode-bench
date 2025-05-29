#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/udp-client.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMobileSimulation");

int main(int argc, char *argv[]) {
  bool verbose = false;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application containers to log if set to true", verbose);
  cmd.AddValue("tracing", "Enable pcap tracing", tracing);

  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  }

  NodeContainer staNodes;
  staNodes.Create(2);

  NodeContainer apNode;
  apNode.Create(1);

  // Configure the PHY and MAC layers for 802.11n 5 GHz
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid("ns-3-ssid");
  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid),
                  "BeaconGeneration", BooleanValue(true),
                  "BeaconInterval", TimeValue(Seconds(1.0)));

  NetDeviceContainer apDevices = wifi.Install(wifiPhy, wifiMac, apNode);

  // Mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                "X", StringValue("50.0"),
                                "Y", StringValue("50.0"),
                                "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
  mobility.Install(staNodes);

  MobilityHelper apMobility;
  apMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  apMobility.Install(apNode);
  apNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(50, 50, 0));

  // Install the TCP/IP stack
  InternetStackHelper internet;
  internet.Install(staNodes);
  internet.Install(apNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staNodeInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apNodeInterfaces = address.Assign(apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create UDP server on the access point
  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(apNode);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // Create UDP client on the mobile nodes
  OnOffHelper client("ns3::UdpSocketFactory", Address(InetSocketAddress(apNodeInterfaces.GetAddress(0), 9)));
  client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  client.SetAttribute("DataRate", StringValue("1Mbps")); // Adjust as needed

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
    clientApps.Add(client.Install(staNodes.Get(i)));
  }

  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  if (tracing) {
    wifiPhy.EnablePcap("wifi-mobile", apDevices);
    wifiPhy.EnablePcap("wifi-mobile", staDevices);
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}