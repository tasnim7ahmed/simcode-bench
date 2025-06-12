#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANET_80211p_Example");

void ReceivePacket(Ptr<Socket> socket) {
    while (socket->Recv()) {
        NS_LOG_INFO("RSU received a UDP packet from the vehicle!");
    }
}

int main() {
    LogComponentEnable("VANET_80211p_Example", LOG_LEVEL_INFO);

    uint16_t port = 4000;

    // Create nodes (1 Vehicle, 1 RSU)
    NodeContainer nodes;
    nodes.Create(2);
    Ptr<Node> vehicle = nodes.Get(0);
    Ptr<Node> rsu = nodes.Get(1);

    // Setup 802.11p (WAVE) for VANET communication
    YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default();
    wavePhy.SetChannel(waveChannel.Create());

    QosWaveMacHelper waveMac = QosWaveMacHelper::Default();
    WaveHelper wave;
    NetDeviceContainer devices = wave.Install(wavePhy, waveMac, nodes);

    // Mobility model (Vehicle moves, RSU is static)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(rsu);

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicle);
    vehicle->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(10.0, 0.0, 0.0)); // Vehicle moves at 10 m/s

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup UDP Receiver (RSU)
    Ptr<Socket> rsuSocket = Socket::CreateSocket(rsu, UdpSocketFactory::GetTypeId());
    rsuSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
    rsuSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

    // Setup UDP Sender (Vehicle)
    Ptr<Socket> vehicleSocket = Socket::CreateSocket(vehicle, UdpSocketFactory::GetTypeId());
    InetSocketAddress rsuAddress(interfaces.GetAddress(1), port);

    Simulator::Schedule(Seconds(1.0), &Socket::SendTo, vehicleSocket, Create<Packet>(128), 0, rsuAddress);
    Simulator::Schedule(Seconds(2.0), &Socket::SendTo, vehicleSocket, Create<Packet>(128), 0, rsuAddress);

    // Run the simulation
    Simulator::Stop(Seconds(5.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

