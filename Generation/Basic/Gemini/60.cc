#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/tcp-header.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpLargeTransferSimulation");

/**
 * \brief Custom TCP Sender Application.
 *
 * This application implements a TCP sender that sends a specified total number of bytes
 * to a peer address and port. It uses the ns-3 Socket API directly.
 */
class BulkTcpSender : public Application
{
public:
    BulkTcpSender();
    ~BulkTcpSender() override;

    /**
     * \brief Setup the sender application.
     * \param socket The TCP socket to use for sending.
     * \param peerAddress The address of the remote peer (PacketSink).
     * \param peerPort The port of the remote peer (PacketSink).
     * \param totalBytes The total number of bytes to send.
     */
    void Setup(Ptr<Socket> socket, Address peerAddress, uint16_t peerPort, uint64_t totalBytes);

    /**
     * \brief Set the size of each packet sent by the application.
     * \param packetSize The size in bytes.
     */
    void SetPacketSize(uint32_t packetSize);

    /**
     * \brief Get the internal socket used by the application.
     * \return A pointer to the socket.
     */
    Ptr<Socket> GetSocket() const;

private:
    void StartApplication() override;
    void StopApplication() override;

    /**
     * \brief Tries to send data if available space in socket buffer and bytes remaining.
     */
    void SendData();

    /**
     * \brief Callback for successful socket connection.
     * \param socket The connected socket.
     */
    void ConnectionSucceeded(Ptr<Socket> socket);

    /**
     * \brief Callback for failed socket connection.
     * \param socket The socket.
     */
    void ConnectionFailed(Ptr<Socket> socket);

    /**
     * \brief Callback for when bytes are successfully sent from the socket's buffer.
     * \param socket The socket.
     * \param numBytes The number of bytes sent.
     */
    void DataSent(Ptr<Socket> socket, uint32_t numBytes);

    Ptr<Socket> m_socket;     //!< The TCP socket
    Address m_peerAddress;    //!< Remote peer address
    uint16_t m_peerPort;      //!< Remote peer port
    uint64_t m_totalBytes;    //!< Total bytes to send
    uint64_t m_sentBytes;     //!< Bytes sent so far
    uint32_t m_packetSize;    //!< Size of each packet
};

NS_LOG_COMPONENT_DEFINE("BulkTcpSender");

BulkTcpSender::BulkTcpSender()
    : m_socket(nullptr),
      m_peerAddress(),
      m_peerPort(0),
      m_totalBytes(0),
      m_sentBytes(0),
      m_packetSize(1448) // Default TCP MSS for Ethernet
{
}

BulkTcpSender::~BulkTcpSender()
{
    m_socket = nullptr;
}

void BulkTcpSender::Setup(Ptr<Socket> socket, Address peerAddress, uint16_t peerPort,
                          uint64_t totalBytes)
{
    m_socket = socket;
    m_peerAddress = peerAddress;
    m_peerPort = peerPort;
    m_totalBytes = totalBytes;
}

void BulkTcpSender::SetPacketSize(uint32_t packetSize)
{
    m_packetSize = packetSize;
}

Ptr<Socket> BulkTcpSender::GetSocket() const
{
    return m_socket;
}

void BulkTcpSender::StartApplication()
{
    NS_LOG_INFO("BulkTcpSender starting at " << Simulator::Now().GetSeconds() << "s on Node " << GetNode()->GetId());
    m_socket->SetConnectCallback(MakeCallback(&BulkTcpSender::ConnectionSucceeded, this),
                                 MakeCallback(&BulkTcpSender::ConnectionFailed, this));
    m_socket->Bind(); // Bind to any local address/port
    m_socket->Connect(m_peerAddress);
    m_socket->SetSendCallback(MakeCallback(&BulkTcpSender::DataSent, this));
}

void BulkTcpSender::StopApplication()
{
    NS_LOG_INFO("BulkTcpSender stopping at " << Simulator::Now().GetSeconds() << "s on Node " << GetNode()->GetId() << ", total bytes sent: " << m_sentBytes);
    if (m_socket)
    {
        m_socket->Close();
        m_socket = nullptr;
    }
}

void BulkTcpSender::ConnectionSucceeded(Ptr<Socket> socket)
{
    NS_LOG_INFO("BulkTcpSender connected to " << InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4() << ":" << InetSocketAddress::ConvertFrom(m_peerAddress).GetPort());
    SendData(); // Start sending data immediately
}

void BulkTcpSender::ConnectionFailed(Ptr<Socket> socket)
{
    NS_LOG_ERROR("BulkTcpSender connection failed to " << InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4() << ":" << InetSocketAddress::ConvertFrom(m_peerAddress).GetPort());
}

void BulkTcpSender::DataSent(Ptr<Socket> socket, uint32_t numBytes)
{
    // This callback means bytes have been removed from the send buffer (acknowledged/sent to lower layer).
    // Try sending more data if available space and bytes remain.
    SendData();
}

void BulkTcpSender::SendData()
{
    while (m_sentBytes < m_totalBytes && m_socket->GetTxAvailable() > 0)
    {
        uint32_t bytesToSend = std::min((uint64_t)m_packetSize, m_totalBytes - m_sentBytes);
        Ptr<Packet> p = Create<Packet>(bytesToSend);
        int actualSent = m_socket->Send(p);

        if (actualSent > 0)
        {
            m_sentBytes += actualSent;
            // NS_LOG_DEBUG("Sent " << actualSent << " bytes, total sent: " << m_sentBytes << " at " << Simulator::Now().GetSeconds() << "s");
        }
        else if (actualSent == -1) // Error, typically EWOULDBLOCK or equivalent
        {
            break; // Stop trying to send, wait for DataSent callback
        }
    }

    if (m_sentBytes >= m_totalBytes)
    {
        NS_LOG_INFO("BulkTcpSender finished sending " << m_totalBytes << " bytes at " << Simulator::Now().GetSeconds() << "s on Node " << GetNode()->GetId());
    }
}

/**
 * \brief Callback function to trace TCP congestion window changes.
 * \param stream Output stream to write trace data to.
 * \param oldCwnd The previous congestion window size.
 * \param newCwnd The new congestion window size.
 */
static void CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

/**
 * \brief Callback function to trace application-level packet receptions.
 * \param stream Output stream to write trace data to.
 * \param packet The received packet.
 * \param address The sender's address.
 */
static void RxPacket(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, const Address &address)
{
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << packet->GetSize() << std::endl;
}


int main(int argc, char *argv[])
{
    // Set default TCP type to NewReno
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));
    // Set initial congestion window (default is 10 segments)
    Config::SetDefault("ns3::TcpSocketBase::InitialCwnd", UintegerValue(10));
    // Set maximum segment size (MSS) for TCP
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448)); // Typical Ethernet MSS

    // Enable logging components
    LogComponentEnable("TcpLargeTransferSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("BulkTcpSender", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Command line arguments
    double simulationTime = 20.0; // seconds
    uint64_t totalBytesToSend = 2000000; // 2 MB
    uint32_t packetSize = 1448; // Max segment size for TCP

    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.AddValue("totalBytes", "Total bytes to send from sender to receiver", totalBytesToSend);
    cmd.AddValue("packetSize", "Size of packets sent by the application", packetSize);
    cmd.Parse(argc, argv);

    // Create nodes: n0, n1, n2
    NodeContainer nodes;
    nodes.Create(3);

    Ptr<Node> n0 = nodes.Get(0);
    Ptr<Node> n1 = nodes.Get(1);
    Ptr<Node> n2 = nodes.Get(2);

    // Configure point-to-point links with 10 Mbps bandwidth and 10 ms delay
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices01 = p2p.Install(n0, n1);
    NetDeviceContainer devices12 = p2p.Install(n1, n2);

    // Install internet stack on all nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses to interfaces
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0"); // Network for n0-n1 link
    Ipv4InterfaceContainer interfaces01 = ipv4.Assign(devices01);

    ipv4.SetBase("10.1.2.0", "255.255.255.0"); // Network for n1-n2 link
    Ipv4InterfaceContainer interfaces12 = ipv4.Assign(devices12);

    // Populate routing tables using global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup PacketSink (Receiver) on node n2
    uint16_t sinkPort = 50000;
    // The sink address is the IP address of n2 on the n1-n2 link
    Address sinkAddress(InetSocketAddress(interfaces12.GetAddress(1), sinkPort));

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(n2);
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime + 1.0)); // Ensure sink runs slightly longer than sender

    // Setup Custom BulkTcpSender on node n0
    Ptr<Socket> tcpSocket = Socket::CreateSocket(n0, TcpSocketFactory::GetTypeId());
    Ptr<BulkTcpSender> senderApp = CreateObject<BulkTcpSender>();
    senderApp->Setup(tcpSocket, sinkAddress, sinkPort, totalBytesToSend);
    senderApp->SetPacketSize(packetSize);
    n0->AddApplication(senderApp);
    senderApp->Start(Seconds(1.0)); // Start sender after 1 second
    senderApp->Stop(Seconds(simulationTime)); // Stop sender at simulationTime

    // --- Tracing Configuration ---

    // 1. ASCII Trace for device-level and queue information
    // This typically includes basic queue stats like drops. For deeper queue metrics,
    // one would connect to specific Queue trace sources.
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> asciiStream = asciiTraceHelper.CreateFileStream("tcp-large-transfer.tr");
    p2p.EnableAsciiAll(asciiStream);

    // 2. PCAP Trace for packet capture
    // 'false' means separate files per device (e.g., tcp-large-transfer-0-0.pcap)
    p2p.EnablePcapAll("tcp-large-transfer", false);

    // 3. Congestion Window tracing (on n0's TCP socket)
    Ptr<OutputStreamWrapper> cwndStream = asciiTraceHelper.CreateFileStream("cwnd.txt");
    senderApp->GetSocket()->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&CwndChange, cwndStream));

    // 4. Packet Reception tracing (at application level on PacketSink)
    Ptr<OutputStreamWrapper> rxStream = asciiTraceHelper.CreateFileStream("packet-reception.txt");
    sinkApps.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&RxPacket, rxStream));

    // Run simulation
    Simulator::Stop(Seconds(simulationTime + 2.0)); // Ensure sufficient time for simulation to complete
    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_INFO("Simulation finished.");

    return 0;
}