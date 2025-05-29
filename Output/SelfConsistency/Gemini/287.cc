#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/mmwave-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MmWaveUdpExample");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Enable logging (optional)
  // LogComponentEnable("MmWaveUdpExample", LOG_LEVEL_INFO);

  // Create Nodes: gNB and UEs
  NodeContainer gnbNodes;
  gnbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(2);

  // Configure Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue("Time"),
                             "Time", StringValue("1s"),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"), // 1 m/s
                             "Bounds", StringValue("0,50,0,50"));
  mobility.Install(ueNodes);

  // Make gNB stationary
  MobilityHelper gnbMobility;
  gnbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  gnbMobility.Install(gnbNodes);
  gnbNodes.Get(0)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(25.0, 25.0, 0.0)); // Center of the area

  // Install mmWave NetDevices
  MmWaveHelper mmwaveHelper;
  NetDeviceContainer gnbDevs = mmwaveHelper.InstallGnb(gnbNodes);
  NetDeviceContainer ueDevs = mmwaveHelper.InstallUe(ueNodes);

  // Install the Internet stack
  InternetStackHelper internet;
  internet.Install(gnbNodes);
  internet.Install(ueNodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer gnbIpIface = ipv4.Assign(gnbDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevs);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create a UDP server on UE node 0
  UdpServerHelper server(5001);
  ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // Create a UDP client on UE node 1
  UdpClientHelper client(ueIpIface.GetAddress(0), 5001);
  client.SetAttribute("MaxPackets", UintegerValue(0)); // Unlimited packets
  client.SetAttribute("Interval", TimeValue(MilliSeconds(10))); // Send every 10ms
  client.SetAttribute("PacketSize", UintegerValue(1024)); // 1024 byte packets

  ApplicationContainer clientApps = client.Install(ueNodes.Get(1));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Enable PCAP tracing
  mmwaveHelper.EnablePcap("mmwave-udp-example", gnbDevs.Get(0));
  mmwaveHelper.EnablePcap("mmwave-udp-example", ueDevs.Get(0));
  mmwaveHelper.EnablePcap("mmwave-udp-example", ueDevs.Get(1));

    // Install FlowMonitor on all nodes
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Run the simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  // Output FlowMonitor results
    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
      {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
	  std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
	  std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
	  std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
	  std::cout << "  Lost Packets:   " << i->second.lostPackets << "\n";

      }

  Simulator::Destroy();

  return 0;
}