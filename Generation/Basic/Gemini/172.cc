#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/udp-socket-factory.h"

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <algorithm> // For std::accumulate (might be in numeric)
#include <numeric>   // For std::accumulate
#include <iomanip>   // For std::setprecision

using namespace ns3;

// Global metrics storage
// Maps (sourceNodeId, sequenceNumber) to the Time when the packet was sent
std::map<std::pair<uint32_t, uint32_t>, Time> g_sentPacketTimestamps; 
uint32_t g_totalPacketsSent = 0;
uint32_t g_totalPacketsReceived = 0;
std::vector<double> g_delays; // Stores individual packet delays in seconds

// Custom Header to carry sequence number, send time, and source node ID
class MyHeader : public Header
{
public:
    static TypeId GetTypeId ();
    MyHeader ();
    virtual TypeId GetInstanceTypeId () const;
    virtual void Serialize (Buffer::Iterator start) const;
    virtual uint32_t Deserialize (Buffer::Iterator start);
    virtual uint32_t GetSerializedSize () const;
    virtual void Print (std::ostream &os) const;

    void SetSequenceNumber (uint32_t seq);
    uint32_t GetSequenceNumber () const;
    void SetSendTime (Time time);
    Time GetSendTime () const;
    void SetSourceNodeId (uint32_t id);
    uint32_t GetSourceNodeId () const;

private:
    uint32_t m_sequenceNumber;
    uint64_t m_sendTimeNs; // Storing as nanoseconds for serialization
    uint32_t m_sourceNodeId;
};

NS_OBJECT_ENSURE_REGISTERED(MyHeader);

TypeId MyHeader::GetTypeId ()
{
    static TypeId tid = TypeId ("ns3::MyHeader")
        .SetParent<Header> ()
        .AddConstructor<MyHeader> ();
    return tid;
}

MyHeader::MyHeader () : m_sequenceNumber (0), m_sendTimeNs (0), m_sourceNodeId (0)
{}

TypeId MyHeader::GetInstanceTypeId () const
{
    return GetTypeId ();
}

void MyHeader::Serialize (Buffer::Iterator start) const
{
    start.WriteHtonU32 (m_sequenceNumber);
    start.WriteHtonU64 (m_sendTimeNs);
    start.WriteHtonU32 (m_sourceNodeId);
}

uint32_t MyHeader::Deserialize (Buffer::Iterator start)
{
    m_sequenceNumber = start.ReadNtohU32 ();
    m_sendTimeNs = start.ReadNtohU64 ();
    m_sourceNodeId = start.ReadNtohU32 ();
    return GetSerializedSize ();
}

uint32_t MyHeader::GetSerializedSize () const
{
    return 4 /*sequenceNumber*/ + 8 /*sendTimeNs*/ + 4 /*sourceNodeId*/;
}

void MyHeader::Print (std::ostream &os) const
{
    os << "MyHeader: seq=" << m_sequenceNumber << ", sendTimeNs=" << m_sendTimeNs << ", srcNodeId=" << m_sourceNodeId;
}

void MyHeader::SetSequenceNumber (uint32_t seq) { m_sequenceNumber = seq; }
uint32_t MyHeader::GetSequenceNumber () const { return m_sequenceNumber; }
void MyHeader::SetSendTime (Time time) { m_sendTimeNs = time.GetNanoSeconds (); }
Time MyHeader::GetSendTime () const { return NanoSeconds (m_sendTimeNs); }
void MyHeader::SetSourceNodeId (uint32_t id) { m_sourceNodeId = id; }
uint32_t MyHeader::GetSourceNodeId () const { return m_sourceNodeId; }


// Custom UDP Client Application
class MyUdpClient : public Application
{
public:
    static TypeId GetTypeId ();
    MyUdpClient ();
    void SetRemote (Ipv4Address ip, uint16_t port);
    void SetInterval (Time interval);
    void SetPacketSize (uint32_t packetSize);

protected:
    virtual void StartApplication ();
    virtual void StopApplication ();

private:
    void SendPacket ();

    Ptr<Socket> m_socket;
    Ipv4Address m_peerAddress;
    uint16_t m_peerPort;
    Time m_interval;
    uint32_t m_packetSize;
    uint32_t m_sequenceNumber;
    EventId m_sendEvent;
};

NS_OBJECT_ENSURE_REGISTERED(MyUdpClient);

TypeId MyUdpClient::GetTypeId ()
{
    static TypeId tid = TypeId ("ns3::MyUdpClient")
        .SetParent<Application> ()
        .AddConstructor<MyUdpClient> ()
        .AddAttribute ("RemoteAddress",
                       "The destination Ipv4Address.",
                       Ipv4AddressValue (),
                       MakeIpv4AddressAccessor (&MyUdpClient::m_peerAddress),
                       MakeIpv4AddressChecker ())
        .AddAttribute ("RemotePort",
                       "The destination port to send packets to.",
                       UintegerValue (1000),
                       MakeUintegerAccessor (&MyUdpClient::m_peerPort),
                       MakeUintegerChecker<uint16_t> ())
        .AddAttribute ("Interval",
                       "The time interval between packets.",
                       TimeValue (Seconds (1.0)),
                       MakeTimeAccessor (&MyUdpClient::m_interval),
                       MakeTimeChecker ())
        .AddAttribute ("PacketSize",
                       "The size of packets to send (bytes).",
                       UintegerValue (1024),
                       MakeUintegerAccessor (&MyUdpClient::m_packetSize),
                       MakeUintegerChecker<uint32_t> ());
    return tid;
}

MyUdpClient::MyUdpClient ()
    : m_socket (nullptr),
      m_peerAddress (),
      m_peerPort (0),
      m_interval (Seconds (0)),
      m_packetSize (0),
      m_sequenceNumber (0)
{}

void MyUdpClient::SetRemote (Ipv4Address ip, uint16_t port)
{
    m_peerAddress = ip;
    m_peerPort = port;
}

void MyUdpClient::SetInterval (Time interval)
{
    m_interval = interval;
}

void MyUdpClient::SetPacketSize (uint32_t packetSize)
{
    m_packetSize = packetSize;
}

void MyUdpClient::StartApplication ()
{
    if (!m_socket)
    {
        m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
        m_socket->Bind (); // Bind to any available port
        m_socket->Connect (InetSocketAddress (m_peerAddress, m_peerPort));
    }
    m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket>> ()); // No receive expected for client
    m_socket->SetAllowBroadcast (true);
    m_sendEvent = Simulator::Schedule (Seconds (0.0), &MyUdpClient::SendPacket, this);
}

void MyUdpClient::StopApplication ()
{
    Simulator::Cancel (m_sendEvent);
    if (m_socket)
    {
        m_socket->Close ();
        m_socket = nullptr;
    }
}

void MyUdpClient::SendPacket ()
{
    // The payload size is total packet size minus the size of our custom header
    uint32_t payloadSize = m_packetSize - MyHeader ().GetSerializedSize (); 
    Ptr<Packet> packet = Create<Packet> (payloadSize); 
    
    MyHeader header;
    header.SetSequenceNumber (m_sequenceNumber);
    header.SetSendTime (Simulator::Now ());
    header.SetSourceNodeId (GetNode ()->GetId ());
    packet->AddHeader (header); // Add custom header to the packet

    // Record packet sent details for metrics
    g_sentPacketTimestamps[std::make_pair (GetNode ()->GetId (), m_sequenceNumber)] = Simulator::Now ();
    g_totalPacketsSent++;

    m_socket->Send (packet);
    m_sequenceNumber++;
    
    // Schedule next packet transmission
    m_sendEvent = Simulator::Schedule (m_interval, &MyUdpClient::SendPacket, this);
}

// Custom UDP Sink Application
class MyUdpSink : public Application
{
public:
    static TypeId GetTypeId ();
    MyUdpSink ();
    void SetLocal (Ipv4Address ip, uint16_t port);

protected:
    virtual void StartApplication ();
    virtual void StopApplication ();

private:
    void HandleRead (Ptr<Socket> socket);

    Ptr<Socket> m_socket;
    Ipv4Address m_localAddress;
    uint16_t m_localPort;
};

NS_OBJECT_ENSURE_REGISTERED(MyUdpSink);

TypeId MyUdpSink::GetTypeId ()
{
    static TypeId tid = TypeId ("ns3::MyUdpSink")
        .SetParent<Application> ()
        .AddConstructor<MyUdpSink> ()
        .AddAttribute ("LocalAddress",
                       "The address of the local interface.",
                       Ipv4AddressValue (),
                       MakeIpv4AddressAccessor (&MyUdpSink::m_localAddress),
                       MakeIpv4AddressChecker ())
        .AddAttribute ("LocalPort",
                       "The local port to listen on.",
                       UintegerValue (1000),
                       MakeUintegerAccessor (&MyUdpSink::m_localPort),
                       MakeUintegerChecker<uint16_t> ());
    return tid;
}

MyUdpSink::MyUdpSink ()
    : m_socket (nullptr),
      m_localAddress (),
      m_localPort (0)
{}

void MyUdpSink::SetLocal (Ipv4Address ip, uint16_t port)
{
    m_localAddress = ip;
    m_localPort = port;
}

void MyUdpSink::StartApplication ()
{
    if (!m_socket)
    {
        m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
        InetSocketAddress local = InetSocketAddress (m_localAddress, m_localPort);
        m_socket->Bind (local);
    }
    m_socket->SetRecvCallback (MakeCallback (&MyUdpSink::HandleRead, this));
}

void MyUdpSink::StopApplication ()
{
    if (m_socket)
    {
        m_socket->Close ();
        m_socket = nullptr;
    }
}

void MyUdpSink::HandleRead (Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
    {
        // Create a copy of the packet and remove header from the copy.
        // This is important because the original packet might still be processed by other modules.
        Ptr<Packet> copyPacket = packet->Copy();
        MyHeader header;
        copyPacket->RemoveHeader (header); // Remove our custom header

        Time sendTime = header.GetSendTime ();
        uint32_t sequenceNumber = header.GetSequenceNumber ();
        uint32_t sourceNodeId = header.GetSourceNodeId ();

        // Look up the sent packet's timestamp to calculate delay
        auto it = g_sentPacketTimestamps.find (std::make_pair (sourceNodeId, sequenceNumber));
        if (it != g_sentPacketTimestamps.end ())
        {
            Time rxTime = Simulator::Now ();
            double delay = (rxTime - sendTime).GetSeconds ();
            g_delays.push_back (delay);
            g_totalPacketsReceived++;
            // Optionally, remove the entry if each packet should only be counted once for PDR
            // g_sentPacketTimestamps.erase(it); 
        }
    }
}


int main (int argc, char *argv[])
{
    uint32_t nNodes = 10;
    double simTime = 30.0; // seconds
    double xMax = 500.0; // meters
    double yMax = 500.0; // meters
    uint32_t packetSize = 1024; // bytes
    double clientInterval = 0.1; // seconds (10 packets/sec per client)
    uint16_t sinkPort = 9;

    // Set up logging for relevant components
    LogComponentEnable ("MyUdpClient", LOG_LEVEL_INFO);
    LogComponentEnable ("MyUdpSink", LOG_LEVEL_INFO);
    LogComponentEnable ("AodvHelper", LOG_LEVEL_WARN); // Log AODV warnings, INFO for more details
    LogComponentEnable ("WifiPhy", LOG_LEVEL_INFO);
    LogComponentEnable ("Ipv4L3Protocol", LOG_LEVEL_WARN);

    // Command line arguments for flexibility
    CommandLine cmd (__FILE__);
    cmd.AddValue ("nNodes", "Number of nodes", nNodes);
    cmd.AddValue ("simTime", "Total simulation time (seconds)", simTime);
    cmd.Parse (argc, argv);

    // Randomness setup for reproducibility
    RngSeedManager::SetSeed (1);
    RngSeedManager::SetRun (2);

    // Create nodes
    NodeContainer nodes;
    nodes.Create (nNodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Mode", StringValue ("RANDOM"),
                               "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=20.0]"), // 1-20 m/s
                               "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"), // 0.5s pause
                               "Bounds", RectangleValue (Rectangle (0, xMax, 0, yMax)));
    mobility.Install (nodes);

    // Wifi setup (Ad-hoc)
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211n_2_4GHZ); // Using 802.11n 2.4GHz standard

    YansWifiPhyHelper phy;
    phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.SetChannel (YansWifiChannelHelper::Create ()); // Create a channel for the PHY layer

    WifiMacHelper mac;
    mac.SetType ("ns3::AdhocWifiMac"); // Set MAC to Ad-hoc mode

    NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

    // AODV routing protocol
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper (aodv); // Install AODV routing
    stack.Install (nodes);

    // Assign IP addresses to devices
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // Enable Pcap tracing
    phy.EnablePcapAll ("adhoc-aodv-udp");

    // UDP Applications setup
    ApplicationContainer clientApps, sinkApps;

    // Install sinks on all nodes, listening on a common port
    for (uint32_t i = 0; i < nNodes; ++i)
    {
        Ptr<MyUdpSink> sink = CreateObject<MyUdpSink> ();
        sink->SetLocal (interfaces.GetAddress (i), sinkPort); // Bind to specific IP of the node
        nodes.Get (i)->AddApplication (sink);
        sink->SetStartTime (Seconds (0.0)); // Sink starts immediately
        sink->SetStopTime (Seconds (simTime)); // Sink stops at the end of simulation
        sinkApps.Add (sink);
    }

    // Install clients: each node sends traffic to a random other node
    Ptr<UniformRandomVariable> appStartTimeRv = CreateObject<UniformRandomVariable>();
    appStartTimeRv->SetAttribute("Min", DoubleValue(0.0));
    appStartTimeRv->SetAttribute("Max", DoubleValue(simTime * 0.1)); // Start apps within first 10% of sim time

    Ptr<UniformRandomVariable> nodePickerRv = CreateObject<UniformRandomVariable>();
    nodePickerRv->SetAttribute("Min", DoubleValue(0.0));
    nodePickerRv->SetAttribute("Max", DoubleValue(nNodes - 1.0));

    for (uint32_t i = 0; i < nNodes; ++i)
    {
        Ptr<Node> clientNode = nodes.Get(i);
        uint32_t destNodeIdx;
        do {
            destNodeIdx = static_cast<uint32_t>(nodePickerRv->GetValue());
        } while (destNodeIdx == i); // Ensure destination is not the client node itself

        Ptr<Node> sinkNode = nodes.Get(destNodeIdx);
        Ipv4Address sinkAddress = interfaces.GetAddress(destNodeIdx);

        Ptr<MyUdpClient> client = CreateObject<MyUdpClient> ();
        client->SetRemote (sinkAddress, sinkPort);
        client->SetInterval (Seconds (clientInterval));
        client->SetPacketSize (packetSize);
        clientNode->AddApplication (client);
        Time startTime = Seconds (appStartTimeRv->GetValue());
        client->SetStartTime (startTime);
        client->SetStopTime (Seconds (simTime)); // Client runs until sim end
        clientApps.Add (client);
    }

    // Run simulation
    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();
    Simulator::Destroy ();

    // Display results
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Simulation Results:" << std::endl;
    std::cout << "  Total packets sent: " << g_totalPacketsSent << std::endl;
    std::cout << "  Total packets received: " << g_totalPacketsReceived << std::endl;

    if (g_totalPacketsSent > 0)
    {
        double pdr = static_cast<double>(g_totalPacketsReceived) / g_totalPacketsSent;
        std::cout << "  Packet Delivery Ratio (PDR): " << pdr * 100 << "%" << std::endl;
    }
    else
    {
        std::cout << "  Packet Delivery Ratio (PDR): N/A (no packets sent)" << std::endl;
    }

    if (!g_delays.empty ())
    {
        double totalDelay = std::accumulate (g_delays.begin (), g_delays.end (), 0.0);
        double avgDelay = totalDelay / g_delays.size ();
        std::cout << "  Average End-to-End Delay: " << avgDelay << " seconds" << std::endl;
    }
    else
    {
        std::cout << "  Average End-to-End Delay: N/A (no packets received)" << std::endl;
    }

    return 0;
}