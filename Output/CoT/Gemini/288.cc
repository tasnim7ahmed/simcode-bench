#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer staNodes;
  staNodes.Create(2);

  NodeContainer apNode;
  apNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211ac);

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, staNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconInterval", TimeValue(Seconds(0.05)));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, apNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
  mobility.Install(staNodes);

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNode);

  InternetStackHelper internet;
  internet.Install(apNode);
  internet.Install(staNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staNodeInterface;
  staNodeInterface = address.Assign(staDevices);
  Ipv4InterfaceContainer apNodeInterface;
  apNodeInterface = address.Assign(apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 5000;
  PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = sink.Install(staNodes.Get(0));
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(10.0));

  BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(staNodeInterface.GetAddress(0), port));
  source.SetAttribute("MaxBytes", UintegerValue(0));
  source.SetAttribute("SendSize", UintegerValue(1024));
  source.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  source.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  ApplicationContainer sourceApps = source.Install(staNodes.Get(1));
  sourceApps.Start(Seconds(2.0));
  sourceApps.Stop(Seconds(10.0));

  phy.EnablePcap("wifi-simulation", apDevices.Get(0));
  phy.EnablePcap("wifi-simulation", staDevices.Get(0));
  phy.EnablePcap("wifi-simulation", staDevices.Get(1));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}