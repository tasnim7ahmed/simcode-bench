#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/http-client.h"
#include "ns3/http-server.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("DsssRate11Mbps"),
                               "RtsCtsThreshold", UintegerValue(2200));

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, nodes.Get(1));

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid),
              "BeaconInterval", TimeValue(MilliSeconds(102)),
              "DtimPeriod", UintegerValue(3));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, nodes.Get(0));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(1),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(apDevices);
  interfaces.Add(address.Assign(staDevices).Get(0));

  HttpServerHelper serverHelper(80);
  ApplicationContainer serverApp = serverHelper.Install(nodes.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  HttpClientHelper clientHelper("10.1.1.1", 80);
  clientHelper.SetAttribute("RequestTotal", UintegerValue(1));
  clientHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));

  ApplicationContainer clientApp = clientHelper.Install(nodes.Get(1));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}