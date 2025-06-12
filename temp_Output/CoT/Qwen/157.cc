#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologyUdpDataset");

class PacketLogger {
public:
  PacketLogger(const std::string& filename);
  void LogPacket(Ptr<const Packet> packet, uint32_t srcNode, uint32_t dstNode, const Address& from, const Address& to);

private:
  std::ofstream m_ofs;
};

PacketLogger::PacketLogger(const std::string& filename) {
  m_ofs.open(filename);
  m_ofs << "Source Node,Destination Node,Packet Size,Transmission Time,Reception Time\n";
}

void
PacketLogger::LogPacket(Ptr<const Packet> packet, uint32_t srcNode, uint32_t dstNode, const Address& from, const Address& to) {
  double txTime = 0.0;
  double rxTime = Simulator::Now().GetSeconds();
  uint32_t size = packet->GetSize();

  m_ofs << srcNode << "," << dstNode << "," << size << "," << txTime << "," << rxTime << "\n";
}

int
main(int argc, char* argv[]) {
  std::string csvFile = "ring_topology_dataset.csv";
  double simDuration = 10.0;

  CommandLine cmd(__FILE__);
  cmd.AddValue("csvFile", "Output CSV file name", csvFile);
  cmd.AddValue("simDuration", "Simulation duration in seconds", simDuration);
  cmd.Parse(argc, argv);

  Packet::EnablePrinting();

  // Create 4 nodes arranged in a ring
  NodeContainer nodes;
  nodes.Create(4);

  // Connect each pair of adjacent nodes with point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices[4];
  for (uint32_t i = 0; i < 4; ++i) {
    uint32_t j = (i + 1) % 4;
    devices[i] = p2p.Install(nodes.Get(i), nodes.Get(j));
  }

  // Install internet stack on all nodes
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  Ipv4InterfaceContainer interfaces[4];
  for (uint32_t i = 0; i < 4; ++i) {
    std::ostringstream subnet;
    subnet << "10." << i << ".0.0";
    address.SetBase(subnet.str().c_str(), "255.255.255.0");
    interfaces[i] = address.Assign(devices[i]);
  }

  // Enable global routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Set up UDP echo server on each node
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps;
  for (uint32_t i = 0; i < 4; ++i) {
    serverApps.Add(echoServer.Install(nodes.Get(i)));
  }
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(simDuration));

  // Set up UDP echo client applications on each node to send to the next node
  PacketLogger logger(csvFile);

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < 4; ++i) {
    uint32_t j = (i + 1) % 4;
    UdpEchoClientHelper echoClient(interfaces[j].GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(20));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer app = echoClient.Install(nodes.Get(i));
    clientApps.Add(app);

    // Trace packets
    Config::ConnectWithoutContext(
        "/NodeList/" + std::to_string(j) + "/ApplicationList/0/$ns3::UdpEchoServer/Socket/Tx",
        MakeCallback(&PacketLogger::LogPacket, &logger));
  }

  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(simDuration));

  // Enable logging
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}