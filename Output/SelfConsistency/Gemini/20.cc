#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/qos-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi80211nBlockAck");

int main(int argc, char *argv[]) {
  bool verbose = false;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if set to true", verbose);
  cmd.AddValue("tracing", "Enable pcap tracing", tracing);

  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  }

  // Disable fragmentation (for now)
  UdpClient::SetDefaultAttribute("MaxBytes", UintegerValue(1024));

  // Create nodes
  NodeContainer staNodes;
  staNodes.Create(2);

  NodeContainer apNode;
  apNode.Create(1);

  // Configure wireless channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  // Configure Wi-Fi
  WifiHelper wifi = WifiHelper::Default();
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);
  wifi.SetRemoteStationManager("ns3::AarfWifiManager");

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();

  // Configure AP
  Ssid ssid = Ssid("ns3-wifi");
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconInterval", TimeValue(Seconds(0.050)));

  NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

  // Configure STAs
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

  // Configure mobility
  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(staNodes);

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNode);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(apNode);
  stack.Install(staNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer apInterfaces = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Setup Applications
  uint16_t port = 9; // Discard port (RFC 863)

  UdpServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(apNode.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper echoClient(apInterfaces.GetAddress(0), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(100));
  echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(staNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Install ICMP echo application to AP to generate ICMP frames

  V4PingHelper ping(staInterfaces.GetAddress(0));
  ping.SetAttribute("Verbose", BooleanValue(verbose));
  ping.SetAttribute("Interval", TimeValue(Seconds(1)));
  ping.SetAttribute("Size", UintegerValue(1024));
  ApplicationContainer pingApps = ping.Install(apNode.Get(0));
  pingApps.Start(Seconds(3.0));
  pingApps.Stop(Seconds(9.0));


  // Enable Block Ack
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/BlockAckThreshold", UintegerValue (1));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/BlockAckInactivityTimeout", TimeValue (Seconds (2)));

  // Enable Tracing
  if (tracing) {
    phy.EnablePcap("wifi-80211n-block-ack", apDevice.Get(0));
  }

  // Run simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  Simulator::Destroy();
  return 0;
}