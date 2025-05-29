#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANET80211pSimulation");

uint32_t packetsSent = 0;
uint32_t packetsReceived = 0;
double totalDelay = 0.0;

void RxCallback(Ptr<Socket> socket)
{
    while (Ptr<Packet> pkt = socket->Recv())
    {
        packetsReceived++;
        Time now = Simulator::Now();
        uint64_t senderTimestamp = 0;
        pkt->CopyData(reinterpret_cast<uint8_t*>(&senderTimestamp), sizeof(uint64_t));
        Time sent = TimeStep(senderTimestamp);
        totalDelay += (now - sent).GetSeconds();
    }
}

class UdpV2VApp : public Application
{
public:
    UdpV2VApp() {}
    virtual ~UdpV2VApp() {}
    void Setup(Address address, uint16_t port, uint32_t packetSize, double interval, uint32_t nPackets)
    {
        m_destAddress = address;
        m_destPort = port;
        m_packetSize = packetSize;
        m_interval = Seconds(interval);
        m_nPackets = nPackets;
    }

private:
    virtual void StartApplication()
    {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        m_socket->Connect(InetSocketAddress(Ipv4Address::ConvertFrom(m_destAddress), m_destPort));
        m_sendEvent = Simulator::Schedule(Seconds(1.0), &UdpV2VApp::SendPacket, this);
    }

    virtual void StopApplication()
    {
        if (m_socket)
        {
            m_socket->Close();
        }
        Simulator::Cancel(m_sendEvent);
    }

    void SendPacket()
    {
        if (m_packetsSent < m_nPackets)
        {
            ++m_packetsSent;
            packetsSent++;

            uint8_t *buf = new uint8_t[m_packetSize];
            uint64_t t = Simulator::Now().GetTimeStep();
            memcpy(buf, &t, std::min(m_packetSize,sizeof(uint64_t)));
            Ptr<Packet> packet = Create<Packet>(buf, m_packetSize);
            m_socket->Send(packet);
            delete[] buf;
            m_sendEvent = Simulator::Schedule(m_interval, &UdpV2VApp::SendPacket, this);
        }
    }

    Ptr<Socket> m_socket;
    Address m_destAddress;
    uint16_t m_destPort;
    uint32_t m_packetSize;
    uint32_t m_nPackets;
    uint32_t m_packetsSent = 0;
    EventId m_sendEvent;
    Time m_interval;
};

int main(int argc, char *argv[])
{
    uint32_t numVehicles = 5;
    double simDuration = 30.0;
    double distance = 50.0; // meters between vehicles
    uint32_t packetSize = 200;
    double appInterval = 1.0;
    uint16_t port = 9000;
    uint32_t packetsPerClient = uint32_t(simDuration / appInterval);

    LogComponentEnable("VANET80211pSimulation", LOG_LEVEL_INFO);

    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    NqosWaveMacHelper mac = NqosWaveMacHelper::Default();
    Wifi80211pHelper waveHelper = Wifi80211pHelper::Default();

    NetDeviceContainer devices = waveHelper.Install(wifiPhy, mac, vehicles);

    InternetStackHelper internet;
    internet.Install(vehicles);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");

    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < numVehicles; ++i)
        posAlloc->Add(Vector(i * distance, 0.0, 0.0));
    mobility.SetPositionAllocator(posAlloc);

    mobility.Install(vehicles);

    // Assign different speeds
    std::vector<double> speeds = {13.0, 15.0, 12.5, 14.2, 16.0}; // m/s
    for (uint32_t i = 0; i < numVehicles; ++i)
    {
        Ptr<ConstantVelocityMobilityModel> mob = vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel>();
        mob->SetVelocity(Vector(speeds[i], 0.0, 0.0));
    }

    // Setup UDP server on the first vehicle
    uint32_t serverIdx = 0;
    Ptr<Socket> recvSink = Socket::CreateSocket(vehicles.Get(serverIdx), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
    recvSink->Bind(local);
    recvSink->SetRecvCallback(MakeCallback(&RxCallback));

    // Setup UDP clients on other vehicles
    for (uint32_t i = 0; i < numVehicles; ++i)
    {
        if (i == serverIdx)
            continue;

        Ptr<UdpV2VApp> app = CreateObject<UdpV2VApp>();
        app->Setup(interfaces.GetAddress(serverIdx), port, packetSize, appInterval, packetsPerClient);
        vehicles.Get(i)->AddApplication(app);
        app->SetStartTime(Seconds(1.0));
        app->SetStopTime(Seconds(simDuration));
    }

    AnimationInterface anim("vanet80211p.xml");
    for (uint32_t i = 0; i < numVehicles; ++i)
    {
        anim.UpdateNodeDescription(vehicles.Get(i), "Vehicle" + std::to_string(i));
        anim.UpdateNodeColor(vehicles.Get(i), 255, 0, 0);
    }
    anim.EnablePacketMetadata(true);

    Simulator::Stop(Seconds(simDuration + 1));
    Simulator::Run();

    double pdr = (packetsSent > 0) ? (double(packetsReceived) / packetsSent * 100.0) : 0.0;
    double avgDelay = (packetsReceived > 0) ? (totalDelay / packetsReceived) : 0.0;

    std::cout << "Packets Sent: " << packetsSent << std::endl;
    std::cout << "Packets Received: " << packetsReceived << std::endl;
    std::cout << "Packet Delivery Ratio (%): " << pdr << std::endl;
    std::cout << "Average End-to-End Delay (s): " << avgDelay << std::endl;

    Simulator::Destroy();
    return 0;
}