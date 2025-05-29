#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiInfrastructureSimulation");

int main(int argc, char *argv[]) {
  bool enablePcap = false;

  CommandLine cmd;
  cmd.AddValue("EnablePcap", "Enable PCAP Tracing", enablePcap);
  cmd.Parse(argc, argv);

  // Create nodes: AP and STAs
  NodeContainer apNode;
  apNode.Create(1);
  NodeContainer staNodes;
  staNodes.Create(2);

  // Configure Wi-Fi PHY and MAC
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid("ns-3-ssid");
  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, apNode);

  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install(apNode);
  internet.Install(staNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create UDP echo server on AP
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(apNode.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // Create UDP echo client on STA
  UdpEchoClientHelper echoClient(apInterface.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer clientApps = echoClient.Install(staNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Mobility model: Place nodes in a grid layout
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  NodeContainer allNodes;
  allNodes.Add(apNode);
  allNodes.Add(staNodes);
  mobility.Install(allNodes);

  // Enable PCAP tracing
  if (enablePcap) {
    wifiPhy.EnablePcapAll("wifi-infrastructure");
  }

  // Enable FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Run simulation
  Simulator::Stop(Seconds(10.0));

  AnimationInterface anim("wifi-infrastructure.xml");
  anim.SetConstantPosition(apNode.Get(0), 10, 10);
  anim.SetConstantPosition(staNodes.Get(0), 15, 15);
  anim.SetConstantPosition(staNodes.Get(1), 5, 5);

  Simulator::Run();

  // Print FlowMonitor statistics
  monitor->CheckForLostPackets();
  Ptr<Iprobe> iprobe = monitor->GetIprobe();
  monitor->SerializeToXmlFile("wifi-infrastructure.flowmon", true, true);

  Simulator::Destroy();

  return 0;
}