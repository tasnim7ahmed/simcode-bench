#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DataCenterSimulation");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Enable logging
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  // Define simulation parameters
  uint32_t k = 4;            // Number of pods (and also switches per pod)
  uint32_t numServers = k * k * k / 4; // Total number of servers
  double simulationTime = 10.0; // Simulation time in seconds
  std::string animFile = "data-center.xml";  // NetAnim trace file

  NS_LOG_INFO("Creating topology");

  // Create nodes
  NodeContainer coreSwitches;
  coreSwitches.Create(k * k / 4);

  NodeContainer aggregationSwitches;
  aggregationSwitches.Create(k * k);

  NodeContainer edgeSwitches;
  edgeSwitches.Create(k * k);

  NodeContainer servers;
  servers.Create(numServers);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(coreSwitches);
  stack.Install(aggregationSwitches);
  stack.Install(edgeSwitches);
  stack.Install(servers);

  // Create channels and links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
  p2p.SetChannelAttribute("Delay", StringValue("1us"));

  // Core to Aggregation
  NetDeviceContainer coreAggDevices[static_cast<unsigned int>(k * k / 4)];
  for (uint32_t i = 0; i < k * k / 4; ++i) {
    for (uint32_t j = 0; j < k; ++j) {
      NodeContainer link(coreSwitches.Get(i), aggregationSwitches.Get(i * k + j));
      coreAggDevices[i].Add(p2p.Install(link));
    }
  }

  // Aggregation to Edge
  NetDeviceContainer aggEdgeDevices[static_cast<unsigned int>(k * k)];
  for (uint32_t i = 0; i < k * k; ++i) {
    for (uint32_t j = 0; j < k / 2; ++j) {
      NodeContainer link(aggregationSwitches.Get(i), edgeSwitches.Get((i / k) * k + j));
      aggEdgeDevices[i].Add(p2p.Install(link));
    }
  }

  // Edge to Servers
  NetDeviceContainer edgeServerDevices[static_cast<unsigned int>(k * k)];
  for (uint32_t i = 0; i < k * k; ++i) {
    for (uint32_t j = 0; j < k / 2; ++j) {
      NodeContainer link(edgeSwitches.Get(i), servers.Get(i * k / 2 + j));
      edgeServerDevices[i].Add(p2p.Install(link));
    }
  }

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.0.0", "255.255.0.0");

  Ipv4InterfaceContainer coreSwitchInterfaces;
  for (uint32_t i = 0; i < k * k / 4; ++i) {
      coreSwitchInterfaces.Add(address.Assign(coreAggDevices[i]));
      address.NewNetwork();
  }

  Ipv4InterfaceContainer aggregationSwitchInterfaces;
  for (uint32_t i = 0; i < k * k; ++i) {
      aggregationSwitchInterfaces.Add(address.Assign(aggEdgeDevices[i]));
      address.NewNetwork();
  }

  Ipv4InterfaceContainer edgeSwitchInterfaces;
  for (uint32_t i = 0; i < k * k; ++i) {
      edgeSwitchInterfaces.Add(address.Assign(edgeServerDevices[i]));
      address.NewNetwork();
  }

  Ipv4InterfaceContainer serverInterfaces;
  for(uint32_t i = 0; i < numServers; ++i){
      NetDeviceContainer serverDevice;
      serverDevice.Add(edgeServerDevices[i / (k/2)].Get(i % (k/2))); //Get the devices connected to server i
      serverInterfaces.Add(address.Assign(serverDevice));
      address.NewNetwork();
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create Applications (Traffic)
  uint16_t port = 9; // Discard port (RFC 863)

  for (uint32_t i = 0; i < numServers; ++i) {
    UdpServerHelper server(port);
    server.SetAttribute("ReceiveBufferSize", UintegerValue(65535));

    ApplicationContainer serverApps = server.Install(servers.Get(i));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime - 1));

    UdpClientHelper client(serverInterfaces.GetAddress(i), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000000));
    client.SetAttribute("Interval", TimeValue(Time("0.00001"))); //10 usec
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(servers.Get((i + 1) % numServers)); //Send to the next server
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime - 2));
  }

  // Enable Tracing for NetAnim
  AnimationInterface anim(animFile);
  anim.EnablePacketTagging();

  NS_LOG_INFO("Running simulation.");
  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();
  NS_LOG_INFO("Done.");

  return 0;
}