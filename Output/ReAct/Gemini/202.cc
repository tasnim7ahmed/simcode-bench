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
  CommandLine cmd;
  cmd.Parse(argc, argv);

  uint32_t numNodes = 6;
  double simulationTime = 10.0;
  double interval = 1.0;
  uint16_t port = 9;

  NodeContainer nodes;
  nodes.Create(numNodes);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_802154);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());

  NqosMac802154Helper mac = NqosMac802154Helper::Default();
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode",
                               StringValue("OfdmRate6Mbps"), "ControlMode",
                               StringValue("OfdmRate6Mbps"));

  NetDeviceContainer devices = wifi.Install(wifiPhy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::CirclePositionAllocator", "Radius",
                                StringValue("5.0"), "X", StringValue("0.0"),
                                "Y", StringValue("0.0"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(simulationTime));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(0), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(0xffffffff));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < numNodes; ++i) {
    clientApps.Add(echoClient.Install(nodes.Get(i)));
  }

  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(simulationTime));

  AnimationInterface anim("wmsn.xml");
  for (uint32_t i = 0; i < numNodes; ++i) {
    anim.UpdateNodeDescription(nodes.Get(i), "Node " + std::to_string(i));
    anim.UpdateNodeSize(nodes.Get(i), 1.0, 1.0);
  }

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier =
      DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i =
           stats.begin();
       i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> "
              << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Delay Sum:    " << i->second.delaySum << "\n";
  }

  Simulator::Destroy();
  return 0;
}