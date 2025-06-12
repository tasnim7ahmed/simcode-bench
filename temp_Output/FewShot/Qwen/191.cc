#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/energy-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessSensorNetwork");

class SensorApplication : public Application {
public:
    static TypeId GetTypeId(void) {
        static TypeId tid = TypeId("SensorApplication")
            .SetParent<Application>()
            .AddConstructor<SensorApplication>()
            .AddAttribute("Destination", "The sink node's IPv4 address",
                          Ipv4AddressValue(),
                          MakeIpv4AddressAccessor(&SensorApplication::m_sinkAddress),
                          MakeIpv4AddressChecker())
            .AddAttribute("PacketSize", "Size of packets generated (bytes)",
                          UintegerValue(128),
                          MakeUintegerAccessor(&SensorApplication::m_packetSize),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("Interval", "Time between packet sends",
                          TimeValue(Seconds(5)),
                          MakeTimeAccessor(&SensorApplication::m_interval),
                          MakeTimeChecker());
        return tid;
    }

    SensorApplication() {}
    virtual ~SensorApplication() {}

protected:
    void StartApplication(void) override {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        m_socket->Bind();
        ScheduleTransmit(Seconds(0));
    }

    void StopApplication(void) override {
        if (m_sendEvent.IsRunning()) {
            Simulator::Cancel(m_sendEvent);
        }
        if (m_socket) {
            m_socket->Close();
        }
    }

private:
    void ScheduleTransmit(Time dt) {
        m_sendEvent = Simulator::Schedule(dt, &SensorApplication::SendPacket, this);
    }

    void SendPacket(void) {
        Ptr<Packet> packet = Create<Packet>(m_packetSize);
        if (m_socket->SendTo(packet, 0, InetSocketAddress(m_sinkAddress, 9)) >= 0) {
            NS_LOG_INFO("Sent packet at time " << Simulator::Now().As(Time::S));
        }

        ScheduleTransmit(m_interval);
    }

    Ipv4Address m_sinkAddress;
    uint32_t m_packetSize;
    Time m_interval;
    EventId m_sendEvent;
    Ptr<Socket> m_socket;
};

int main(int argc, char *argv[]) {
    LogComponentEnable("SensorApplication", LOG_LEVEL_INFO);
    LogComponentEnable("DeviceEnergyModel", LOG_LEVEL_INFO);

    // Simulation parameters
    uint32_t gridSize = 5;
    double areaSize = 100.0;  // meters
    double txPower = 10.0;    // dBm

    // Create nodes in a grid
    NodeContainer nodes;
    nodes.Create(gridSize * gridSize + 1);  // +1 for sink node

    // Install mobility models
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(areaSize / gridSize),
                                  "DeltaY", DoubleValue(areaSize / gridSize),
                                  "GridWidth", UintegerValue(gridSize),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Set sink node position to center
    Ptr<Node> sinkNode = nodes.Get(gridSize * gridSize);
    Ptr<MobilityModel> sinkMobility = sinkNode->GetObject<MobilityModel>();
    sinkMobility->SetPosition(Vector(areaSize / 2, areaSize / 2, 0));

    // WiFi configuration
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Energy model setup
    BasicEnergySourceHelper energySource;
    energySource.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(10000));  // 10 kJ initial energy
    energySource.Set("BasicEnergySourceSupplyVoltageV", DoubleValue(3.0));

    WifiRadioEnergyModelHelper radioEnergyHelper;
    radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.350));  // Transmission current in Amps
    radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.197));  // Reception current in Amps

    EnergySourceContainer sources = energySource.Install(nodes);
    radioEnergyHelper.Install(devices, sources);

    // Install applications on sensor nodes
    uint16_t port = 9;
    for (uint32_t i = 0; i < gridSize * gridSize; ++i) {
        Ptr<Node> node = nodes.Get(i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        Ipv4InterfaceAddress iaddr = ipv4->GetInterface(1)->GetAddress(0);
        Ipv4Address localAddr = iaddr.GetLocal();

        Ipv4Address sinkAddr = interfaces.GetAddress(gridSize * gridSize);

        NS_LOG_INFO("Installing application from " << localAddr << " to sink at " << sinkAddr);

        SensorApplicationHelper appHelper;
        appHelper.SetAttribute("Destination", Ipv4AddressValue(sinkAddr));
        appHelper.SetAttribute("PacketSize", UintegerValue(100));
        appHelper.SetAttribute("Interval", TimeValue(Seconds(5)));

        ApplicationContainer app = appHelper.Install(node);
        app.Start(Seconds(1.0));
        app.Stop(Seconds(1000.0));
    }

    // Sink application - receive packets
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(sinkNode);
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(1000.0));

    // Stop simulation when all nodes run out of energy
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/PowerLimited",
                    MakeCallback([](std::string path, Ptr<const Packet> p) {
                        NS_LOG_INFO("Node at " << path << " ran out of energy at " << Simulator::Now().As(Time::S));
                        static int powerLimitedCount = 0;
                        powerLimitedCount++;
                        if (powerLimitedCount == gridSize * gridSize + 1) {
                            Simulator::Stop();
                        }
                    }));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}