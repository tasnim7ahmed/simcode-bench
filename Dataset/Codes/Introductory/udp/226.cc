#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/energy-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpWSNExample");

// Function to send UDP packets from sensors to the sink
void SendPacket(Ptr<Socket> socket, Ipv4Address sinkAddr, uint16_t port) {
    Ptr<Packet> packet = Create<Packet>(256); // 256-byte packet
    socket->SendTo(packet, 0, InetSocketAddress(sinkAddr, port));
    Simulator::Schedule(Seconds(2.0), &SendPacket, socket, sinkAddr, port); // Schedule next packet
}

// Function to receive packets at the sink node
void ReceivePacket(Ptr<Socket> socket) {
    while (socket->Recv()) {
        NS_LOG_INFO("Sink node received a packet from a sensor!");
    }
}

int main(int argc, char *argv[]) {
    LogComponentEnable("UdpWSNExample", LOG_LEVEL_INFO);

    int numSensors = 20;
    double simTime = 50.0;

    // Create nodes (sensor nodes + 1 sink)
    NodeContainer sensors;
    sensors.Create(numSensors);

    NodeContainer sink;
    sink.Create(1);

    // Configure Wi-Fi (Ad-Hoc Mode for WSN)
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b); // 802.11b for low-power sensor communication

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer sensorDevices = wifi.Install(phy, mac, sensors);
    NetDeviceContainer sinkDevice = wifi.Install(phy, mac, sink);

    // Set up mobility (random sensor deployment)
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    
    for (int i = 0; i < numSensors; i++) {
        positionAlloc->Add(Vector(rand() % 100, rand() % 100, 0.0)); // Random deployment in 100x100m area
    }
    positionAlloc->Add(Vector(50.0, 50.0, 0.0)); // Sink at the center

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(sensors);
    mobility.Install(sink);

    // Install Energy Model
    BasicEnergySourceHelper energySourceHelper;
    energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(10.0)); // 10 Joules per node
    EnergySourceContainer energySources = energySourceHelper.Install(sensors);

    WifiRadioEnergyModelHelper radioEnergyHelper;
    radioEnergyHelper.Install(sensorDevices, energySources);

    // Install Internet stack with OLSR routing
    OlsrHelper olsr;
    InternetStackHelper internet;
    internet.SetRoutingHelper(olsr);
    internet.Install(sensors);
    internet.Install(sink);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer sensorInterfaces = address.Assign(sensorDevices);
    Ipv4InterfaceContainer sinkInterface = address.Assign(sinkDevice);

    uint16_t port = 9090; // UDP port

    // Setup UDP Server at Sink Node
    Ptr<Socket> sinkSocket = Socket::CreateSocket(sink.Get(0), UdpSocketFactory::GetTypeId());
    sinkSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
    sinkSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

    // Setup UDP Clients in Sensor Nodes
    for (int i = 0; i < numSensors; i++) {
        Ptr<Socket> sensorSocket = Socket::CreateSocket(sensors.Get(i), UdpSocketFactory::GetTypeId());
        Simulator::Schedule(Seconds(2.0 + i * 0.5), &SendPacket, sensorSocket, sinkInterface.GetAddress(0), port);
    }

    // Run the simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

