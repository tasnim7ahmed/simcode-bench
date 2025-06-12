#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FatTreeDataCenterSimulation");

class FatTreeHelper {
public:
  struct FatTreeNode {
    NodeContainer nodes;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;
  };

  FatTreeHelper(uint32_t k);

  void InstallStack(InternetStackHelper stack);
  void AssignIpv4Addresses(Ipv4Address network, Ipv4Mask mask);
  ApplicationContainer SetupTcpBulkSend(Ptr<Node> source, Ipv4Address dest, uint16_t port, Time startTime, Time stopTime);
  void EnableNetAnim();

private:
  uint32_t m_k;
  std::vector<FatTreeNode> m_coreSwitches;
  std::vector<std::vector<FatTreeNode>> m_aggrSwitches;
  std::vector<std::vector<FatTreeNode>> m_edgeSwitches;
  std::vector<Ptr<Node>> m_servers;
};

FatTreeHelper::FatTreeHelper(uint32_t k)
  : m_k(k) {
  uint32_t coreSwitchCount = (k * k) / 4;
  uint32_t podCount = k;
  uint32_t aggrSwitchesPerPod = k / 2;
  uint32_t edgeSwitchesPerPod = k / 2;
  uint32_t serversPerEdge = k / 2;

  // Create Core Switches
  m_coreSwitches.resize(coreSwitchCount);
  for (auto &switchNode : m_coreSwitches) {
    switchNode.nodes.Create(1);
  }

  // Create Aggregation and Edge Switches per Pod
  m_aggrSwitches.resize(podCount);
  m_edgeSwitches.resize(podCount);
  for (uint32_t pod = 0; pod < podCount; ++pod) {
    m_aggrSwitches[pod].resize(aggrSwitchesPerPod);
    m_edgeSwitches[pod].resize(edgeSwitchesPerPod);
    for (auto &switchNode : m_aggrSwitches[pod]) {
      switchNode.nodes.Create(1);
    }
    for (auto &switchNode : m_edgeSwitches[pod]) {
      switchNode.nodes.Create(1);
    }
  }

  // Create Servers
  m_servers.resize(podCount * edgeSwitchesPerPod * serversPerEdge);
  for (size_t i = 0; i < m_servers.size(); ++i) {
    m_servers[i] = CreateObject<Node>();
  }

  // Connect Core to Aggregation Switches
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
  p2p.SetChannelAttribute("Delay", StringValue("1ms"));

  for (uint32_t core = 0; core < coreSwitchCount; ++core) {
    uint32_t aggrPod = core % podCount;
    uint32_t aggrIndex = core / podCount;
    if (aggrIndex >= aggrSwitchesPerPod) continue;

    NetDeviceContainer devs = p2p.Install(m_coreSwitches[core].nodes.Get(0), m_aggrSwitches[aggrPod][aggrIndex].nodes.Get(0));
    m_coreSwitches[core].devices.Add(devs.Get(0));
    m_aggrSwitches[aggrPod][aggrIndex].devices.Add(devs.Get(1));
  }

  // Connect Aggregation to Edge Switches
  for (uint32_t pod = 0; pod < podCount; ++pod) {
    for (uint32_t aggr = 0; aggr < aggrSwitchesPerPod; ++aggr) {
      for (uint32_t edge = 0; edge < edgeSwitchesPerPod; ++edge) {
        NetDeviceContainer devs = p2p.Install(m_aggrSwitches[pod][aggr].nodes.Get(0),
                                              m_edgeSwitches[pod][edge].nodes.Get(0));
        m_aggrSwitches[pod][aggr].devices.Add(devs.Get(0));
        m_edgeSwitches[pod][edge].devices.Add(devs.Get(1));
      }
    }
  }

  // Connect Edge Switches to Servers
  for (uint32_t pod = 0; pod < podCount; ++pod) {
    uint32_t serverBase = pod * edgeSwitchesPerPod * serversPerEdge;
    for (uint32_t edge = 0; edge < edgeSwitchesPerPod; ++edge) {
      for (uint32_t host = 0; host < serversPerEdge; ++host) {
        uint32_t idx = serverBase + edge * serversPerEdge + host;
        NetDeviceContainer devs = p2p.Install(m_edgeSwitches[pod][edge].nodes.Get(0), m_servers[idx]);
        m_edgeSwitches[pod][edge].devices.Add(devs.Get(0));
        devs.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(nullptr));
      }
    }
  }
}

void FatTreeHelper::InstallStack(InternetStackHelper stack) {
  for (auto &node : m_coreSwitches) {
    stack.Install(node.nodes);
  }
  for (auto &pod : m_aggrSwitches) {
    for (auto &node : pod) {
      stack.Install(node.nodes);
    }
  }
  for (auto &pod : m_edgeSwitches) {
    for (auto &node : pod) {
      stack.Install(node.nodes);
    }
  }
  stack.Install(m_servers);
}

void FatTreeHelper::AssignIpv4Addresses(Ipv4Address network, Ipv4Mask mask) {
  Ipv4AddressHelper address;
  address.SetBase(network, mask);

  // Assign addresses to all links
  for (auto &node : m_coreSwitches) {
    node.interfaces = address.Assign(node.devices);
    address.NewNetwork();
  }
  for (auto &pod : m_aggrSwitches) {
    for (auto &node : pod) {
      node.interfaces = address.Assign(node.devices);
      address.NewNetwork();
    }
  }
  for (auto &pod : m_edgeSwitches) {
    for (auto &node : pod) {
      node.interfaces = address.Assign(node.devices);
      address.NewNetwork();
    }
  }
}

ApplicationContainer FatTreeHelper::SetupTcpBulkSend(Ptr<Node> source, Ipv4Address dest, uint16_t port, Time startTime, Time stopTime) {
  BulkSendHelper sourceHelper("ns3::TcpSocketFactory", InetSocketAddress(dest, port));
  sourceHelper.SetAttribute("MaxBytes", UintegerValue(0));

  ApplicationContainer app = sourceHelper.Install(source);
  app.Start(startTime);
  app.Stop(stopTime);

  PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  app = sink.Install(m_servers[0]->GetId() == source->GetId() ? m_servers[1] : m_servers[0]);
  app.Start(startTime);
  app.Stop(stopTime);

  return app;
}

void FatTreeHelper::EnableNetAnim() {
  AnimationInterface anim("fat-tree-animation.xml");
  uint32_t nodeId = 0;

  for (auto &node : m_coreSwitches) {
    anim.UpdateNodeDescription(node.nodes.Get(0)->GetId(), "CoreSwitch");
    anim.UpdateNodeColor(node.nodes.Get(0)->GetId(), 255, 0, 0);
  }

  for (auto &pod : m_aggrSwitches) {
    for (auto &node : pod) {
      anim.UpdateNodeDescription(node.nodes.Get(0)->GetId(), "AggregationSwitch");
      anim.UpdateNodeColor(node.nodes.Get(0)->GetId(), 0, 255, 0);
    }
  }

  for (auto &pod : m_edgeSwitches) {
    for (auto &node : pod) {
      anim.UpdateNodeDescription(node.nodes.Get(0)->GetId(), "EdgeSwitch");
      anim.UpdateNodeColor(node.nodes.Get(0)->GetId(), 0, 0, 255);
    }
  }

  for (auto server : m_servers) {
    anim.UpdateNodeDescription(server->GetId(), "Server");
    anim.UpdateNodeColor(server->GetId(), 255, 255, 0);
  }
}

int main(int argc, char *argv[]) {
  uint32_t k = 4;
  double simulationTime = 10.0;

  CommandLine cmd(__FILE__);
  cmd.AddValue("k", "Fat-Tree parameter k (even number)", k);
  cmd.Parse(argc, argv);

  NS_ABORT_MSG_IF(k % 2 != 0, "k must be even");

  FatTreeHelper fatTree(k);
  InternetStackHelper stack;
  stack.SetRoutingHelper(Ipv4StaticRoutingHelper());
  fatTree.InstallStack(stack);

  fatTree.AssignIpv4Addresses("10.1.0.0", "255.255.0.0");

  // Set up TCP flows between servers
  uint16_t port = 5000;
  Time startTime = Seconds(1.0);
  Time stopTime = Seconds(simulationTime - 1.0);

  for (size_t i = 0; i < fatTree.m_servers.size(); i += 2) {
    if (i + 1 >= fatTree.m_servers.size()) break;
    Ipv4Address destAddr = fatTree.m_servers[i + 1]->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
    fatTree.SetupTcpBulkSend(fatTree.m_servers[i], destAddr, port, startTime, stopTime);
  }

  // Enable animation output
  fatTree.EnableNetAnim();

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}