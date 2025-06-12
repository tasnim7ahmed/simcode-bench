#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridTopologySimulation");

class HybridTopologyHelper {
public:
    HybridTopologyHelper();

    void BuildTopology();
    void SetupApplications();
    void SetupTracing();
    void SetupAnimation();

private:
    NodeContainer m_ringNodes;
    NodeContainer m_meshNodes;
    NodeContainer m_busNodes;
    NodeContainer m_lineNodes;
    NodeContainer m_starHub;
    NodeContainer m_starSpokes;
    NodeContainer m_treeRoot;
    NodeContainer m_treeLevel1;
    NodeContainer m_treeLevel2;

    NetDeviceContainer m_ringDevices;
    PointToPointHelper m_pointToPoint;

    Ipv4InterfaceContainer m_interfaces;
    InternetStackHelper m_stack;
};

HybridTopologyHelper::HybridTopologyHelper() {
    m_pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    m_pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
}

void HybridTopologyHelper::BuildTopology() {
    // Ring Topology (4 nodes)
    m_ringNodes.Create(4);
    for (uint32_t i = 0; i < m_ringNodes.GetN(); ++i) {
        NetDeviceContainer container = m_pointToPoint.Install(m_ringNodes.Get(i), m_ringNodes.Get((i + 1) % m_ringNodes.GetN()));
        m_ringDevices.Add(container);
    }

    // Mesh Topology (4 fully connected nodes)
    m_meshNodes.Create(4);
    for (uint32_t i = 0; i < m_meshNodes.GetN(); ++i) {
        for (uint32_t j = i + 1; j < m_meshNodes.GetN(); ++j) {
            m_pointToPoint.Install(m_meshNodes.Get(i), m_meshNodes.Get(j));
        }
    }

    // Bus Topology (5 nodes on a single bus with one central hub)
    NodeContainer busHub;
    busHub.Create(1);
    m_busNodes.Create(5);
    for (auto& node : m_busNodes) {
        m_pointToPoint.Install(busHub.Get(0), node);
    }

    // Line Topology (6 nodes in a line)
    m_lineNodes.Create(6);
    for (uint32_t i = 0; i < m_lineNodes.GetN() - 1; ++i) {
        m_pointToPoint.Install(m_lineNodes.Get(i), m_lineNodes.Get(i + 1));
    }

    // Star Topology (1 hub and 5 spokes)
    m_starHub.Create(1);
    m_starSpokes.Create(5);
    for (auto& spoke : m_starSpokes) {
        m_pointToPoint.Install(m_starHub.Get(0), spoke);
    }

    // Tree Topology (root -> 2 level1 -> each 2 level2)
    m_treeRoot.Create(1);
    m_treeLevel1.Create(2);
    m_treeLevel2.Create(4);

    m_pointToPoint.Install(m_treeRoot.Get(0), m_treeLevel1.Get(0));
    m_pointToPoint.Install(m_treeRoot.Get(0), m_treeLevel1.Get(1));

    m_pointToPoint.Install(m_treeLevel1.Get(0), m_treeLevel2.Get(0));
    m_pointToPoint.Install(m_treeLevel1.Get(0), m_treeLevel2.Get(1));
    m_pointToPoint.Install(m_treeLevel1.Get(1), m_treeLevel2.Get(2));
    m_treeLevel2.Get(3)->AddDevice(m_pointToPoint.Install(m_treeLevel1.Get(1), m_treeLevel2.Get(3)).Get(1));

    // Install internet stack
    m_stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    for (NetDeviceContainer::Iterator iter = m_ringDevices.Begin(); iter != m_ringDevices.End(); ++iter) {
        m_interfaces.Add(address.Assign(*iter));
        address.NewNetwork();
    }
}

void HybridTopologyHelper::SetupApplications() {
    UdpEchoServerHelper echoServer(9);

    ApplicationContainer serverApps = echoServer.Install(m_ringNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(m_ringNodes.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(m_ringNodes.Get(2));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));
}

void HybridTopologyHelper::SetupTracing() {
    AsciiTraceHelper asciiTraceHelper;
    m_pointToPoint.EnableAsciiAll(asciiTraceHelper.CreateFileStream("hybrid-topology.tr"));
    m_pointToPoint.EnablePcapAll("hybrid-topology");
}

void HybridTopologyHelper::SetupAnimation() {
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(20),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.InstallAll();

    AnimationInterface anim("hybrid-topology.xml");
    anim.EnablePacketMetadata(true);
}

int main(int argc, char* argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    LogComponentEnable("HybridTopologySimulation", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    HybridTopologyHelper topology;
    topology.BuildTopology();
    topology.SetupApplications();
    topology.SetupTracing();
    topology.SetupAnimation();

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}