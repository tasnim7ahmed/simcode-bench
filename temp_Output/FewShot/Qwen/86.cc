#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/packet.h"
#include "ns3/udp-socket-factory.h"

using namespace ns3;

class PacketReceiver : public Application {
public:
    static TypeId GetTypeId() {
        static TypeId tid = TypeId("PacketReceiver")
            .SetParent<Application>()
            .AddConstructor<PacketReceiver>()
            .AddAttribute("Port", "Listening port",
                          UintegerValue(9),
                          MakeUintegerAccessor(&PacketReceiver::m_port),
                          MakeUintegerChecker<uint16_t>());
        return tid;
    }

    PacketReceiver() {}
    ~PacketReceiver() {}

private:
    void StartApplication() override {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
        m_socket->Bind(local);
        m_socket->SetRecvCallback(MakeCallback(&PacketReceiver::ReceivePacket, this));
    }

    void StopApplication() override {
        if (m_socket) {
            m_socket->Close();
            m_socket = nullptr;
        }
    }

    void ReceivePacket(Ptr<Socket> socket) {
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(from))) {
            Time rxTime = Simulator::Now();
            uint8_t buffer[8];
            packet->CopyData(buffer, 8);
            Time txTime = NanoSeconds(*reinterpret_cast<uint64_t*>(buffer));
            Time delay = rxTime - txTime;
            NS_LOG_INFO("Received packet at " << rxTime.As(Time::S)
                        << " with delay " << delay.As(Time::MS));
        }
    }

    uint16_t m_port;
    Ptr<Socket> m_socket;
};

class PacketSender : public Application {
public:
    static TypeId GetTypeId() {
        static TypeId tid = TypeId("PacketSender")
            .SetParent<Application>()
            .AddConstructor<PacketSender>()
            .AddAttribute("Destination", "Destination address",
                          Ipv4AddressValue(),
                          MakeIpv4AddressAccessor(&PacketSender::m_destAddr),
                          MakeIpv4AddressChecker())
            .AddAttribute("Port", "Destination port",
                          UintegerValue(9),
                          MakeUintegerAccessor(&PacketSender::m_destPort),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("PacketSize", "Size of packets to send",
                          UintegerValue(1024),
                          MakeUintegerAccessor(&PacketSender::m_packetSize),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("Interval", "Interval between packets",
                          TimeValue(Seconds(1.0)),
                          MakeTimeAccessor(&PacketSender::m_interval),
                          MakeTimeChecker())
            .AddAttribute("MaxPackets", "Number of packets to send",
                          UintegerValue(5),
                          MakeUintegerAccessor(&PacketSender::m_maxPackets),
                          MakeUintegerChecker<uint32_t>());
        return tid;
    }

    PacketSender() {}
    ~PacketSender() {}

private:
    void StartApplication() override {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        m_sendEvent = Simulator::Schedule(Seconds(0), &PacketSender::SendPacket, this);
    }

    void StopApplication() override {
        if (m_sendEvent.IsRunning()) {
            Simulator::Cancel(m_sendEvent);
        }
        if (m_socket) {
            m_socket->Close();
            m_socket = nullptr;
        }
    }

    void SendPacket() {
        if (m_packetsSent >= m_maxPackets) {
            return;
        }

        Ptr<Packet> packet = Create<Packet>(m_packetSize);
        uint64_t txTimestamp = Simulator::Now().GetNanoSeconds();
        packet->AddHeader(Create<ByteTag>(txTimestamp));

        m_socket->SendTo(packet, 0, InetSocketAddress(m_destAddr, m_destPort));
        NS_LOG_INFO("Sent packet at " << Simulator::Now().As(Time::S)
                    << " (seq=" << m_packetsSent << ")");

        m_packetsSent++;
        if (m_packetsSent < m_maxPackets) {
            m_sendEvent = Simulator::Schedule(m_interval, &PacketSender::SendPacket, this);
        }
    }

    class ByteTag : public Tag {
    public:
        ByteTag(uint64_t t = 0) : timestamp(t) {}
        static TypeId GetTypeId() {
            static TypeId tid = TypeId("ByteTag")
                .SetParent<Tag>()
                .AddConstructor<ByteTag>();
            return tid;
        }
        TypeId GetInstanceTypeId() const override { return GetTypeId(); }
        uint32_t GetSerializedSize() const override { return 8; }
        void Serialize(TagBuffer i) const override {
            i.WriteU64(timestamp);
        }
        void Deserialize(TagBuffer i) override {
            timestamp = i.ReadU64();
        }
        void Print(std::ostream &os) const override {
            os << "Timestamp=" << timestamp;
        }
        uint64_t timestamp;
    };

    Ipv4Address m_destAddr;
    uint16_t m_destPort;
    uint32_t m_packetSize;
    Time m_interval;
    uint32_t m_maxPackets;
    uint32_t m_packetsSent;
    Ptr<Socket> m_socket;
    EventId m_sendEvent;
};

int main(int argc, char *argv[]) {
    LogComponentEnable("PacketReceiver", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSender", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate6Mbps"));

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    PacketReceiverHelper receiverHelper;
    receiverHelper.SetAttribute("Port", UintegerValue(9));
    ApplicationContainer receiverApp = receiverHelper.Install(nodes.Get(1));
    receiverApp.Start(Seconds(0.0));
    receiverApp.Stop(Seconds(10.0));

    PacketSenderHelper senderHelper;
    senderHelper.SetAttribute("Destination", Ipv4Value(interfaces.GetAddress(1)));
    senderHelper.SetAttribute("Port", UintegerValue(9));
    senderHelper.SetAttribute("PacketSize", UintegerValue(1024));
    senderHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    senderHelper.SetAttribute("MaxPackets", UintegerValue(5));
    ApplicationContainer senderApp = senderHelper.Install(nodes.Get(0));
    senderApp.Start(Seconds(1.0));
    senderApp.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}