#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FatTreeSimulation");

class FatTreeHelper {
public:
  FatTreeHelper(uint32_t k);

  void InstallStack(InternetStackHelper stack);
  void AssignIpv4Addresses(Ipv4AddressHelper ip);
  NodeContainer GetHosts() const { return m_hosts; }
  void CreateTcpConnections(Time startTime, Time endTime);

private:
  uint32_t m_k;
  NodeContainer m_coreSwitches;
  NodeContainer m_aggSwitches;
  NodeContainer m_edgeSwitches;
  NodeContainer m_hosts;

  std::vector<std::vector<Ptr<Node>>> m_aggSwitchMatrix;
  std::vector<std::vector<Ptr<Node>>> m_edgeSwitchMatrix;
  std::vector<std::vector<Ptr<Node>>> m_hostMatrix;
};

FatTreeHelper::FatTreeHelper(uint32_t k) : m_k(k) {
  uint32_t numPods = k;
  uint32_t numCoreSwitches = (k / 2) * (k / 2);
  uint32_t numAggPerPod = k / 2;
  uint32_t numEdgePerPod = k / 2;
  uint32_t numHostsPerEdge = k / 2;

  // Core switches
  m_coreSwitches.Create(numCoreSwitches);
  for (uint32_t i = 0; i < numCoreSwitches; ++i) {
    Ptr<Node> switchNode = m_coreSwitches.Get(i);
    InternetStackHelper::Install(switchNode);
  }

  // Pod-level switches and hosts
  m_aggSwitchMatrix.resize(numPods);
  m_edgeSwitchMatrix.resize(numPods);
  m_hostMatrix.resize(numPods);

  for (uint32_t p = 0; p < numPods; ++p) {
    m_aggSwitchMatrix[p].resize(numAggPerPod);
    m_edgeSwitchMatrix[p].resize(numEdgePerPod);
    m_hostMatrix[p].resize(numEdgePerPod);

    for (uint32_t a = 0; a < numAggPerPod; ++a) {
      Ptr<Node> switchNode = CreateObject<Node>();
      InternetStackHelper::Install(switchNode);
      m_aggSwitchMatrix[p][a] = switchNode;
      m_aggSwitches.Add(switchNode);
    }

    for (uint32_t e = 0; e < numEdgePerPod; ++e) {
      Ptr<Node> switchNode = CreateObject<Node>();
      InternetStackHelper::Install(switchNode);
      m_edgeSwitchMatrix[p][e] = switchNode;
      m_edgeSwitches.Add(switchNode);

      m_hostMatrix[p][e].Create(numHostsPerEdge);
      m_hosts.Add(m_hostMatrix[p][e]);
    }
  }

  // Connect core to agg
  PointToPointHelper coreLink;
  coreLink.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
  coreLink.SetChannelAttribute("Delay", StringValue("1ms"));

  for (uint32_t c = 0; c < numCoreSwitches; ++c) {
    uint32_t upperPortion = c / (k / 2);
    uint32_t podIndex = upperPortion;
    NS_ASSERT_MSG(podIndex < numPods, "Invalid pod index during core-agg connection");

    for (uint32_t a = 0; a < numAggPerPod; ++a) {
      Ptr<Node> core = m_coreSwitches.Get(c);
      Ptr<Node> agg = m_aggSwitchMatrix[podIndex][a];
      coreLink.Install(core, agg);
    }
  }

  // Connect agg to edge within each pod
  PointToPointHelper aggLink;
  aggLink.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
  aggLink.SetChannelAttribute("Delay", StringValue("1ms"));

  for (uint32_t p = 0; p < numPods; ++p) {
    for (uint32_t a = 0; a < numAggPerPod; ++a) {
      Ptr<Node> agg = m_aggSwitchMatrix[p][a];
      for (uint32_t e = 0; e < numEdgePerPod; ++e) {
        Ptr<Node> edge = m_edgeSwitchMatrix[p][e];
        aggLink.Install(agg, edge);
      }
    }
  }

  // Connect edge to hosts
  PointToPointHelper hostLink;
  hostLink.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
  hostLink.SetChannelAttribute("Delay", StringValue("10ms"));

  for (uint32_t p = 0; p < numPods; ++p) {
    for (uint32_t e = 0; e < numEdgePerPod; ++e) {
      Ptr<Node> edge = m_edgeSwitchMatrix[p][e];
      for (uint32_t h = 0; h < numHostsPerEdge; ++h) {
        Ptr<Node> host = m_hostMatrix[p][e].Get(h);
        hostLink.Install(edge, host);
      }
    }
  }
}

void FatTreeHelper::InstallStack(InternetStackHelper stack) {
  stack.Install(m_hosts);
  stack.Install(m_coreSwitches);
  stack.Install(m_aggSwitches);
  stack.Install(m_edgeSwitches);
}

void FatTreeHelper::AssignIpv4Addresses(Ipv4AddressHelper ip) {
  PointToPointHelper p2p;
  Ipv4InterfaceContainer interfaces;

  // Core to Agg links
  for (auto core : m_coreSwitches) {
    for (auto agg : m_aggSwitches) {
      NetDeviceContainer devices = p2p.Install(core, agg);
      interfaces = ip.Assign(devices);
      ip.NewNetwork();
    }
  }

  // Agg to Edge links
  for (auto pod : m_aggSwitchMatrix) {
    for (auto agg : pod) {
      for (auto edge : m_edgeSwitchMatrix[0]) { // Assuming all pods have same edge count
        NetDeviceContainer devices = p2p.Install(agg, edge);
        interfaces = ip.Assign(devices);
        ip.NewNetwork();
      }
    }
  }

  // Edge to Host links
  for (auto pod : m_hostMatrix) {
    for (auto edgeHosts : pod) {
      for (auto host : edgeHosts) {
        NetDeviceContainer devices = p2p.Install(m_edgeSwitches.Get(0), host); // Simplified
        interfaces = ip.Assign(devices);
        ip.NewNetwork();
      }
    }
  }
}

void FatTreeHelper::CreateTcpConnections(Time startTime, Time endTime) {
  uint32_t totalHosts = m_hosts.GetN();
  for (uint32_t i = 0; i < totalHosts; ++i) {
    for (uint32_t j = 0; j < totalHosts; ++j) {
      if (i != j) {
        Ptr<Node> src = m_hosts.Get(i);
        Ptr<Node> dst = m_hosts.Get(j);

        Address destIp = dst->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(); // Assume interface 1

        InetSocketAddress destSocketAddr(destIp, 50000);
        OnOffHelper onoff("ns3::TcpSocketFactory", destSocketAddr);
        onoff.SetConstantRate(DataRate("1Mbps"), 512);

        ApplicationContainer app = onoff.Install(src);
        app.Start(startTime);
        app.Stop(endTime);
      }
    }
  }
}

int main(int argc, char *argv[]) {
  uint32_t fatTreeK = 4;
  Time simDuration = Seconds(10);

  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  FatTreeHelper fatTree(fatTreeK);

  InternetStackHelper stack;
  fatTree.InstallStack(stack);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  fatTree.AssignIpv4Addresses(address);

  fatTree.CreateTcpConnections(Seconds(1), simDuration - Seconds(1));

  AnimationInterface anim("fat-tree.xml");
  anim.EnablePacketMetadata(true);

  Simulator::Stop(simDuration);
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}