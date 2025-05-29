#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/qos-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool verbose = false;
  bool tracing = false;
  std::string phyMode("OfdmRate6MbpsBW10MHz");
  double simulationTime = 10;
  double distance = 5;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell applications to log if set to true", verbose);
  cmd.AddValue("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue("phyMode", "Wifi phy mode", phyMode);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("distance", "Distance between nodes", distance);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  }

  NodeContainer staNodes;
  staNodes.Create(2);

  NodeContainer apNode;
  apNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);
  wifi.SetRemoteStationManager("ns3::AarfWifiManager");

  WifiMacHelper mac;

  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, staNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconGeneration", BooleanValue(true),
              "QosSupported", BooleanValue(true));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, apNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(distance),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(staNodes);
  mobility.Install(apNode);

  InternetStackHelper stack;
  stack.Install(apNode);
  stack.Install(staNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staNodeInterfaces;
  staNodeInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apNodeInterfaces;
  apNodeInterfaces = address.Assign(apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  UdpServerHelper echoServer(9);

  ApplicationContainer serverApps = echoServer.Install(apNode.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime - 1));

  UdpClientHelper echoClient(apNodeInterfaces.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1000000));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.001)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(staNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simulationTime - 2));

  if (tracing) {
    phy.EnablePcap("80211n-comp-block-ack", apDevices.Get(0));
  }

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}