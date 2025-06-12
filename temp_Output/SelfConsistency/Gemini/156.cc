#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyTcp");

int main() {
  LogComponent::SetAttribute("UdpClient", "Interval", StringValue("0.01"));

  // Enable logging
  LogComponent::Enable("StarTopologyTcp");

  // Define simulation parameters
  uint32_t numNodes = 5; // One central node + 4 peripheral nodes
  double simulationTime = 10.0; // Simulation time in seconds
  std::string dataRate = "5Mbps";
  std::string delay = "2ms";
  uint16_t port = 9; // Port for TCP communication
  uint32_t packetSize = 1024; // Size of packets in bytes

  // Create nodes
  NodeContainer nodes;
  nodes.Create(numNodes);

  // Create point-to-point channels
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
  pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

  // Create net devices and channels
  NetDeviceContainer devices[4];
  for (uint32_t i = 1; i < numNodes; ++i) {
    NetDeviceContainer link = pointToPoint.Install(nodes.Get(0), nodes.Get(i));
    devices[i-1] = link;
  }

  // Install internet stack
  InternetStackHelper internet;
  internet.Install(nodes);

  // Assign IPv4 addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces[4];
  for (uint32_t i = 0; i < 4; ++i)
  {
    Ipv4InterfaceContainer interface = address.Assign(devices[i]);
    interfaces[i] = interface;
    address.NewNetwork();
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create TCP server on node 1
  TcpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(nodes.Get(1));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(simulationTime));

   // Create TCP client on node 0
  TcpClientHelper client(interfaces[0].GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(0));
  client.SetAttribute("PacketSize", UintegerValue(packetSize));
  client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  ApplicationContainer clientApp = client.Install(nodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(simulationTime));

  // Create TCP server on node 2
  TcpServerHelper server2(port+1);
  ApplicationContainer serverApp2 = server2.Install(nodes.Get(2));
  serverApp2.Start(Seconds(1.0));
  serverApp2.Stop(Seconds(simulationTime));

   // Create TCP client on node 0
  TcpClientHelper client2(interfaces[1].GetAddress(1), port+1);
  client2.SetAttribute("MaxPackets", UintegerValue(0));
  client2.SetAttribute("PacketSize", UintegerValue(packetSize));
  client2.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  ApplicationContainer clientApp2 = client2.Install(nodes.Get(0));
  clientApp2.Start(Seconds(2.0));
  clientApp2.Stop(Seconds(simulationTime));

  // Create TCP server on node 3
  TcpServerHelper server3(port+2);
  ApplicationContainer serverApp3 = server3.Install(nodes.Get(3));
  serverApp3.Start(Seconds(1.0));
  serverApp3.Stop(Seconds(simulationTime));

   // Create TCP client on node 0
  TcpClientHelper client3(interfaces[2].GetAddress(1), port+2);
  client3.SetAttribute("MaxPackets", UintegerValue(0));
  client3.SetAttribute("PacketSize", UintegerValue(packetSize));
  client3.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  ApplicationContainer clientApp3 = client3.Install(nodes.Get(0));
  clientApp3.Start(Seconds(2.0));
  clientApp3.Stop(Seconds(simulationTime));

    // Create TCP server on node 4
  TcpServerHelper server4(port+3);
  ApplicationContainer serverApp4 = server4.Install(nodes.Get(4));
  serverApp4.Start(Seconds(1.0));
  serverApp4.Stop(Seconds(simulationTime));

   // Create TCP client on node 0
  TcpClientHelper client4(interfaces[3].GetAddress(1), port+3);
  client4.SetAttribute("MaxPackets", UintegerValue(0));
  client4.SetAttribute("PacketSize", UintegerValue(packetSize));
  client4.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  ApplicationContainer clientApp4 = client4.Install(nodes.Get(0));
  clientApp4.Start(Seconds(2.0));
  clientApp4.Stop(Seconds(simulationTime));


  // Set up tracing to capture packet information
  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll(ascii.CreateFileStream("star-topology-tcp.tr"));

  // Run the simulation
  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  // Parse the trace file and store the data in a CSV file
  std::ifstream traceFile("star-topology-tcp.tr");
  std::ofstream dataFile("star-topology-tcp.csv");

  // Write CSV header
  dataFile << "Source Node,Destination Node,Packet Size,Transmission Time,Reception Time" << std::endl;

  std::string line;
  while (std::getline(traceFile, line)) {
    if (line.find("TCP") != std::string::npos && line.find("SEND") != std::string::npos) {
      // Extract relevant information from the trace line
      size_t srcNodePos = line.find("Node=");
      size_t dstNodePos = line.find("-> Node=");
      size_t timePos = line.find("Time=");
      size_t packetSizePos = line.find("len=");

      if (srcNodePos != std::string::npos && dstNodePos != std::string::npos && timePos != std::string::npos && packetSizePos != std::string::npos) {
        int srcNode = std::stoi(line.substr(srcNodePos + 5, dstNodePos - srcNodePos - 8));
        int dstNode = std::stoi(line.substr(dstNodePos + 8, timePos - dstNodePos - 11));
        double txTime = std::stod(line.substr(timePos + 5, packetSizePos - timePos - 8));
        int packetSize = std::stoi(line.substr(packetSizePos + 4));

        // Find corresponding receive event
        std::string rxLine;
        double rxTime = -1.0;
        std::ifstream traceFile2("star-topology-tcp.tr");
        while (std::getline(traceFile2, rxLine)) {
          if (rxLine.find("TCP") != std::string::npos && rxLine.find("RECV") != std::string::npos) {
             size_t rxSrcNodePos = rxLine.find("Node=");
              size_t rxDstNodePos = rxLine.find("-> Node=");
              size_t rxTimePos = rxLine.find("Time=");
              size_t rxPacketSizePos = rxLine.find("len=");

              if(rxSrcNodePos != std::string::npos && rxDstNodePos != std::string::npos && rxTimePos != std::string::npos && rxPacketSizePos != std::string::npos)
              {
                int rxSrcNode = std::stoi(rxLine.substr(rxSrcNodePos + 5, rxDstNodePos - rxSrcNodePos - 8));
                int rxDstNode = std::stoi(rxLine.substr(rxDstNodePos + 8, rxTimePos - rxDstNodePos - 11));
                double rxTimeTemp = std::stod(rxLine.substr(rxTimePos + 5, rxPacketSizePos - rxTimePos - 8));
                int rxPacketSize = std::stoi(rxLine.substr(rxPacketSizePos + 4));

                  if(rxSrcNode == srcNode && rxDstNode == dstNode && rxPacketSize == packetSize && rxTimeTemp > txTime)
                  {
                    rxTime = rxTimeTemp;
                    break;
                  }
              }
          }
        }
        traceFile2.close();

        if (rxTime != -1.0) {
          // Write data to CSV file
          dataFile << srcNode << "," << dstNode << "," << packetSize << "," << txTime << "," << rxTime << std::endl;
        }
      }
    }
  }

  // Close files
  traceFile.close();
  dataFile.close();

  Simulator::Destroy();
  return 0;
}