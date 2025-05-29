#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTcpClientServer");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetLevel(
      "PacketSink", LOG_LEVEL_INFO);  // Enable logging for PacketSink
  LogComponent::SetLevel("OnOffApplication", LOG_LEVEL_INFO);

  NodeContainer wifiNodes;
  wifiNodes.Create(6);

  NodeContainer clientNodes;
  clientNodes.Add(wifiNodes.Get(0));
  clientNodes.Add(wifiNodes.Get(1));
  clientNodes.Add(wifiNodes.Get(2));
  clientNodes.Add(wifiNodes.Get(3));
  clientNodes.Add(wifiNodes.Get(4));

  NodeContainer serverNode;
  serverNode.Add(wifiNodes.Get(5));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  Ssid ssid = Ssid("wifi-network");
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing",
              BooleanValue(false));

  NetDeviceContainer clientDevices = wifi.Install(phy, mac, clientNodes);

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices = wifi.Install(phy, mac, serverNode);

  InternetStackHelper stack;
  stack.Install(wifiNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer clientInterfaces = address.Assign(clientDevices);
  Ipv4InterfaceContainer apInterface = address.Assign(apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;  // well-known echo port number

  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                              InetSocketAddress(apInterface.GetAddress(0), port));
  ApplicationContainer sinkApp = sinkHelper.Install(serverNode.Get(0));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  for (uint32_t i = 0; i < clientNodes.GetN(); ++i) {
    OnOffHelper onoff("ns3::TcpSocketFactory",
                      InetSocketAddress(apInterface.GetAddress(0), port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mb/s")));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = onoff.Install(clientNodes.Get(i));
    clientApp.Start(Seconds(2.0 + i * 0.2));
    clientApp.Stop(Seconds(9.0));
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}