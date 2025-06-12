#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AodvManetSimulation");

int main(int argc, char *argv[]) {

  bool enablePcap = false;
  double simulationTime = 10.0;
  std::string traceFile = "aodv-manet.ns_movements";
  std::string animFile = "aodv-manet.anim.xml";

  CommandLine cmd(__FILE__);
  cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("traceFile", "Mobility trace file", traceFile);
  cmd.AddValue("animFile", "NetAnim animation file", animFile);
  cmd.Parse(argc, argv);

  LogComponent::SetLevel(AodvHelper::GetTypeId(), LogLevel(LOG_LEVEL_ALL | LOG_PREFIX | LOG_FUNCTION | LOG_TIME));

  NodeContainer nodes;
  nodes.Create(5);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());

  wifi.SetRemoteStationManager("ns3::AarfWifiManager");

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
  wifiMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper(aodv);
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("250"),
                                  "Y", StringValue("250"),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=200.0]"));

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", StringValue("1s"),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                              "Bounds", StringValue("0|500|0|500"));
  mobility.Install(nodes);

  // Create Applications (UDP Echo client/server)
  uint16_t port = 9; // Discard port (RFC 863)

  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(4));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime - 1));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(4), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simulationTime - 2));

  if (enablePcap) {
    wifiPhy.EnablePcapAll("aodv-manet");
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Enable Tracing for Mobility (optional)
  //AsciiTraceHelper ascii;
  //mobility.EnableAsciiAll(ascii.CreateFileStream(traceFile));

  //Enable NetAnim
  AnimationInterface anim(animFile);
  anim.SetMaxPktsPerTraceFile(500000);

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (auto i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
    std::cout << "  Jitter Sum: " << i->second.jitterSum << "\n";
  }

  Simulator::Destroy();

  return 0;
}