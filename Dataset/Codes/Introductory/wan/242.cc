#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SixLowpanIoTExample");

void ReceivePacket(Ptr<Socket> socket) {
    while (socket->Recv()) {
        NS_LOG_INFO("IoT Server received a UDP packet from an IoT device!");
    }
}

int main() {
    LogComponentEnable("SixLowpanIoTExample", LOG_LEVEL_INFO);

    uint16_t port = 5000;

    // Create nodes (IoT Devices + Server)
    NodeContainer nodes;
    nodes.Create(4);  // 3 IoT devices + 1 server

    Ptr<Node> server = nodes.Get(3);

    // Setup IEEE 802.15.4 (Low-Power Wireless)
    LrWpanHelper lrWpan;
    NetDeviceContainer lrDevices = lrWpan.Install(nodes);

    // Enable 6LoWPAN over IEEE 802.15.4
    SixLowPanHelper sixlowpan;
    NetDeviceContainer sixlowpanDevices = sixlowpan.Install(lrDevices);

    // Mobility model (static nodes)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(sixlowpanDevices);

    // Setup UDP Receiver (IoT Server)
    Ptr<Socket> serverSocket = Socket::CreateSocket(server, UdpSocketFactory::GetTypeId());
    serverSocket->Bind(Inet6SocketAddress(Ipv6Address::GetAny(), port));
    serverSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

    // Setup UDP Senders (IoT Devices)
    for (uint32_t i = 0; i < 3; i++) {
        Ptr<Socket> deviceSocket = Socket::CreateSocket(nodes.Get(i), UdpSocketFactory::GetTypeId());
        Inet6SocketAddress serverAddress(interfaces.GetAddress(3, 1), port);

        Simulator::Schedule(Seconds(1.0 + i), &Socket::SendTo, deviceSocket, Create<Packet>(64), 0, serverAddress);
        Simulator::Schedule(Seconds(3.0 + i), &Socket::SendTo, deviceSocket, Create<Packet>(64), 0, serverAddress);
    }

    // Run the simulation
    Simulator::Stop(Seconds(6.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

