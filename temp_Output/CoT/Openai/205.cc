#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/packet-sink.h"
#include <fstream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANETAdhocExample");

uint32_t g_packetsLost = 0;
std::vector<double> g_delays;
uint32_t g_rxPackets = 0;

void RxCallback(Ptr<const Packet> packet, const Address &address, const Address &srcAddress)
{
    // This will be handled in the DelayTracer below for delays
}

void DelayTracer(Ptr<Packet> packet, const Address &address)
{
    // Retrieve packet's send timestamp (set using PacketTag)
    struct TimestampTag : public Tag
    {
        Time m_time;
        static TypeId GetTypeId()
        {
            static TypeId tid = TypeId("TimestampTag")
                                    .SetParent<Tag>()
                                    .AddConstructor<TimestampTag>();
            return tid;
        }
        TypeId GetInstanceTypeId() const override
        {
            return GetTypeId();
        }
        uint32_t GetSerializedSize() const override
        {
            return 8;
        }
        void Serialize(TagBuffer i) const override
        {
            i.WriteU64(m_time.GetNanoSeconds());
        }
        void Deserialize(TagBuffer i) override
        {
            m_time = NanoSeconds(i.ReadU64());
        }
        void Print(std::ostream &os) const override
        {
            os << "t=" << m_time;
        }
    } tag;
    if(packet->PeekPacketTag(tag))
    {
        double delay = (Simulator::Now() - tag.m_time).GetMilliSeconds();
        g_delays.push_back(delay);
    }
    ++g_rxPackets;
}

void LostCallback(Ptr<const Packet> packet)
{
    ++g_packetsLost;
}

class PeriodicSender : public Application
{
public:
    PeriodicSender() {}
    virtual ~PeriodicSender() {}

    void Setup(std::vector<Ipv4Address> dests, uint16_t port, Time interval)
    {
        m_dests = dests;
        m_port = port;
        m_interval = interval;
    }

protected:
    virtual void StartApplication()
    {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        m_sendEvent = Simulator::Schedule(Seconds(0.1), &PeriodicSender::SendPacket, this);
    }
    virtual void StopApplication()
    {
        if(m_sendEvent.IsRunning())
        {
            Simulator::Cancel(m_sendEvent);
        }
        if(m_socket)
        {
            m_socket->Close();
        }
    }
    void SendPacket()
    {
        // Send a UDP packet to all other vehicles
        Ptr<Packet> pkt = Create<Packet>(100); // arbitrary 100 bytes

        struct TimestampTag : public Tag
        {
            Time m_time;
            static TypeId GetTypeId()
            {
                static TypeId tid = TypeId("TimestampTag")
                                        .SetParent<Tag>()
                                        .AddConstructor<TimestampTag>();
                return tid;
            }
            TypeId GetInstanceTypeId() const override
            {
                return GetTypeId();
            }
            uint32_t GetSerializedSize() const override
            {
                return 8;
            }
            void Serialize(TagBuffer i) const override
            {
                i.WriteU64(m_time.GetNanoSeconds());
            }
            void Deserialize(TagBuffer i) override
            {
                m_time = NanoSeconds(i.ReadU64());
            }
            void Print(std::ostream &os) const override
            {
                os << "t=" << m_time;
            }
        } tag;
        tag.m_time = Simulator::Now();
        pkt->AddPacketTag(tag);
        for(const auto &addr : m_dests)
        {
            m_socket->SendTo(pkt->Copy(), 0, InetSocketAddress(addr, m_port));
        }
        m_sendEvent = Simulator::Schedule(m_interval, &PeriodicSender::SendPacket, this);
    }

private:
    Ptr<Socket> m_socket;
    std::vector<Ipv4Address> m_dests;
    uint16_t m_port;
    Time m_interval;
    EventId m_sendEvent;
};

int main(int argc, char *argv[])
{
    uint32_t numVehicles = 4;
    double simulationTime = 20.0;
    double distance = 50.0;
    double velocity = 15.0;
    double packetInterval = 0.5;
    uint16_t port = 4000;

    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, vehicles);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(numVehicles),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    for(uint32_t i = 0; i < numVehicles; ++i)
    {
        Ptr<ConstantVelocityMobilityModel> cv = vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel>();
        cv->SetVelocity(Vector(velocity,0,0));
    }

    InternetStackHelper stack;
    stack.Install(vehicles);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    ApplicationContainer sinks;
    for(uint32_t i = 0; i < numVehicles; ++i)
    {
        Ptr<Socket> recvSocket = Socket::CreateSocket(vehicles.Get(i), UdpSocketFactory::GetTypeId());
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
        recvSocket->Bind(local);
        recvSocket->SetRecvCallback(MakeCallback(&RxCallback));
        recvSocket->SetRecvPktInfo(false);
        recvSocket->TraceConnectWithoutContext("Recv", MakeCallback(&DelayTracer));
        recvSocket->SetAllowBroadcast(true);
        // Packet loss calculation is with WifiPhy-level stats
    }

    for(uint32_t i = 0; i < numVehicles; ++i)
    {
        Ptr<PeriodicSender> sender = CreateObject<PeriodicSender>();
        std::vector<Ipv4Address> dests;
        for(uint32_t j = 0; j < numVehicles; ++j)
        {
            if(i != j)
            {
                dests.push_back(interfaces.GetAddress(j));
            }
        }
        sender->Setup(dests, port, Seconds(packetInterval));
        vehicles.Get(i)->AddApplication(sender);
        sender->SetStartTime(Seconds(1.0));
        sender->SetStopTime(Seconds(simulationTime));
    }

    wifiPhy.EnablePcapAll("vanet-adhoc");

    AnimationInterface anim("vanet-adhoc.xml");
    for(uint32_t i = 0; i < numVehicles; ++i)
    {
        anim.UpdateNodeDescription(vehicles.Get(i), "Vehicle-" + std::to_string(i));
        anim.UpdateNodeColor(vehicles.Get(i), 255, 0, 0);
    }

    // Packet loss tracing
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferRx", MakeCallback(&RxCallback));
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferLost", MakeCallback(&LostCallback));

    Simulator::Stop(Seconds(simulationTime + 1.0));
    Simulator::Run();

    double avgDelay = 0.0;
    if(!g_delays.empty())
    {
        double sum = 0.0;
        for(double d : g_delays) sum += d;
        avgDelay = sum / g_delays.size();
    }
    std::ofstream out("vanet-metrics.txt");
    out << "Total packets received: " << g_rxPackets << std::endl;
    out << "Packets lost: " << g_packetsLost << std::endl;
    out << "Average delay (ms): " << avgDelay << std::endl;
    out.close();

    Simulator::Destroy();
    return 0;
}