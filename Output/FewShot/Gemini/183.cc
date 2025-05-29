#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  bool enable_visualization = true;

  NodeContainer apNodes;
  apNodes.Create(2);

  NodeContainer staNodes;
  staNodes.Create(4);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconInterval", TimeValue(Seconds(0.1)));

  NetDeviceContainer apDevices = wifi.Install(phy, mac, apNodes);

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

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

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(2.0),
                                "MinY", DoubleValue(2.0),
                                "DeltaX", DoubleValue(3.0),
                                "DeltaY", DoubleValue(3.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));

  mobility.Install(staNodes);

  InternetStackHelper stack;
  stack.Install(apNodes);
  stack.Install(staNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(staNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(staInterfaces.GetAddress(0), 9);
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(staNodes.Get(1));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  UdpEchoServerHelper echoServer2(9);
  ApplicationContainer serverApps2 = echoServer2.Install(staNodes.Get(2));
  serverApps2.Start(Seconds(1.0));
  serverApps2.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient2(staInterfaces.GetAddress(2), 9);
  echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient2.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps2 = echoClient2.Install(staNodes.Get(3));
  clientApps2.Start(Seconds(2.0));
  clientApps2.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));

  if (enable_visualization) {
    AnimationInterface anim("wifi-network.xml");
  }

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}