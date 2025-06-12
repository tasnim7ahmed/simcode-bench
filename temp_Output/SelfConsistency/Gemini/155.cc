#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <iostream>
#include <iomanip>

using namespace ns3;

// Define a global output stream for the CSV file
std::ofstream csvFile;

// Packet sink callback function to record data
static void
ReceivePacket(std::string context, Ptr<const Packet> p, const Address& addr)
{
  // Extract source and destination node IDs from the context string
  size_t delimiterPos = context.find("-");
  uint32_t sourceNodeId = std::stoul(context.substr(0, delimiterPos));
  uint32_t destinationNodeId = std::stoul(context.substr(delimiterPos + 1));

  // Get the current simulation time
  Time currentTime = Simulator::Now();

  // Write the data to the CSV file
  csvFile << sourceNodeId << ","
          << destinationNodeId << ","
          << p->GetSize() << ","
          << p->GetUid() << ","
          << Simulator::GetStartTime().GetSeconds() << ","
          << currentTime.GetSeconds() << std::endl;
}

int
main(int argc, char* argv[])
{
  // Enable logging (optional)
  // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  // Set the simulation time resolution
  Time::SetResolution(Time::NS);

  // Define simulation parameters
  double simulationTime = 10.0; // seconds
  std::string dataRate = "5Mbps";
  std::string delay = "2ms";

  // Create nodes
  NodeContainer nodes;
  nodes.Create(5); // Create 5 nodes

  // Create point-to-point channels
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
  pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

  // Create net devices
  NetDeviceContainer devices;
  for (int i = 1; i < 5; ++i) {
    NetDeviceContainer link = pointToPoint.Install(nodes.Get(0), nodes.Get(i));
    devices.Add(link.Get(0));
    devices.Add(link.Get(1));
  }

  // Install the internet stack
  InternetStackHelper internet;
  internet.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces;
  interfaces = address.Assign(devices);

  // Create TCP server applications on the peripheral nodes
  uint16_t port = 50000;
  ApplicationContainer servers;
  for (int i = 1; i < 5; ++i) {
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    servers.Add(sink.Install(nodes.Get(i)));

    // Set up callback for packet reception
    std::ostringstream oss;
    oss << 0 << "-" << i;  // Store source and destination ids in the context
    std::string context = oss.str();
    Config::ConnectWithoutContext("/NodeList/" + std::to_string(i) + "/$ns3::TcpL4Protocol/SocketList/0/DataRecv", MakeBoundCallback(&ReceivePacket, context));

    port++; // Increment port for each server
  }

  servers.Start(Seconds(1.0));

  // Create TCP client applications on the central node, sending to each peripheral node
  ApplicationContainer clients;
  port = 50000;
  for (int i = 1; i < 5; ++i) {
    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(2 * i - 1), port)); //use the device address on the peripheral node.
    source.SetAttribute("MaxBytes", UintegerValue(1000000)); // Set total data to send
    clients.Add(source.Install(nodes.Get(0)));
    port++;
  }
  clients.Start(Seconds(2.0));

  // Enable global static routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Open the CSV file for writing
  csvFile.open("star_topology_data.csv");
  csvFile << "SourceNode,DestinationNode,PacketSize,PacketUid,TransmissionTime,ReceptionTime" << std::endl;

  // Run the simulation
  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  // Close the CSV file
  csvFile.close();

  Simulator::Destroy();
  return 0;
}