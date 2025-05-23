#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/bulk-send-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/tcp-socket.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpRttSimulation");

// Callback function to print Round-Trip Time (RTT)
void RttTrace(Time rtt)
{
    // Log the current simulation time and the RTT value in milliseconds unconditionally.
    // This ensures the RTT is printed to standard output regardless of log level.
    NS_LOG_UNCOND("At time " << Simulator::Now().GetSeconds() << "s, RTT = " << rtt.GetMilliSeconds() << "ms");
}

// Helper function to connect the RTT trace source to the TCP socket
// This function is scheduled to execute after the client application starts and creates its socket.
void TraceSocketRTT(Ptr<Socket> clientSocket)
{
    // Attempt to dynamically cast the generic Ptr<Socket> to Ptr<TcpSocket>.
    // The "RTT" trace source is specific to TcpSocket.
    Ptr<TcpSocket> tcpClientSocket = DynamicCast<TcpSocket>(clientSocket);
    if (tcpClientSocket)
    {
        // If the cast is successful, connect the "RTT" trace source to our RttTrace callback.
        tcpClientSocket->TraceConnectWithoutContext("RTT", MakeCallback(&RttTrace));
        NS_LOG_INFO("RTT trace connected to client TCP socket.");
    }
    else
    {
        // Log a warning if the socket is not a TCP socket, indicating RTT won't be logged.
        NS_LOG_WARN("Could not cast socket to TcpSocket for RTT trace. RTT will not be logged.");
    }
}

int main(int argc, char *argv[])
{
    // Set the default TCP congestion control algorithm to TCP New Reno.
    // This provides a common and predictable TCP behavior for the simulation.
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));

    // Enable logging for our custom component "TcpRttSimulation" at INFO level.
    // This allows custom NS_LOG_INFO messages defined within this script to be displayed.
    LogComponentEnable("TcpRttSimulation", LOG_LEVEL_INFO);

    // Create two nodes for the simulation: one client and one server.
    NodeContainer nodes;
    nodes.Create(2);

    Ptr<Node> clientNode = nodes.Get(0); // Assign client node
    Ptr<Node> serverNode = nodes.Get(1); // Assign server node

    // Configure the PointToPointHelper for the link properties.
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps")); // Set the link data rate to 5 Mbps
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));   // Set the link propagation delay to 10 ms

    // Install the point-to-point network devices on the nodes.
    NetDeviceContainer devices;
    devices = p2p.Install(nodes);

    // Install the Internet stack (IPv4, TCP, UDP, etc.) on both nodes.
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the network interfaces.
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0"); // Define the network address and subnet mask
    Ipv4InterfaceContainer interfaces = address.Assign(devices); // Assign addresses to the installed devices

    // Configure the server application (PacketSink).
    // The PacketSink listens for incoming TCP traffic.
    uint16_t port = 9; // Use port 9 (discard port) for the application
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), port)); // Listen on any IP address on the specified port
    ApplicationContainer serverApps = sinkHelper.Install(serverNode); // Install PacketSink on the server node
    serverApps.Start(Seconds(1.0)); // Server application starts at 1 second
    serverApps.Stop(Seconds(10.0)); // Server application stops at 10 seconds (end of simulation)

    // Configure the client application (BulkSend).
    // The BulkSend application sends continuous TCP data.
    BulkSendHelper clientHelper("ns3::TcpSocketFactory",
                                InetSocketAddress(interfaces.GetAddress(1), port)); // Connect to the server's IP address and port
    clientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Set to 0 to send data indefinitely until the application stops
    clientHelper.SetAttribute("SendSize", UintegerValue(1000)); // Set the size of each TCP segment sent to 1000 bytes

    ApplicationContainer clientApps = clientHelper.Install(clientNode); // Install BulkSend on the client node
    clientApps.Start(Seconds(2.0)); // Client application starts sending at 2 seconds
    clientApps.Stop(Seconds(10.0)); // Client application stops at 10 seconds

    // Get a pointer to the BulkSendApplication instance.
    Ptr<BulkSendApplication> bulkSendApp = DynamicCast<BulkSendApplication>(clientApps.Get(0));
    // Schedule the TraceSocketRTT function to connect the RTT trace.
    // The BulkSendApplication creates its internal TCP socket during its StartApplication() method.
    // Therefore, we schedule this connection slightly after the client application's start time
    // to ensure the socket object is instantiated and accessible. NanoSeconds(1) ensures it's
    // in the same simulation tick or immediately after.
    Simulator::Schedule(clientApps.Get(0)->GetStartTime() + NanoSeconds(1), &TraceSocketRTT, bulkSendApp->GetSocket());

    // Set the global simulation stop time.
    Simulator::Stop(Seconds(10.0));

    // Run the simulation.
    Simulator::Run();

    // Clean up simulation resources.
    Simulator::Destroy();

    return 0;
}