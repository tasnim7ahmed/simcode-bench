#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include <fstream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANETSimulation");

class UdpPacketSinkWithStats : public Application
{
public:
    UdpPacketSinkWithStats();
    virtual ~UdpPacketSinkWithStats();
    void Setup(Address address, uint16_t port);

    uint32_t GetTotalRx() const { return m_totalRx; }
    uint32_t GetPacketCount() const { return m_count; }
    double GetAverageDelay() const;

protected:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    void HandleRead(Ptr<Socket> socket);

private:
    Ptr<Socket>     m_socket;
    Address         m_local;
    uint16_t        m_port;
    uint32_t        m_totalRx;
    uint32_t        m_count;
    std::vector<double> m_delays;
    bool            m_running;
};

UdpPacketSinkWithStats::UdpPacketSinkWithStats()
    : m_socket(0), m_totalRx(0), m_count(0), m_port(0), m_running(false)
{
}

UdpPacketSinkWithStats::~UdpPacketSinkWithStats()
{
    m_socket = 0;
}

void
UdpPacketSinkWithStats::Setup(Address address, uint16_t port)
{
    m_local = address;
    m_port = port;
}

void
UdpPacketSinkWithStats::StartApplication(void)
{
    m_running = true;
    if (!m_socket)
    {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
        m_socket->Bind(local);
    }
    m_socket->SetRecvCallback(MakeCallback(&UdpPacketSinkWithStats::HandleRead, this));
}

void
UdpPacketSinkWithStats::StopApplication(void)
{
    m_running = false;
    if (m_socket)
    {
        m_socket->Close();
    }
}

void
UdpPacketSinkWithStats::HandleRead(Ptr<Socket> socket)
{
    while (Ptr<Packet> packet = socket->Recv())
    {
        m_totalRx += packet->GetSize();
        m_count++;
        Time sendTs;
        if (packet->PeekPacketTag(sendTs))
        {
            double delay = (Simulator::Now() - sendTs).GetSeconds();
            m_delays.push_back(delay);
        }
    }
}

double
UdpPacketSinkWithStats::GetAverageDelay() const
{
    if (m_delays.empty())
        return 0.0;
    double sum = 0.0;
    for (double d : m_delays)
        sum += d;
    return sum / m_delays.size();
}

class UdpStampedClient : public Application
{
public:
    UdpStampedClient();
    virtual ~UdpStampedClient();
    void Setup(Address address, uint16_t port, uint32_t packetSize, uint32_t nPackets, Time interval);

protected:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    void ScheduleTx();
    void SendPacket();

private:
    Ptr<Socket>     m_socket;
    Address         m_peer;
    uint16_t        m_port;
    uint32_t        m_packetSize;
    uint32_t        m_nPackets;
    Time            m_interval;
    uint32_t        m_sent;
    EventId         m_sendEvent;
    bool            m_running;
};

UdpStampedClient::UdpStampedClient()
    : m_socket(0), m_port(0), m_packetSize(1024), m_nPackets(1), m_interval(Seconds(1.0)), m_sent(0), m_running(false)
{}

UdpStampedClient::~UdpStampedClient()
{
    m_socket = 0;
}

void
UdpStampedClient::Setup(Address address, uint16_t port, uint32_t packetSize, uint32_t nPackets, Time interval)
{
    m_peer = address;
    m_port = port;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_interval = interval;
}

void
UdpStampedClient::StartApplication(void)
{
    m_running = true;
    m_sent = 0;
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Connect(InetSocketAddress(InetSocketAddress::ConvertFrom(m_peer).GetIpv4(), m_port));
    SendPacket();
}

void
UdpStampedClient::StopApplication(void)
{
    m_running = false;
    if (m_sendEvent.IsRunning())
        Simulator::Cancel(m_sendEvent);
    if (m_socket)
        m_socket->Close();
}

void
UdpStampedClient::SendPacket()
{
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    Time sendTs = Simulator::Now();
    packet->AddPacketTag(sendTs);
    m_socket->Send(packet);

    ++m_sent;
    if (m_sent < m_nPackets)
        ScheduleTx();
}

void
UdpStampedClient::ScheduleTx()
{
    if (m_running)
    {
        m_sendEvent = Simulator::Schedule(m_interval, &UdpStampedClient::SendPacket, this);
    }
}

static void CourseChange(std::string context, Ptr<const MobilityModel> model)
{
    Vector pos = model->GetPosition();
    Vector vel = model->GetVelocity();
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: " << context <<
        " Position: (" << pos.x << ", " << pos.y << ", " << pos.z << ") " <<
        " Velocity: (" << vel.x << ", " << vel.y << ", " << vel.z << ")");
}

int main(int argc, char *argv[])
{
    LogComponentEnable("VANETSimulation", LOG_LEVEL_INFO);

    uint32_t numVehicles = 5;
    double simTime = 20.0;
    uint16_t udpPort = 4000;
    double roadLength = 500.0; // meters
    double laneWidth = 5.0;
    double startY = 20.0;
    Time packetInterval = Seconds(1.0);
    uint32_t packetSize = 256;
    uint32_t maxPackets = uint32_t(simTime / packetInterval.GetSeconds()) + 2;

    // Create nodes
    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    // Mobility: place cars on a straight road with random speeds
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(startY),
        "DeltaX", DoubleValue(40.0),
        "DeltaY", DoubleValue(0.0),
        "GridWidth", UintegerValue(numVehicles),
        "LayoutType", StringValue("RowFirst")
    );
    mobility.Install(vehicles);

    Ptr<UniformRandomVariable> speedRand = CreateObject<UniformRandomVariable>();
    speedRand->SetAttribute("Min", DoubleValue(15.0)); // m/s
    speedRand->SetAttribute("Max", DoubleValue(25.0));

    for (uint32_t i = 0; i < vehicles.GetN(); ++i)
    {
        Ptr<ConstantVelocityMobilityModel> mob = vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel>();
        double speed = speedRand->GetValue();
        mob->SetVelocity(Vector(speed,0,0));
    }

    Config::Connect("/NodeList/*/$ns3::MobilityModel/CourseChange", MakeCallback(&CourseChange));

    // Wi-Fi 802.11p (WAVE)
    YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default();
    wavePhy.SetChannel(waveChannel.Create());

    NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
    Wifi80211pHelper waveHelper = Wifi80211pHelper::Default();

    NetDeviceContainer devices = waveHelper.Install(wavePhy, waveMac, vehicles);

    // Internet stack
    InternetStackHelper internet;
    internet.Install(vehicles);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // UDP server App on the first vehicle
    Ptr<UdpPacketSinkWithStats> sinkApp = CreateObject<UdpPacketSinkWithStats>();
    sinkApp->Setup(interfaces.GetAddress(0), udpPort);
    vehicles.Get(0)->AddApplication(sinkApp);
    sinkApp->SetStartTime(Seconds(1.0));
    sinkApp->SetStopTime(Seconds(simTime));

    // UDP clients on all others
    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < vehicles.GetN(); ++i)
    {
        Ptr<UdpStampedClient> client = CreateObject<UdpStampedClient>();
        client->Setup(interfaces.GetAddress(0), udpPort, packetSize, maxPackets, packetInterval);
        vehicles.Get(i)->AddApplication(client);
        client->SetStartTime(Seconds(2.0));
        client->SetStopTime(Seconds(simTime));
        clientApps.Add(client);
    }

    // NetAnim setup for visualization
    AnimationInterface anim("vanet_anim.xml");
    for (uint32_t i = 0; i < numVehicles; ++i)
    {
        anim.UpdateNodeDescription(vehicles.Get(i), "Vehicle " + std::to_string(i));
        anim.UpdateNodeColor(vehicles.Get(i), 255, 0, 0); // Red
    }
    anim.SetMobilityPollInterval(Seconds(0.5));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Metrics: PDR and average delay
    uint32_t totalRx = sinkApp->GetPacketCount();
    uint32_t totalTx = (numVehicles - 1) * maxPackets;
    double pdr = totalRx * 100.0 / totalTx;

    double avgDelay = sinkApp->GetAverageDelay();

    std::cout << "===== VANET Metrics =====" << std::endl;
    std::cout << "Packets sent:   " << totalTx << std::endl;
    std::cout << "Packets received: " << totalRx << std::endl;
    std::cout << "Packet Delivery Ratio: " << pdr << " %" << std::endl;
    std::cout << "Average end-to-end delay: " << avgDelay << " s" << std::endl;

    Simulator::Destroy();
    return 0;
}