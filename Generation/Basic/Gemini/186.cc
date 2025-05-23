#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/udp-socket-factory.h"

namespace ns3
{

// Define a custom application for sending Basic Safety Messages (BSM)
class BsmSender : public Application
{
public:
    static TypeId GetTypeId();
    BsmSender();
    void Setup(Address peer, uint32_t packetSize, Time interval);

protected:
    void DoStart() override;
    void DoStop() override;

private:
    void SendPacket();
    void ScheduleTx();

    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_packetSize;
    Time m_interval;
    EventId m_sendEvent;
};

// BsmSender Class Implementation
TypeId BsmSender::GetTypeId()
{
    static TypeId tid = TypeId("BsmSender")
        .SetParent<Application>()
        .SetGroupName("Applications")
        .AddConstructor<BsmSender>();
    return tid;
}

BsmSender::BsmSender() : m_socket(nullptr), m_packetSize(0), m_interval(Seconds(0))
{
}

void BsmSender::Setup(Address peer, uint32_t packetSize, Time interval)
{
    m_peer = peer;
    m_packetSize = packetSize;
    m_interval = interval;
}

void BsmSender::DoStart()
{
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Bind();

    ScheduleTx();
}

void BsmSender::DoStop()
{
    Simulator::Cancel(m_sendEvent);
    if (m_socket)
    {
        m_socket->Close();
        m_socket = nullptr;
    }
}

void BsmSender::SendPacket()
{
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->SendTo(packet, 0, m_peer);

    ScheduleTx();
}

void BsmSender::ScheduleTx()
{
    if (Simulator::Now() < Simulator::GetStopTime())
    {
        m_sendEvent = Simulator::Schedule(m_interval, &BsmSender::SendPacket, this);
    }
}

} // namespace ns3

// Main simulation function
int main(int argc, char *argv[])
{
    using namespace ns3;

    CommandLine cmd(__FILE__);
    uint32_t nVehicles = 5;
    Time simTime = Seconds(60.0);
    uint32_t packetSize = 100; // bytes (typical BSM size)
    Time bsmInterval = MilliSeconds(100); // 10 Hz

    cmd.AddValue("nVehicles", "Number of vehicles", nVehicles);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime.GetSeconds());
    cmd.AddValue("packetSize", "Size of BSM packets in bytes", packetSize);
    cmd.AddValue("bsmInterval", "Interval between BSM transmissions in milliseconds", bsmInterval.GetMilliSeconds());
    cmd.Parse(argc, argv);

    // Create nodes (vehicles)
    NodeContainer vehicles;
    vehicles.Create(nVehicles);

    // Install mobility model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    
    // Set initial positions for vehicles
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < nVehicles; ++i)
    {
        positionAlloc->Add(Vector(0.0 + i * 30.0, 0.0, 0.0)); // Place vehicles 30m apart
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(vehicles);

    // Set initial velocities for vehicles (moving in positive X direction)
    for (uint32_t i = 0; i < nVehicles; ++i)
    {
        Ptr<ConstantVelocityMobilityModel> mob = vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel>();
        mob->SetVelocity(Vector(10.0 + i * 0.5, 0.0, 0.0)); // Different speeds (e.g., 10 m/s to 12 m/s)
    }

    // Configure 802.11p (WAVE) Wifi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211p);

    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationLossModel("ns3::FriisPropagationLossModel");
    wifiChannel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    Ptr<YansWifiChannel> channel = wifiChannel.Create();
    wifiPhy.SetChannel(channel);

    wifiPhy.Set("TxPowerStart", DoubleValue(20)); // 20 dBm (100 mW)
    wifiPhy.Set("TxPowerEnd", DoubleValue(20));
    wifiPhy.Set("TxGain", DoubleValue(0));
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-90));
    wifiPhy.Set("CcaMode1Threshold", DoubleValue(-90));

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac"); // Ad-hoc mode for V2V communication

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, vehicles);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(vehicles);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Install BSM Sender applications on each vehicle
    ApplicationContainer bsmApps;
    uint16_t bsmPort = 9999;
    Ipv4Address broadcastAddress = Ipv4Address("10.0.0.255"); // Subnet broadcast address

    for (uint32_t i = 0; i < nVehicles; ++i)
    {
        Ptr<ns3::BsmSender> bsmSender = CreateObject<ns3::BsmSender>();
        bsmSender->Setup(InetSocketAddress(broadcastAddress, bsmPort), packetSize, bsmInterval);
        vehicles.Get(i)->AddApplication(bsmSender);
        bsmSender->SetStartTime(Seconds(1.0)); // Start sending after 1 second
        bsmSender->SetStopTime(simTime);
        bsmApps.Add(bsmSender);
    }

    // Enable NetAnim visualization
    NetAnimHelper netanim;
    netanim.EnableAnimationFile("vanet-simulation.xml");

    // Configure simulation stop time
    Simulator::Stop(simTime);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}