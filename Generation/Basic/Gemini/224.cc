#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/udp-client-server-helper.h"

#include <iostream>
#include <vector>
#include <random>
#include <algorithm> // For std::remove

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("MobileAodvUdp");

/**
 * \brief Custom Application class to send UDP packets to a random node in the network.
 *
 * Each instance of this application runs on a node and periodically sends a UDP
 * packet to a randomly chosen destination IP address from the list of all
 * other nodes' IP addresses in the simulation.
 */
class RandomUdpApplication : public Application
{
public:
    RandomUdpApplication();
    ~RandomUdpApplication() override;

    /**
     * \brief Configure the application.
     * \param socket The socket to use for sending packets.
     * \param myIp The IPv4 address of the node this application is running on.
     * \param allNodeIps A vector containing all valid IPv4 addresses in the simulation.
     * \param sendInterval The time interval between consecutive packet sends.
     * \param packetSize The size of each UDP packet to send.
     * \param port The destination port for UDP packets.
     */
    void Setup(Ptr<Socket> socket, Ipv4Address myIp, std::vector<Ipv4Address> allNodeIps,
               Time sendInterval, uint32_t packetSize, uint16_t port);

private:
    void StartApplication() override;
    void StopApplication() override;
    void SendPacket();

    Ptr<Socket> m_socket;
    Ipv4Address m_myIp;
    std::vector<Ipv4Address> m_allNodeIps; // All possible destination IPs
    Time m_sendInterval;
    uint32_t m_packetSize;
    uint16_t m_port;
    EventId m_sendEvent;
    uint32_t m_packetsSent;

    std::mt19937 m_rng; // Mersenne Twister random number generator
};

NS_OBJECT_ENSURE_REGISTERED(RandomUdpApplication);

RandomUdpApplication::RandomUdpApplication()
    : m_socket(nullptr),
      m_myIp(),
      m_sendInterval(Seconds(1.0)),
      m_packetSize(1024),
      m_port(9), // Default echo port
      m_packetsSent(0)
{
    // Seed the RNG with current simulation time in milliseconds for variation across runs.
    // For reproducible runs, a fixed seed (e.g., m_rng.seed(12345);) would be used.
    m_rng.seed(Simulator::Now().GetMilliSeconds());
}

RandomUdpApplication::~RandomUdpApplication()
{
    m_socket = nullptr;
}

void RandomUdpApplication::Setup(Ptr<Socket> socket, Ipv4Address myIp, std::vector<Ipv4Address> allNodeIps,
                                 Time sendInterval, uint32_t packetSize, uint16_t port)
{
    m_socket = socket;
    m_myIp = myIp;
    m_allNodeIps = allNodeIps;
    m_sendInterval = sendInterval;
    m_packetSize = packetSize;
    m_port = port;

    // Remove own IP from the list of possible destinations to avoid sending to self
    m_allNodeIps.erase(
        std::remove(m_allNodeIps.begin(), m_allNodeIps.end(), m_myIp),
        m_allNodeIps.end());

    if (m_allNodeIps.empty() && myIp != Ipv4Address::GetZero()) {
        NS_FATAL_ERROR("RandomUdpApplication: Node " << GetNode()->GetId() << " (IP: " << m_myIp << ") has no other nodes to send to.");
    }
}

void RandomUdpApplication::StartApplication()
{
    m_packetsSent = 0;
    // Check if the node has an IPv4 address configured (interface 1 is typically the wireless, 0 is loopback)
    if (GetNode()->GetObject<Ipv4>()->GetNInterfaces() <= 1 ||
        GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal() == Ipv4Address::GetZero()) {
        NS_LOG_WARN("RandomUdpApplication on Node " << GetNode()->GetId() << ": No valid IPv4 address found on non-loopback interface. Application will not start.");
        return;
    }

    // Schedule the first packet to be sent immediately at application start
    m_sendEvent = Simulator::Schedule(Seconds(0.0), &RandomUdpApplication::SendPacket, this);
}

void RandomUdpApplication::StopApplication()
{
    if (m_sendEvent.IsRunning()) {
        Simulator::Cancel(m_sendEvent);
    }
    if (m_socket) {
        m_socket->Close();
        m_socket = nullptr;
    }
}

void RandomUdpApplication::SendPacket()
{
    if (m_allNodeIps.empty()) {
        NS_LOG_WARN("RandomUdpApplication on Node " << GetNode()->GetId() << ": No valid destinations available, stopping sending.");
        return;
    }

    // Pick a random destination IP from the list of all other nodes' IPs
    std::uniform_int_distribution<size_t> distribution(0, m_allNodeIps.size() - 1);
    Ipv4Address destIp = m_allNodeIps[distribution(m_rng)];

    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->SendTo(packet, 0, InetSocketAddress(destIp, m_port));
    m_packetsSent++;

    NS_LOG_INFO("At " << Simulator::Now().GetSeconds() << "s, "
                      << "Node " << GetNode()->GetId() << " (IP: " << m_myIp << ") sent "
                      << m_packetSize << " bytes to " << destIp << ":" << m_port);

    // Schedule the next packet send
    m_sendEvent = Simulator::Schedule(m_sendInterval, &RandomUdpApplication::SendPacket, this);
}

int main(int argc, char *argv[])
{
    // Simulation parameters
    uint32_t numNodes = 20;
    double simulationTime = 20.0; // seconds
    Time sendInterval = Seconds(1.0);
    uint32_t packetSize = 1024; // bytes
    uint16_t port = 9;          // Echo port (UDP server listens on this port)

    // Configure logging for various components
    LogComponentEnable("MobileAodvUdp", LOG_LEVEL_INFO);
    LogComponentEnable("AodvHelper", LOG_LEVEL_DEBUG);
    LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("RandomUdpApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Configure mobility using RandomWaypoint model
    MobilityHelper mobility;
    mobility.SetMobilityModelType("ns3::RandomWaypointMobilityModel");
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=1000.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=1000.0]"));
    mobility.SetAttribute("Speed", StringValue("ns3::UniformRandomVariable[Min=5.0|Max=15.0]")); // meters/second
    mobility.SetAttribute("Pause", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=5.0]"));  // seconds
    mobility.Install(nodes);

    // Install Internet Stack with AODV routing protocol
    AodvHelper aodv;
    InternetStackHelper internetStack;
    internetStack.SetRoutingHelper(aodv);
    internetStack.Install(nodes);

    // Configure Wifi devices (Ad-hoc mode)
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac"); // Ad-hoc MAC layer

    WifiPhyHelper wifiPhy = WifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ); // Use 802.11n 5GHz for higher throughput

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Gather all assigned IPv4 addresses to pass to the custom application
    std::vector<Ipv4Address> allNodeIps;
    for (uint32_t i = 0; i < interfaces.GetN(); ++i) {
        allNodeIps.push_back(interfaces.GetAddress(i));
    }

    // Install UDP server on all nodes (to receive packets)
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(nodes);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    // Install custom RandomUdpApplication (client) on all nodes
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        Ptr<Ipv4> ipv4Obj = node->GetObject<Ipv4>();
        // Get the IP address of the wireless interface (index 1 is typically the first non-loopback interface)
        Ipv4Address nodeIp = ipv4Obj->GetAddress(1, 0).GetLocal();

        Ptr<Socket> clientSocket = Socket::CreateSocket(node, UdpSocketFactory::GetTypeId());
        Ptr<RandomUdpApplication> clientApp = CreateObject<RandomUdpApplication>();
        clientApp->SetNode(node); // Associate the application with its node
        clientApp->Setup(clientSocket, nodeIp, allNodeIps, sendInterval, packetSize, port);
        node->AddApplication(clientApp); // Add the application to the node's application container
        clientApps.Add(clientApp);       // Add to our list for starting/stopping
    }
    // Start clients after 1 second to allow routing protocols to settle
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    // Enable NetAnim tracing for visualization (optional)
    AnimationInterface anim("mobile_aodv_udp.xml");

    // Set simulation stop time and run
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

} // namespace ns3