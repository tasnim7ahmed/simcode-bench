#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/olsr-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/buildings-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetSimulation");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  // Configure mobility for the vehicle (node 0)
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(nodes.Get(0));
  Ptr<ConstantVelocityMobilityModel> vehicleMobility = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
  vehicleMobility->SetVelocity(Vector(10, 0, 0));  // Move along the x-axis

  // Configure static mobility for the RSU (node 1)
  MobilityHelper staticMobility;
  staticMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  staticMobility.Install(nodes.Get(1));
  nodes.Get(1)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(50, 0, 0));

  // Configure WAVE (802.11p)
  YansWifiChannelHelper wifiChannelHelper = YansWifiChannelHelper::Default();
  YansWifiPhyHelper wifiPhyHelper = YansWifiPhyHelper::Default();
  wifiPhyHelper.SetChannel(wifiChannelHelper.Create());
  WaveMacHelper waveMacHelper = WaveMacHelper::Default();
  Wifi80211pHelper wifi80211pHelper;
  NetDeviceContainer devices = wifi80211pHelper.Install(wifiPhyHelper, waveMacHelper, nodes);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // UDP Echo client/server application
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Simulation time
  Simulator::Stop(Seconds(11.0));

  // Enable Tracing
  // AsciiTraceHelper ascii;
  // wifiPhyHelper.EnableAsciiAll(ascii.CreateFileStream("vanet-simulation.tr"));
  // wifiPhyHelper.EnablePcapAll("vanet-simulation");

  AnimationInterface anim("vanet-simulation.xml");
  anim.SetConstantPosition(nodes.Get(1), 50.0, 0.0);

  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
  }

  Simulator::Destroy();

  return 0;
}