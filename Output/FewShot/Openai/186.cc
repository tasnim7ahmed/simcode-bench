#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetWaveSimulation");

class BsmApp : public Application
{
public:
    BsmApp() {}
    virtual ~BsmApp() {}

    void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, Time interval)
    {
        m_socket = socket;
        m_peer = address;
        m_packetSize = packetSize;
        m_interval = interval;
    }

private:
    virtual void StartApplication(void)
    {
        m_running = true;
        m_socket->Bind();
        m_socket->Connect(m_peer);
        ScheduleTx();
    }

    virtual void StopApplication(void)
    {
        m_running = false;
        if (m_sendEvent.IsRunning())
        {
            Simulator::Cancel(m_sendEvent);
        }
        if (m_socket)
        {
            m_socket->Close();
        }
    }

    void ScheduleTx()
    {
        if (m_running)
        {
            m_sendEvent = Simulator::Schedule(m_interval, &BsmApp::SendPacket, this);
        }
    }

    void SendPacket()
    {
        Ptr<Packet> packet = Create<Packet>(m_packetSize);
        m_socket->Send(packet);
        ScheduleTx();
    }

    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_packetSize;
    Time m_interval;
    EventId m_sendEvent;
    bool m_running{false};
};

void ReceivePacket(Ptr<Socket> socket)
{
    while (Ptr<Packet> packet = socket->Recv())
    {
        NS_LOG_INFO("Received BSM of size " << packet->GetSize() << " bytes at "
                                            << Simulator::Now().GetSeconds() << "s");
    }
}

int main(int argc, char *argv[])
{
    LogComponentEnable("VanetWaveSimulation", LOG_LEVEL_INFO);

    // Simulation parameters
    uint32_t numVehicles = 5;
    double simTime = 20.0;
    double distance = 30.0; // meters between vehicles
    double speed = 15.0; // m/s

    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    // Install Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(distance),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(numVehicles),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.Install(vehicles);
    for (uint32_t i = 0; i < numVehicles; ++i)
    {
        Ptr<ConstantVelocityMobilityModel> cv = vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel>();
        cv->SetVelocity(Vector(speed, 0, 0));
    }

    // Install WAVE (802.11p)
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    NqosWaveMacHelper wifi80211pMac = NqosWaveMacHelper::Default();
    WaveHelper waveHelper = WaveHelper::Default();
    NetDeviceContainer devices = waveHelper.Install(wifiPhy, wifi80211pMac, vehicles);

    // Install Internet stack for IP communication
    InternetStackHelper internet;
    internet.Install(vehicles);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Install BSM App on all vehicles, each broadcasts to all
    uint16_t bsmPort = 3000;
    uint32_t bsmSize = 200; // bytes
    Time bsmInterval = Seconds(1.0); // BSM every second

    std::vector<Ptr<BsmApp>> apps(numVehicles);

    for (uint32_t i = 0; i < numVehicles; ++i)
    {
        // Install receiver on each node
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        Ptr<Socket> recvSocket = Socket::CreateSocket(vehicles.Get(i), tid);
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), bsmPort);
        recvSocket->Bind(local);
        recvSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

        // Each sender sends to broadcast address
        Ptr<Socket> sendSocket = Socket::CreateSocket(vehicles.Get(i), tid);
        InetSocketAddress broadcastAddr = InetSocketAddress(Ipv4Address("255.255.255.255"), bsmPort);
        sendSocket->SetAllowBroadcast(true);

        Ptr<BsmApp> app = CreateObject<BsmApp>();
        app->Setup(sendSocket, broadcastAddr, bsmSize, bsmInterval);
        vehicles.Get(i)->AddApplication(app);
        app->SetStartTime(Seconds(1.0));
        app->SetStopTime(Seconds(simTime));
        apps[i] = app;
    }

    // NetAnim setup
    AnimationInterface anim("vanet-wave.xml");
    anim.SetMaxPktsPerTraceFile(2000000);
    for (uint32_t i = 0; i < numVehicles; ++i)
    {
        anim.UpdateNodeDescription(vehicles.Get(i), "Vehicle" + std::to_string(i+1));
        anim.UpdateNodeColor(vehicles.Get(i), 0, 0, 255); // Blue
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}