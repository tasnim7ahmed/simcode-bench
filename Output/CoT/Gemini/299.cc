#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer wifiNodes;
  wifiNodes.Create(2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, wifiNodes.Get(0));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, wifiNodes.Get(1));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiNodes);

  InternetStackHelper stack;
  stack.Install(wifiNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces;
  interfaces = address.Assign(staDevices);
  interfaces = address.Assign(apDevices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(wifiNodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
  echoClient.SetAttribute("PacketSize", UintegerValue(128));

  ApplicationContainer clientApps = echoClient.Install(wifiNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  phy.EnablePcap("wifi-example", apDevices.Get(0));

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}