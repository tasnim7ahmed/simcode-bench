#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool enableFlowMonitor = true;

  CommandLine cmd;
  cmd.AddValue("EnableFlowMonitor", "Enable flow monitor", enableFlowMonitor);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer staNodes;
  staNodes.Create(2);

  NodeContainer apNode;
  apNode.Create(1);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, staNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconInterval", TimeValue(Seconds(0.1)));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, apNode);

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

  UdpServerHelper server(9);
  ApplicationContainer serverApp = server.Install(staNodes.Get(1));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  UdpClientHelper client(staNodeInterface.GetAddress(1), 9);
  client.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp = client.Install(staNodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  AnimationInterface anim("wifi-simple.xml");
  anim.SetConstantPosition(apNode.Get(0), 20.0, 20.0);
  anim.SetConstantPosition(staNodes.Get(0), 10.0, 10.0);
  anim.SetConstantPosition(staNodes.Get(1), 30.0, 10.0);

  FlowMonitorHelper flowmon;
  if (enableFlowMonitor) {
    flowmon.InstallAll();
  }

  Simulator::Stop(Seconds(11.0));

  Simulator::Run();

  if (enableFlowMonitor) {
    flowmon.SerializeToXmlFile("wifi-simple.flowmon", false, true);
  }

  Simulator::Destroy();
  return 0;
}