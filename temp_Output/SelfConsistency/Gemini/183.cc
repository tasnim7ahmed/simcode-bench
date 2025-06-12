#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/olsr-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiNetanimExample");

int main(int argc, char *argv[]) {

  bool verbose = false;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if true", verbose);
  cmd.AddValue("tracing", "Enable pcap tracing", tracing);

  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  }

  // Disable fragmentation at IP level
  GlobalValue::Bind("Packet::EnableFragmentatation", BooleanValue(false));

  // Create Nodes
  NodeContainer apNodes;
  apNodes.Create(2);

  NodeContainer staNodes;
  staNodes.Create(4);

  // Configure Wifi
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());

  // Configure MAC layer for AP
  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconInterval", TimeValue(Seconds(0.1)));

  NetDeviceContainer apDevices = wifi.Install(wifiPhy, mac, apNodes);

  // Configure MAC layer for STA
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices = wifi.Install(wifiPhy, mac, staNodes);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

  mobility.Install(apNodes);
  mobility.Install(staNodes);

  // Internet stack
  InternetStackHelper internet;
  internet.Install(apNodes);
  internet.Install(staNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces = ipv4.Assign(apDevices);
  Ipv4InterfaceContainer staInterfaces = ipv4.Assign(staDevices);

  // Create Applications (UDP Echo)
  UdpEchoServerHelper echoServer(9);

  ApplicationContainer serverApps = echoServer.Install(apNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(apInterfaces.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(staNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  //Second Client
  UdpEchoServerHelper echoServer2(9);
  ApplicationContainer serverApps2 = echoServer2.Install(apNodes.Get(1));
  serverApps2.Start(Seconds(1.0));
  serverApps2.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient2(apInterfaces.GetAddress(2), 9);
  echoClient2.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient2.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps2 = echoClient2.Install(staNodes.Get(1));
  clientApps2.Start(Seconds(2.0));
  clientApps2.Stop(Seconds(10.0));

  // Enable Tracing
  if (tracing) {
    wifiPhy.EnablePcapAll("wifi-netanim");
  }

  // Animation
  AnimationInterface anim("wifi-netanim.xml");
  anim.SetConstantPosition(apNodes.Get(0), 5.0, 5.0);
  anim.SetConstantPosition(apNodes.Get(1), 5.0, 15.0);
  anim.SetConstantPosition(staNodes.Get(0), 1.0, 1.0);
  anim.SetConstantPosition(staNodes.Get(1), 1.0, 11.0);
  anim.SetConstantPosition(staNodes.Get(2), 9.0, 1.0);
  anim.SetConstantPosition(staNodes.Get(3), 9.0, 11.0);

  // Run Simulation
  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}