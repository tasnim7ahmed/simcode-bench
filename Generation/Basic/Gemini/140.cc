#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleTcpSocketSimulation");

// Global variables for client state
Ptr<Socket> g_clientSocket;
uint32_t g_bytesSent = 0;
uint32_t g_totalBytesToSend = 100000; // Total bytes for the client to send (e.g., 100KB)
uint32_t g_packetSize = 1024;        // Size of each packet to attempt to send

// Global variable for server state
uint32_t g_bytesReceived = 0;

// Function prototypes
void ClientSend();
void ClientConnected(Ptr<Socket> socket);
void ClientDataSend(Ptr<Socket> socket, uint32_t txSpace);
void ServerAccept(Ptr<Socket> newSocket, const InetSocketAddress &peerAddress);
void ServerReceive(Ptr<Socket> socket);

// Client Send Function: attempts to send data
void ClientSend()
{
    while (g_bytesSent < g_totalBytesToSend && g_clientSocket->GetTxAvailable() > 0)
    {
        uint32_t sendSize = std::min(g_packetSize, g_totalBytesToSend - g_bytesSent);
        Ptr<Packet> p = Create<Packet>(sendSize);
        int actualSent = g_clientSocket->Send(p);

        if (actualSent > 0)
        {
            g_bytesSent += actualSent;
            NS_LOG_INFO(Simulator::Now().GetSeconds() << "s: Client sent " << actualSent << " bytes, Total sent: " << g_bytesSent << " bytes.");
        }
        else if (actualSent < 0)
        {
            // Error occurred, log and break
            NS_LOG_ERROR(Simulator::Now().GetSeconds() << "s: Client failed to send packet. Error: " << g_clientSocket->GetErrno());
            break;
        }
        else // actualSent == 0 implies send buffer is full
        {
            NS_LOG_DEBUG(Simulator::Now().GetSeconds() << "s: Client send buffer full, pausing sending.");
            break; // Stop sending for now, wait for TxAvailable callback
        }
    }

    if (g_bytesSent >= g_totalBytesToSend)
    {
        NS_LOG_INFO(Simulator::Now().GetSeconds() << "s: Client finished sending " << g_bytesSent << " bytes. Closing socket.");
        g_clientSocket->Close();
    }
}

// Callback for when client's send buffer space is available
void ClientDataSend(Ptr<Socket> socket, uint32_t txSpace)
{
    NS_LOG_DEBUG(Simulator::Now().GetSeconds() << "s: Client send buffer available. TxSpace: " << txSpace);
    ClientSend(); // Try sending again
}

// Callback for when client successfully connects
void ClientConnected(Ptr<Socket> socket)
{
    NS_LOG_INFO(Simulator::Now().GetSeconds() << "s: Client successfully connected to " << socket->GetPeerName());
    g_clientSocket = socket; // Store the socket pointer

    // Set callback for when send buffer space is available
    g_clientSocket->SetSendCallback(MakeCallback(&ClientDataSend));

    // Start sending data after connection is established
    ClientSend();
}

// Callback for server accepting a new connection
void ServerAccept(Ptr<Socket> newSocket, const InetSocketAddress &peerAddress)
{
    NS_LOG_INFO(Simulator::Now().GetSeconds() << "s: Server accepted connection from " << peerAddress);
    // Set receive callback for the newly accepted socket
    newSocket->SetRecvCallback(MakeCallback(&ServerReceive));
}

// Callback for server receiving data
void ServerReceive(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
        uint32_t bytesReceived = packet->GetSize();
        g_bytesReceived += bytesReceived;
        NS_LOG_INFO(Simulator::Now().GetSeconds() << "s: Server received " << bytesReceived << " bytes from " << InetSocketAddress::ConvertFrom(from) << ", Total received: " << g_bytesReceived << " bytes.");
    }
}

int main(int argc, char *argv[])
{
    // Enable logging for this component and potentially TCP
    LogComponentEnable("SimpleTcpSocketSimulation", LOG_LEVEL_INFO);
    // LogComponentEnable("ns3::TcpSocketBase", LOG_LEVEL_DEBUG);
    // LogComponentEnable("ns3::TcpL4Protocol", LOG_LEVEL_DEBUG);

    // Command line arguments (optional)
    CommandLine cmd(__FILE__);
    cmd.AddValue("totalBytes", "Total bytes for the client to send", g_totalBytesToSend);
    cmd.AddValue("packetSize", "Size of each packet to send", g_packetSize);
    cmd.Parse(argc, argv);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2); // node 0 is client, node 1 is server

    // Create point-to-point link characteristics
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    // Install internet stack on nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Server (Node 1) setup
    TypeId tcpTid = TypeId::LookupByName("ns3::TcpSocketFactory");
    Ptr<Socket> serverListenSocket = Socket::CreateSocket(nodes.Get(1), tcpTid);
    NS_ASSERT_MSG(serverListenSocket != 0, "Failed to create server socket");

    InetSocketAddress serverAddress(Ipv4Address::GetAny(), 9999); // Listen on any address on port 9999
    serverListenSocket->Bind(serverAddress);
    serverListenSocket->Listen();
    NS_LOG_INFO(Simulator::Now().GetSeconds() << "s: Server listening on " << serverAddress);

    // Set callback for accepting new connections
    serverListenSocket->SetAcceptCallback(
        MakeNullCallback<bool, Ptr<Socket>, const InetSocketAddress &>(), // Connection filter (none)
        MakeCallback(&ServerAccept)); // Accept callback

    // Client (Node 0) setup
    Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), tcpTid);
    NS_ASSERT_MSG(clientSocket != 0, "Failed to create client socket");

    // Connect to the server's IP address and port
    InetSocketAddress serverRemoteAddress(interfaces.GetAddress(1), 9999);
    clientSocket->Connect(serverRemoteAddress);
    NS_LOG_INFO(Simulator::Now().GetSeconds() << "s: Client attempting to connect to " << serverRemoteAddress);

    // Set connection callback (on successful connection)
    clientSocket->SetConnectCallback(
        MakeCallback(&ClientConnected),       // On success
        MakeNullCallback<void, Ptr<Socket>>()); // On failure (we ignore failure for simplicity in this example)

    // Set simulation duration
    Simulator::Stop(Seconds(20.0)); 

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_INFO("Simulation finished.");
    NS_LOG_INFO("Total bytes sent by client: " << g_bytesSent);
    NS_LOG_INFO("Total bytes received by server: " << g_bytesReceived);

    return 0;
}