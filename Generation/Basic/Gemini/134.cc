#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/random-variable-stream.h"
#include "ns3/flow-monitor-module.h"

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <numeric>

// Define a logging component for the routing agent
NS_LOG_COMPONENT_DEFINE("PoissonReverseRoutingAgent");

// Custom Application class for the Poisson Reverse Routing Protocol
class PoissonReverseRoutingAgent : public ns3::Application
{
public:
    static ns3::TypeId GetTypeId();
    PoissonReverseRoutingAgent();
    virtual ~PoissonReverseRoutingAgent();
    void SetPeer(ns3::Ipv4Address peerAddress, uint16_t peerPort);
    void SetUpdateInterval(ns3::Time updateInterval);
    void SetPoissonMean(double mean);
    void SetBaseCost(uint32_t baseCost);
    void SetLocalP2pInterface(ns3::Ptr<ns3::NetDevice> p2pNetDevice);

protected:
    virtual void DoInitialize() override;
    virtual void DoDispose() override;
    virtual void StartApplication() override;
    virtual void StopApplication() override;

private:
    void SendUpdate();
    void HandleRead(ns3::Ptr<ns3::Socket> socket);
    void UpdateRouteTable();

    ns3::Ptr<ns3::Node> m_node;
    ns3::Ptr<ns3::Ipv4> m_ipv4;
    ns3::Ptr<ns3::NetDevice> m_p2pNetDevice; // Pointer to the actual P2P NetDevice
    uint32_t m_p2pInterfaceIndex; // Index of the P2P interface on this node

    ns3::Ipv4Address m_peerAddress;
    uint16_t m_peerPort;
    ns3::Ptr<ns3::Socket> m_socket; // UDP socket for routing updates

    ns3::EventId m_updateTimer;
    ns3::Time m_updateInterval;

    ns3::Ptr<ns3::PoissonRandomVariable> m_poissonRv;
    double m_poissonMean;
    uint32_t m_baseCost;

    uint32_t m_myLinkCost;      // Current cost of this node's outgoing link
    uint32_t m_peerLinkCost;    // Last received cost from the peer's outgoing link

    ns3::Ptr<ns3::Ipv4StaticRouting> m_staticRouting;
};

NS_OBJECT_ENSURE_REGISTERED(PoissonReverseRoutingAgent);

ns3::TypeId PoissonReverseRoutingAgent::GetTypeId()
{
    static ns3::TypeId tid = ns3::TypeId("ns3::PoissonReverseRoutingAgent")
                                 .SetParent<ns3::Application>()
                                 .SetGroupName("Applications")
                                 .AddConstructor<PoissonReverseRoutingAgent>()
                                 .AddAttribute("PeerAddress",
                                               "The IP address of the peer node.",
                                               ns3::Ipv4AddressValue(),
                                               ns3::MakeIpv4AddressAccessor(&PoissonReverseRoutingAgent::m_peerAddress),
                                               ns3::MakeIpv4AddressChecker())
                                 .AddAttribute("PeerPort",
                                               "The UDP port for routing updates.",
                                               ns3::UintegerValue(9999),
                                               ns3::MakeUintegerAccessor(&PoissonReverseRoutingAgent::m_peerPort),
                                               ns3::MakeUintegerChecker<uint16_t>())
                                 .AddAttribute("UpdateInterval",
                                               "Time interval between routing updates.",
                                               ns3::TimeValue(ns3::Seconds(1.0)),
                                               ns3::MakeTimeAccessor(&PoissonReverseRoutingAgent::m_updateInterval),
                                               ns3::MakeTimeChecker())
                                 .AddAttribute("PoissonMean",
                                               "Mean of the Poisson distribution for cost calculation.",
                                               ns3::DoubleValue(5.0),
                                               ns3::MakeDoubleAccessor(&PoissonReverseRoutingAgent::m_poissonMean),
                                               ns3::MakeDoubleChecker<double>())
                                 .AddAttribute("BaseCost",
                                               "Base cost for the link.",
                                               ns3::UintegerValue(1),
                                               ns3::MakeUintegerAccessor(&PoissonReverseRoutingAgent::m_baseCost),
                                               ns3::MakeUintegerChecker<uint32_t>());
    return tid;
}

PoissonReverseRoutingAgent::PoissonReverseRoutingAgent()
    : m_node(nullptr),
      m_ipv4(nullptr),
      m_p2pNetDevice(nullptr),
      m_p2pInterfaceIndex(0),
      m_peerAddress("0.0.0.0"),
      m_peerPort(0),
      m_socket(nullptr),
      m_updateTimer(),
      m_updateInterval(ns3::Seconds(1.0)),
      m_poissonRv(ns3::CreateObject<ns3::PoissonRandomVariable>()),
      m_poissonMean(5.0),
      m_baseCost(1),
      m_myLinkCost(0),
      m_peerLinkCost(0),
      m_staticRouting(nullptr)
{
}

PoissonReverseRoutingAgent::~PoissonReverseRoutingAgent()
{
}

void PoissonReverseRoutingAgent::DoInitialize()
{
    m_node = GetNode();
    m_ipv4 = m_node->GetObject<ns3::Ipv4>();

    // Get the Ipv4StaticRouting object for this node
    ns3::Ptr<ns3::Ipv4RoutingProtocol> ipv4Routing = m_ipv4->GetRoutingProtocol();
    m_staticRouting = ns3::DynamicCast<ns3::Ipv4StaticRouting>(ipv4Routing);
    if (!m_staticRouting)
    {
        NS_FATAL_ERROR("PoissonReverseRoutingAgent requires Ipv4StaticRouting on the node.");
    }

    // Set the mean for the Poisson random variable
    m_poissonRv->SetMean(m_poissonMean);

    ns3::Application::DoInitialize();
}

void PoissonReverseRoutingAgent::DoDispose()
{
    m_socket = nullptr;
    m_poissonRv = nullptr;
    m_staticRouting = nullptr;
    m_ipv4 = nullptr;
    m_node = nullptr;
    ns3::Application::DoDispose();
}

void PoissonReverseRoutingAgent::SetPeer(ns3::Ipv4Address peerAddress, uint16_t peerPort)
{
    m_peerAddress = peerAddress;
    m_peerPort = peerPort;
}

void PoissonReverseRoutingAgent::SetUpdateInterval(ns3::Time updateInterval)
{
    m_updateInterval = updateInterval;
}

void PoissonReverseRoutingAgent::SetPoissonMean(double mean)
{
    m_poissonMean = mean;
    if (m_poissonRv)
    {
        m_poissonRv->SetMean(m_poissonMean);
    }
}

void PoissonReverseRoutingAgent::SetBaseCost(uint32_t baseCost)
{
    m_baseCost = baseCost;
}

void PoissonReverseRoutingAgent::SetLocalP2pInterface(ns3::Ptr<ns3::NetDevice> p2pNetDevice)
{
    m_p2pNetDevice = p2pNetDevice;
    m_p2pInterfaceIndex = m_ipv4->GetInterfaceForDevice(m_p2pNetDevice);
    if (m_p2pInterfaceIndex == (uint32_t)-1)
    {
        NS_FATAL_ERROR("P2P NetDevice not found on node's IPv4 interface list.");
    }
}

void PoissonReverseRoutingAgent::StartApplication()
{
    if (!m_socket)
    {
        m_socket = ns3::Socket::CreateSocket(GetNode(), ns3::UdpSocketFactory::GetTypeId());
        ns3::InetSocketAddress local = ns3::InetSocketAddress(m_ipv4->GetAddress(m_p2pInterfaceIndex, 0).GetLocal(), m_peerPort);
        m_socket->Bind(local);
        m_socket->SetRecvCallback(ns3::MakeCallback(&PoissonReverseRoutingAgent::HandleRead, this));
    }

    // Initial cost setup for both own and peer link. Peer cost is assumed base until actual update.
    m_myLinkCost = m_baseCost + static_cast<uint32_t>(m_poissonRv->GetValue());
    if (m_myLinkCost == 0) m_myLinkCost = 1; // Ensure non-zero cost
    m_peerLinkCost = m_baseCost; 

    // Schedule the first update
    m_updateTimer = ns3::Simulator::Schedule(ns3::Seconds(0.0), &PoissonReverseRoutingAgent::SendUpdate, this);

    NS_LOG_INFO("Node " << GetNode()->GetId() << ": PoissonReverseRoutingAgent started. Listening on "
                         << m_ipv4->GetAddress(m_p2pInterfaceIndex, 0).GetLocal() << ":" << m_peerPort
                         << ", Peer: " << m_peerAddress << ":" << m_peerPort);
}

void PoissonReverseRoutingAgent::StopApplication()
{
    ns3::Simulator::Cancel(m_updateTimer);
    if (m_socket)
    {
        m_socket->Close();
        m_socket = nullptr;
    }
    NS_LOG_INFO("Node " << GetNode()->GetId() << ": PoissonReverseRoutingAgent stopped.");
}

void PoissonReverseRoutingAgent::SendUpdate()
{
    // 1. Generate my current link cost
    uint32_t newMyLinkCost = m_baseCost + static_cast<uint32_t>(m_poissonRv->GetValue());
    if (newMyLinkCost == 0)
    {
        newMyLinkCost = 1;
    }

    if (newMyLinkCost != m_myLinkCost)
    {
        NS_LOG_INFO("Node " << GetNode()->GetId() << ": My link cost changed from " << m_myLinkCost << " to " << newMyLinkCost);
        m_myLinkCost = newMyLinkCost;
    }
    else
    {
        NS_LOG_INFO("Node " << GetNode()->GetId() << ": My link cost remains " << m_myLinkCost);
    }

    // 2. Create and send a packet with my link cost
    ns3::Ptr<ns3::Packet> packet = ns3::Create<ns3::Packet>((uint8_t *)&m_myLinkCost, sizeof(m_myLinkCost));
    m_socket->SendTo(packet, 0, ns3::InetSocketAddress(m_peerAddress, m_peerPort));

    NS_LOG_INFO("Node " << GetNode()->GetId() << ": Sent update (my link cost: " << m_myLinkCost << ") to " << m_peerAddress << ":" << m_peerPort);

    // 3. Update local routing table with the latest available information
    UpdateRouteTable();

    // 4. Schedule next update
    m_updateTimer = ns3::Simulator::Schedule(m_updateInterval, &PoissonReverseRoutingAgent::SendUpdate, this);
}

void PoissonReverseRoutingAgent::HandleRead(ns3::Ptr<ns3::Socket> socket)
{
    ns3::Ptr<ns3::Packet> packet;
    ns3::InetSocketAddress from;
    uint32_t receivedCost = 0;

    while ((packet = socket->RecvFrom(from)))
    {
        if (packet->GetSize() == sizeof(uint32_t))
        {
            packet->CopyDataTo(reinterpret_cast<uint8_t *>(&receivedCost), sizeof(uint32_t));
            if (receivedCost == 0) 
            {
                receivedCost = 1;
            }

            if (receivedCost != m_peerLinkCost)
            {
                NS_LOG_INFO("Node " << GetNode()->GetId() << ": Received peer cost changed from " << m_peerLinkCost << " to " << receivedCost << " from " << from.GetIpv4());
                m_peerLinkCost = receivedCost;
                // Update local routing table with the new peer cost
                UpdateRouteTable();
            }
            else
            {
                NS_LOG_INFO("Node " << GetNode()->GetId() << ": Received peer cost remains " << receivedCost << " from " << from.GetIpv4());
            }
        }
        else
        {
            NS_LOG_WARN("Node " << GetNode()->GetId() << ": Received packet of unexpected size: " << packet->GetSize());
        }
    }
}

void PoissonReverseRoutingAgent::UpdateRouteTable()
{
    if (!m_staticRouting)
    {
        NS_LOG_ERROR("Node " << GetNode()->GetId() << ": Ipv4StaticRouting object is null, cannot update route table.");
        return;
    }

    uint32_t oldMetric = 0;
    bool routeExists = false;

    // Iterate through static routes to find the one to the peer's host IP
    // and remove it if it exists.
    // Note: GetRouteList might return routes that are not specifically host routes
    // to the peer if other helper functions configured them.
    // For a simple P2P, a single host route is expected.
    for (uint32_t i = 0; i < m_staticRouting->GetNRoutes(); ++i)
    {
        ns3::Ipv4RoutingTableEntry entry = m_staticRouting->GetRoute(i);
        if (entry.IsHost() && entry.GetDest() == m_peerAddress && 
            entry.GetGateway() == m_peerAddress && entry.GetInterface() == m_p2pInterfaceIndex)
        {
            oldMetric = entry.GetMetric();
            routeExists = true;
            // Remove the route. Note: The exact Remove method depends on how it was added.
            // For AddHostRouteTo(dst, gateway, interface, metric), this works.
            m_staticRouting->RemoveHostRouteTo(m_peerAddress, m_peerAddress, m_p2pInterfaceIndex);
            NS_LOG_INFO("Node " << GetNode()->GetId() << ": Removed old route to " << m_peerAddress << " with metric " << oldMetric);
            break; 
        }
    }

    uint32_t newMetric = m_myLinkCost + m_peerLinkCost;
    if (newMetric == 0) 
    {
        newMetric = 1;
    }
    
    // Add the new route with the updated metric.
    // The metric combines this node's link cost and the peer's link cost.
    m_staticRouting->AddHostRouteTo(m_peerAddress, m_peerAddress, m_p2pInterfaceIndex, newMetric);
    NS_LOG_INFO("Node " << GetNode()->GetId() << ": Added/Updated route to " << m_peerAddress << " with new metric " << newMetric << " (my: " << m_myLinkCost << ", peer: " << m_peerLinkCost << ")");
}

// Main simulation function
int main(int argc, char *argv[])
{
    // Enable logging for relevant components
    ns3::LogComponentEnable("PoissonReverseRoutingAgent", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("OnOffApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("PacketSink", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("PointToPointNetDevice", ns3::LOG_LEVEL_DEBUG);
    ns3::LogComponentEnable("Ipv4L3Protocol", ns3::LOG_LEVEL_DEBUG); // To see route lookups
    ns3::LogComponentEnable("Ipv4StaticRouting", ns3::LOG_LEVEL_ALL); // To see route adds/removes

    // Command line arguments
    double simTime = 20.0; // seconds
    double updateInterval = 1.0; // seconds for routing updates
    double poissonMean = 5.0; // Mean for the Poisson distribution
    uint32_t baseLinkCost = 1; // Base cost for the P2P link

    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Total simulation time in seconds", simTime);
    cmd.AddValue("updateInterval", "Time interval between routing updates in seconds", updateInterval);
    cmd.AddValue("poissonMean", "Mean for the Poisson distribution", poissonMean);
    cmd.AddValue("baseLinkCost", "Base cost for the P2P link", baseLinkCost);
    cmd.Parse(argc, argv);

    // Set a fixed random seed for reproducibility
    ns3::RngSeedManager::SetSeed(1);
    ns3::RngSeedManager::SetRun(1);

    // 1. Create nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // 2. Create point-to-point link
    ns3::PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", ns3::StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", ns3::StringValue("10ms"));
    ns3::NetDeviceContainer p2pDevices;
    p2pDevices = p2p.Install(nodes);

    // 3. Install Internet stack (with static routing)
    ns3::InternetStackHelper internet;
    internet.SetRoutingHelper(ns3::Ipv4StaticRoutingHelper()); // Use static routing
    internet.Install(nodes);

    // 4. Assign IP addresses
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.252"); // /30 network for P2P
    ns3::Ipv4InterfaceContainer p2pInterfaces = ipv4.Assign(p2pDevices);

    ns3::Ipv4Address node0_ip = p2pInterfaces.GetAddress(0);
    ns3::Ipv4Address node1_ip = p2pInterfaces.GetAddress(1);

    // 5. Install PoissonReverseRoutingAgent application on both nodes
    ns3::Ptr<PoissonReverseRoutingAgent> routingAgent0 = ns3::CreateObject<PoissonReverseRoutingAgent>();
    routingAgent0->SetNode(nodes.Get(0));
    routingAgent0->SetPeer(node1_ip, 9999);
    routingAgent0->SetUpdateInterval(ns3::Seconds(updateInterval));
    routingAgent0->SetPoissonMean(poissonMean);
    routingAgent0->SetBaseCost(baseLinkCost);
    routingAgent0->SetLocalP2pInterface(p2pDevices.Get(0)); // Pass the NetDevice object
    nodes.Get(0)->AddApplication(routingAgent0);
    routingAgent0->StartApplication();

    ns3::Ptr<PoissonReverseRoutingAgent> routingAgent1 = ns3::CreateObject<PoissonReverseRoutingAgent>();
    routingAgent1->SetNode(nodes.Get(1));
    routingAgent1->SetPeer(node0_ip, 9999);
    routingAgent1->SetUpdateInterval(ns3::Seconds(updateInterval));
    routingAgent1->SetPoissonMean(poissonMean);
    routingAgent1->SetBaseCost(baseLinkCost);
    routingAgent1->SetLocalP2pInterface(p2pDevices.Get(1)); // Pass the NetDevice object
    nodes.Get(1)->AddApplication(routingAgent1);
    routingAgent1->StartApplication();

    // 6. Install traffic generating application (OnOffApplication)
    uint16_t sinkPort = 9000;
    ns3::Address sinkAddress(ns3::InetSocketAddress(node1_ip, sinkPort));
    ns3::PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkAddress);
    ns3::ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(ns3::Seconds(0.0));
    sinkApps.Stop(ns3::Seconds(simTime + 0.1));

    ns3::OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoff.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0.0]")); // Always on
    onoff.SetAttribute("PacketSize", ns3::UintegerValue(1024));
    onoff.SetAttribute("DataRate", ns3::DataRateValue("500kbps"));
    ns3::ApplicationContainer clientApps = onoff.Install(nodes.Get(0));
    clientApps.Start(ns3::Seconds(1.0)); // Start traffic after initial routing setup
    clientApps.Stop(ns3::Seconds(simTime));

    // 7. Setup FlowMonitor for statistics
    ns3::FlowMonitorHelper flowMonitor;
    ns3::Ptr<ns3::FlowMonitor> monitor = flowMonitor.InstallAll();

    // 8. Run simulation
    ns3::Simulator::Stop(ns3::Seconds(simTime));
    ns3::Simulator::Run();

    // 9. Print FlowMonitor statistics
    monitor->CheckFor ;
    monitor->SerializeToXmlFile("poisson-reverse-flowmon.xml", true, true);

    ns3::Simulator::Destroy();

    NS_LOG_INFO("Simulation finished.");
    return 0;
}