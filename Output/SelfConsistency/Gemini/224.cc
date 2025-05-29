#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/udp.h"
#include "ns3/random-waypoint-mobility-model.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AodvManetSimulation");

int main(int argc, char *argv[]) {
  // Enable command line arguments
  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Set simulation parameters
  uint32_t numNodes = 10;
  double simulationTime = 20.0;
  double interval = 1.0; // seconds

  // Create nodes
  NodeContainer nodes;
  nodes.Create(numNodes);

  // Install AODV routing protocol
  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper(aodv);
  internet.Install(nodes);

  // Set mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
  mobility.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(nodes);

  // Create UDP client applications
  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < numNodes; ++i) {
    Ptr<Node> source = nodes.Get(i);
    Ptr<Application> app = CreateObject<Application>();

    // Install a sending application on node i
    TypeId tid = TypeId::LookupByName("ns3::UdpClient");
    Ptr<UdpClient> client = CreateObject<UdpClient>();
    client->SetTypeId(tid);
    AddressValue remoteAddress;
    remoteAddress.Set(InetSocketAddress(interfaces.GetAddress(0), 9)); // Destination address and port are placeholders
    client->SetAttribute("Remote", remoteAddress);
    client->SetAttribute("Port", UintegerValue(9));  // Port is a placeholder

    source->AddApplication(client);
    client->SetStartTime(Seconds(1.0));
    client->SetStopTime(Seconds(simulationTime));

    clientApps.Add(client);

    // Schedule a function to change destination dynamically
    Simulator::Schedule(Seconds(1.0), [i, numNodes, &nodes, &interfaces, simulationTime]() {
      for (double time = 1.0; time < simulationTime; time += interval) {
        Simulator::Schedule(Seconds(time), [i, numNodes, &nodes, &interfaces]() {
          // Select a random destination (excluding itself)
          uint32_t destinationId;
          do {
            destinationId = rand() % numNodes;
          } while (destinationId == i);

          Ptr<Node> source = nodes.Get(i);

          for (uint32_t appIndex = 0; appIndex < source->GetNApplications(); ++appIndex) {
             Ptr<Application> app = source->GetApplication(appIndex);
              TypeId tid = app->GetTypeId();
              if(tid.GetName() == "ns3::UdpClient"){

                 Ptr<UdpClient> client = DynamicCast<UdpClient>(app);
                 if(client){
                    Ipv4Address destIp = interfaces.GetAddress(destinationId);
                    AddressValue remoteAddress;
                    remoteAddress.Set(InetSocketAddress(destIp, 9));

                    client->SetAttribute("Remote", remoteAddress);
                    NS_LOG_INFO("Node " << i << " sending to " << destinationId << " at time " << Simulator::Now().GetSeconds());

                 }
              }

          }
        });
      }
    });
  }

  // Create UDP server applications
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes);
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(simulationTime));

  // Enable flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Run simulation
  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  // Print flow statistics
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
  }

  Simulator::Destroy();
  return 0;
}