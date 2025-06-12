#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <memory>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsr-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetRoutingComparison");

int main(int argc, char *argv[]) {
  // Set up command line arguments
  bool enableFlowMonitor = false;
  bool enableMobilityTrace = false;
  std::string routingProtocol = "AODV";
  uint32_t numNodes = 50;
  double simulationTime = 200;
  double maxSpeed = 20;
  double txPowerDbm = 7.5;

  CommandLine cmd;
  cmd.AddValue("enableFlowMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.AddValue("enableMobilityTrace", "Enable Mobility Trace", enableMobilityTrace);
  cmd.AddValue("routingProtocol", "Routing Protocol [AODV|DSDV|OLSR|DSR]", routingProtocol);
  cmd.AddValue("numNodes", "Number of nodes", numNodes);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("maxSpeed", "Maximum speed for Random Waypoint model", maxSpeed);
  cmd.AddValue("txPowerDbm", "Transmit power in dBm", txPowerDbm);

  cmd.Parse(argc, argv);

  // Log component enable
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(numNodes);

  // Install wireless devices
  WifiHelper wifi;
  WifiMacHelper wifiMac;
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());

  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  // Set transmit power
  wifiPhy.Set("TxPowerStart", DoubleValue(txPowerDbm));
  wifiPhy.Set("TxPowerEnd", DoubleValue(txPowerDbm));

  // Install mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=1500.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=300.0]"));
  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(maxSpeed) + "]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"));
  mobility.Install(nodes);

  // Install routing protocol
  InternetStackHelper internet;
  if (routingProtocol == "AODV") {
    AodvHelper aodv;
    internet.Add(aodv, nodes);
  } else if (routingProtocol == "DSDV") {
    DsdvHelper dsdv;
    internet.Add(dsdv, nodes);
  } else if (routingProtocol == "OLSR") {
    OlsrHelper olsr;
    internet.Add(olsr, nodes);
  } else if (routingProtocol == "DSR") {
    DsrHelper dsr;
    DsrMainHelper dsrMain;
    internet.Add(dsr, nodes);
    internet.Add(dsrMain, nodes);
  } else {
    std::cerr << "Error: Routing protocol " << routingProtocol << " not supported." << std::endl;
    return 1;
  }

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // Create UDP applications
  uint16_t port = 9;
  ApplicationContainer clientApps;
  ApplicationContainer serverApps;

  for (uint32_t i = 0; i < 10; ++i) {
    // Create server (sink)
    UdpServerHelper server(port);
    serverApps.Add(server.Install(nodes.Get(i)));

    // Create client (source)
    UdpClientHelper client(interfaces.GetAddress(i), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    // Schedule client to start at a random time between 50 and 51 seconds
    Ptr<UniformRandomVariable> startTimeSeconds = CreateObject<UniformRandomVariable>();
    startTimeSeconds->SetAttribute("Min", DoubleValue(50.0));
    startTimeSeconds->SetAttribute("Max", DoubleValue(51.0));
    Time startTime = Seconds(startTimeSeconds->GetValue());

    clientApps.Add(client.Install(nodes.Get(i + numNodes / 2))); // Send to a different node

    serverApps.Start(Seconds(0.0));
    clientApps.Start(startTime);

    port++;
  }

  // Create output file
  std::ofstream ascii;
  ascii.open("manet-routing-comparison.csv");
  ascii << "Time,NodeId,BytesReceived\n";

  // Add trace sinks
  for (uint32_t i = 0; i < numNodes; ++i) {
    Ptr<Node> node = nodes.Get(i);
    Ptr<Application> app = serverApps.Get(i % 10); // Assuming that there are 10 server applications
    Ptr<UdpServer> server = DynamicCast<UdpServer>(app);

    if (server) {
      server->TraceConnectWithoutContext("Rx", MakeBoundCallback(&
        [](std::ofstream *stream, Time t, Ptr < Socket > socket, Ptr < Packet > packet, const Address & from) {
          * stream << t.GetSeconds() << "," << socket->GetNode()->GetId() << "," << packet->GetSize() << "\n";
        }, & ascii));
    }
  }

  // Enable flow monitor
  FlowMonitorHelper flowmon;
  if (enableFlowMonitor) {
    flowmon.InstallAll();
  }

  // Enable mobility trace
  if (enableMobilityTrace) {
    MobilityModel::EnableAsciiAll(
        "manet-routing-comparison-mobility.mob");
  }

  // Enable NetAnim
  AnimationInterface anim("manet-routing-comparison.xml");

  // Run simulation
  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  // Print flow monitor statistics
  if (enableFlowMonitor) {
    flowmon.SerializeToXmlFile("manet-routing-comparison-flowmon.xml", false, true);
  }

  Simulator::Destroy();
  ascii.close();

  return 0;
}