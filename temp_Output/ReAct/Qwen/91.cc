#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiInterfaceRoutingSimulation");

class PacketSink : public Application {
public:
    static TypeId GetTypeId(void) {
        static TypeId tid = TypeId("PacketSink")
            .SetParent<Application>()
            .AddConstructor<PacketSink>();
        return tid;
    }

    void ReceivePacket(Ptr<Socket> socket) {
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(from))) {
            Ipv4Header ipHdr;
            packet->PeekHeader(ipHdr);
            NS_LOG_UNCOND("Received packet at node " << GetNode()->GetId()
                          << " from " << ipHdr.GetSource()
                          << " to " << ipHdr.GetDestination()
                          << " size: " << packet->GetSize());
        }
    }

private:
    virtual void StartApplication() override {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
        m_socket->Bind(local);
        m_socket->SetRecvCallback(MakeCallback(&PacketSink::ReceivePacket, this));
    }

    virtual void StopApplication() override {}

    Ptr<Socket> m_socket;
};

class UdpSender : public Application {
public:
    static TypeId GetTypeId(void) {
        static TypeId tid = TypeId("UdpSender")
            .SetParent<Application>()
            .AddConstructor<UdpSender>()
            .AddAttribute("Remote", "The destination address",
                          Ipv4AddressValue(),
                          MakeIpv4AddressAccessor(&UdpSender::m_remote),
                          MakeIpv4AddressChecker())
            .AddAttribute("Interval", "Time between packets",
                          TimeValue(Seconds(1.0)),
                          MakeTimeAccessor(&UdpSender::m_interval),
                          MakeTimeChecker())
            .AddAttribute("PacketSize", "Size of each packet",
                          UintegerValue(512),
                          MakeUintegerAccessor(&UdpSender::m_size),
                          MakeUintegerChecker<uint32_t>());
        return tid;
    }

    UdpSender() : m_socket(nullptr), m_sendEvent(), m_interfaceIndex(0) {}

    ~UdpSender() {
        m_socket = nullptr;
    }

    void SwitchInterface(uint32_t interfaceIndex) {
        m_interfaceIndex = interfaceIndex;
        if (m_socket) {
            m_socket->Close();
            m_socket = nullptr;
        }

        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        m_socket->BindToNetDevice(GetNode()->GetDevice(m_interfaceIndex));
        m_socket->Connect(InetSocketAddress(m_remote, 9));
    }

    void SendPacket() {
        Ptr<Packet> packet = Create<Packet>(m_size);
        m_socket->Send(packet);

        // Schedule next send on a different interface
        m_interfaceIndex = (m_interfaceIndex + 1) % 2;
        m_sendEvent = Simulator::Schedule(m_interval, &UdpSender::SendPacket, this);
    }

private:
    virtual void StartApplication() override {
        SwitchInterface(m_interfaceIndex);
        m_sendEvent = Simulator::ScheduleNow(&UdpSender::SendPacket, this);
    }

    virtual void StopApplication() override {
        if (m_sendEvent.IsRunning()) Simulator::Cancel(m_sendEvent);
        if (m_socket) m_socket->Close();
    }

    Ipv4Address m_remote;
    uint32_t m_size;
    Time m_interval;
    EventId m_sendEvent;
    Ptr<Socket> m_socket;
    uint32_t m_interfaceIndex;
};

int main(int argc, char *argv[]) {
    LogComponentEnable("MultiInterfaceRoutingSimulation", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4); // Source (0), Router1 (1), Router2 (2), Destination (3)

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices01 = p2p.Install(nodes.Get(0), nodes.Get(1)); // Source <-> R1
    NetDeviceContainer devices02 = p2p.Install(nodes.Get(0), nodes.Get(2)); // Source <-> R2
    NetDeviceContainer devices13 = p2p.Install(nodes.Get(1), nodes.Get(3)); // R1 <-> Dest
    NetDeviceContainer devices23 = p2p.Install(nodes.Get(2), nodes.Get(3)); // R2 <-> Dest

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);

    address.NewNetwork();
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces02 = address.Assign(devices02);

    address.NewNetwork();
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces13 = address.Assign(devices13);

    address.NewNetwork();
    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces23 = address.Assign(devices23);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Add static routes with different metrics for source node (node 0)
    Ptr<Ipv4> ipv4Src = nodes.Get(0)->GetObject<Ipv4>();
    Ipv4Address destNetwork = "10.1.4.0";
    Ipv4Mask networkMask("255.255.255.0");
    Ipv4Address nextHop1 = interfaces01.GetAddress(1); // via R1
    Ipv4Address nextHop2 = interfaces02.GetAddress(1); // via R2

    // Route through R1 with lower metric
    ipv4Src->AddRoute(destNetwork, networkMask, nextHop1, 1, 0);

    // Route through R2 with higher metric (backup)
    ipv4Src->AddRoute(destNetwork, networkMask, nextHop2, 2, 0);

    // Install applications
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(3));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    UdpSenderHelper sender;
    sender.SetAttribute("Remote", Ipv4AddressValue(interfaces23.GetAddress(1)));
    sender.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    sender.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer senderApp = sender.Install(nodes.Get(0));
    senderApp.Start(Seconds(1.0));
    senderApp.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}