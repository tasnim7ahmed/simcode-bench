#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lorawan-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LoRaSensorExample");

// Function to log received packets
void ReceivePacket(Ptr<Socket> socket) {
    while (socket->Recv()) {
        NS_LOG_INFO("Sink received data from sensor!");
    }
}

int main() {
    LogComponentEnable("LoRaSensorExample", LOG_LEVEL_INFO);

    uint16_t port = 6000;

    // Create nodes (sensor and sink)
    NodeContainer nodes;
    nodes.Create(2);
    Ptr<Node> sensor = nodes.Get(0);
    Ptr<Node> sink = nodes.Get(1);

    // LoRaWAN setup
    LoraHelper lora;
    LoraPhyHelper phy = LoraPhyHelper::Default();
    LorawanMacHelper mac;
    LorawanHelper lorawan;
    
    NetDeviceContainer devices = lorawan.Install(lora, phy, mac, nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup UDP Sink (Receiver)
    Ptr<Socket> sinkSocket = Socket::CreateSocket(sink, UdpSocketFactory::GetTypeId());
    sinkSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
    sinkSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

    // Setup UDP Sensor (Sender)
    Ptr<Socket> sensorSocket = Socket::CreateSocket(sensor, UdpSocketFactory::GetTypeId());
    InetSocketAddress sinkAddress(interfaces.GetAddress(1), port);

    // Send periodic packets (every 5 seconds)
    for (double time = 1.0; time <= 10.0; time += 5.0) {
        Simulator::Schedule(Seconds(time), &Socket::SendTo, sensorSocket, Create<Packet>(64), 0, sinkAddress);
    }

    // Run the simulation
    Simulator::Stop(Seconds(12.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

