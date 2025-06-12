#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer sensors;
  sensors.Create(5);
  NodeContainer baseStation;
  baseStation.Create(1);
  NodeContainer allNodes;
  allNodes.Add(sensors);
  allNodes.Add(baseStation);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer sensorDevices = csma.Install(sensors);
  NetDeviceContainer baseStationDevice = csma.Install(baseStation);
  NetDeviceContainer allDevices;
  allDevices.Add(sensorDevices);
  allDevices.Add(baseStationDevice);

  InternetStackHelper stack;
  stack.Install(allNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer sensorInterfaces = address.Assign(sensorDevices);
  Ipv4InterfaceContainer baseStationInterface = address.Assign(baseStationDevice);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;

  UdpServerHelper echoServer(port);
  echoServer.SetAttribute("MaxPacketSize", UintegerValue(1500));
  ApplicationContainer serverApps = echoServer.Install(baseStation.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper echoClient(baseStationInterface.GetAddress(0), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < sensors.GetN(); ++i) {
    clientApps.Add(echoClient.Install(sensors.Get(i)));
  }

  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  AnimationInterface anim("wsm-animation.xml");
  anim.SetConstantPosition(baseStation.Get(0), 10.0, 10.0);
  for (uint32_t i = 0; i < sensors.GetN(); ++i) {
    anim.SetConstantPosition(sensors.Get(i), 20.0 + i * 5, 10.0);
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (auto i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    if (t.destinationAddress == baseStationInterface.GetAddress(0)) {
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Packet Delivery Ratio: " << (double)i->second.rxBytes / (double)i->second.txBytes << "\n";
      std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
      std::cout << "  Mean Delay: " << i->second.delaySum / i->second.rxPackets << "\n";
    }
  }

  Simulator::Destroy();
  return 0;
}