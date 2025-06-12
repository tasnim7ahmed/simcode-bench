#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include <fstream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkTopologySimulation");

class NodePosition {
public:
  double x;
  double y;
};

int main(int argc, char *argv[]) {
  LogComponentEnable("NetworkTopologySimulation", LOG_LEVEL_INFO);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  std::string matrixFile = "adjacency_matrix.txt";
  std::string coordFile = "node_coordinates.txt";
  std::string animFile = "animation.xml";
  uint16_t port = 9;
  Time interPacketInterval = Seconds(0.001);
  Time simulationDuration = Seconds(10.0);

  CommandLine cmd;
  cmd.AddValue("matrixFile", "Adjacency matrix input file name", matrixFile);
  cmd.AddValue("coordFile", "Node coordinates input file name", coordFile);
  cmd.Parse(argc, argv);

  // Read adjacency matrix
  std::ifstream matIn(matrixFile);
  uint32_t n;
  matIn >> n;
  std::vector<std::vector<uint32_t>> adjMatrix(n, std::vector<uint32_t>(n));
  for (uint32_t i = 0; i < n; ++i) {
    for (uint32_t j = 0; j < n; ++j) {
      matIn >> adjMatrix[i][j];
    }
  }

  // Read node positions
  std::ifstream posIn(coordFile);
  std::vector<NodePosition> positions(n);
  for (uint32_t i = 0; i < n; ++i) {
    posIn >> positions[i].x >> positions[i].y;
  }

  NodeContainer nodes;
  nodes.Create(n);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices[n];
  for (uint32_t i = 0; i < n; ++i) {
    for (uint32_t j = i + 1; j < n; ++j) {
      if (adjMatrix[i][j] > 0) {
        NodeContainer pair = NodeContainer(nodes.Get(i), nodes.Get(j));
        devices[i].Add(p2p.Install(pair).Get(0));
        devices[j].Add(p2p.Install(pair).Get(1));
      }
    }
  }

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer interfaces[n];
  uint32_t devIdx = 0;
  for (uint32_t i = 0; i < n; ++i) {
    for (uint32_t j = i + 1; j < n; ++j) {
      if (adjMatrix[i][j] > 0) {
        std::ostringstream subnet;
        subnet << "10." << devIdx++ << ".0.0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i].Add(address.Assign(devices[i]));
        interfaces[j].Add(address.Assign(devices[j]));
      }
    }
  }

  UdpServerHelper server(port);
  ApplicationContainer servers[n];
  for (uint32_t i = 0; i < n; ++i) {
    servers[i] = server.Install(nodes.Get(i));
    servers[i].Start(Seconds(0.0));
    servers[i].Stop(simulationDuration);
  }

  UdpClientHelper client;
  ApplicationContainer clients;
  for (uint32_t src = 0; src < n; ++src) {
    for (uint32_t dst = 0; dst < n; ++dst) {
      if (src != dst) {
        client = UdpClientHelper(interfaces[dst].GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        client.SetAttribute("Interval", TimeValue(interPacketInterval));
        client.SetAttribute("PacketSize", UintegerValue(1024));

        clients = client.Install(nodes.Get(src));
        clients.Start(Seconds(1.0));
        clients.Stop(simulationDuration);
      }
    }
  }

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  for (uint32_t i = 0; i < n; ++i) {
    Ptr<MobilityModel> mob = nodes.Get(i)->GetObject<MobilityModel>();
    NS_ASSERT(mob != nullptr);
    mob->SetPosition(Vector(positions[i].x, positions[i].y, 0.0));
  }

  AnimationInterface anim(animFile);
  for (uint32_t i = 0; i < n; ++i) {
    anim.UpdateNodeDescription(nodes.Get(i), "Node_" + std::to_string(i));
    anim.UpdateNodeColor(nodes.Get(i), 255, 0, 0);
  }

  Simulator::Stop(simulationDuration);
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}